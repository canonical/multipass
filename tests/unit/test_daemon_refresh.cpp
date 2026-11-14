/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "common.h"
#include "daemon_test_fixture.h"
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_virtual_machine.h"
#include "mock_virtual_machine_factory.h"
#include "mock_vm_image_vault.h"
#include "temp_dir.h"

#include <daemon/daemon.h>
#include <multipass/build_constants.h>
#include <multipass/constants.h>

#include <algorithm>

#include <QFile>
#include <QJsonArray>
#include <google/protobuf/repeated_field.h>

#include <QJsonDocument>
#include <QJsonObject>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
constexpr auto intent_file_name = "multipassd-refresh-restart-intent.json";

struct PrepareRefreshResult
{
    grpc::Status status;
    mp::PrepareRefreshReply reply;
};

struct AbortRefreshResult
{
    grpc::Status status;
    mp::AbortRefreshReply reply;
};

struct TestDaemonRefresh : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());

        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

        mock_factory = use_a_mock_vm_factory();
        ON_CALL(*mock_factory, get_instance_directory(_)).WillByDefault(Return(QString{}));
    }

    // Plants a stopped instance; returns the mock vm (kept alive by the daemon).
    mpt::MockVirtualMachine* plant_instance(mp::VirtualMachine::State state,
                                            bool expect_start = false)
    {
        auto mock_vm = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(state);
        auto* mock_vm_ptr = mock_vm.get();

        ON_CALL(*mock_vm_ptr, get_name).WillByDefault(ReturnRef(mock_instance_name));
        // State transitions that real backends perform on shutdown/start; the
        // re-scan and gate logic depend on them. fetch_state defaults are
        // flipped from within the actions because the mock's state member is
        // private.
        ON_CALL(*mock_vm_ptr, shutdown).WillByDefault([mock_vm_ptr] {
            ON_CALL(*mock_vm_ptr, fetch_state)
                .WillByDefault(Return(mp::VirtualMachine::State::stopped));
        });
        ON_CALL(*mock_vm_ptr, start).WillByDefault([mock_vm_ptr] {
            ON_CALL(*mock_vm_ptr, fetch_state)
                .WillByDefault(Return(mp::VirtualMachine::State::running));
        });
        if (expect_start)
            EXPECT_CALL(*mock_vm_ptr, start).Times(1);

        mpt::fake_vm_properties props;
        props.name = mock_instance_name;
        props.state = mp::VirtualMachine::State::stopped;
        std::tie(temp_dir, std::ignore) = plant_instance_json(fake_json_contents(props));
        config_builder.data_directory = temp_dir->path();

        EXPECT_CALL(*mock_factory, get_instance_directory(Eq(mock_instance_name)))
            .WillRepeatedly(Return(temp_dir->path()));
        EXPECT_CALL(*mock_factory, create_virtual_machine).WillOnce(Return(std::move(mock_vm)));

        return mock_vm_ptr;
    }

    QString intent_file_path() const
    {
        return temp_dir->filePath(intent_file_name);
    }

    PrepareRefreshResult call_prepare_refresh(mp::Daemon& daemon)
    {
        PrepareRefreshResult result;

        auto reader_writer = StrictMock<
            mpt::MockServerReaderWriter<mp::PrepareRefreshReply, mp::PrepareRefreshRequest>>{};
        EXPECT_CALL(reader_writer, Write)
            .Times(AtMost(1))
            .WillOnce(DoAll(SaveArg<0>(&result.reply), Return(true)));

        mp::PrepareRefreshRequest request;
        result.status = call_daemon_slot(daemon,
                                         &mp::Daemon::prepare_refresh,
                                         request,
                                         reader_writer);

        return result;
    }

    AbortRefreshResult call_abort_refresh(mp::Daemon& daemon)
    {
        AbortRefreshResult result;

        auto reader_writer = StrictMock<
            mpt::MockServerReaderWriter<mp::AbortRefreshReply, mp::AbortRefreshRequest>>{};
        EXPECT_CALL(reader_writer, Write)
            .Times(AtMost(1))
            .WillOnce(DoAll(SaveArg<0>(&result.reply), Return(true)));

        mp::AbortRefreshRequest request;
        result.status = call_daemon_slot(daemon,
                                         &mp::Daemon::abort_refresh,
                                         request,
                                         reader_writer);

        return result;
    }

    QStringList intent_names() const
    {
        QFile file{intent_file_path()};
        if (!file.open(QIODevice::ReadOnly))
            return {};

        const auto doc = QJsonDocument::fromJson(file.readAll());
        QStringList names;
        for (const auto& value : doc.object()["instances"].toArray())
            names.push_back(value.toString());

        return names;
    }

    void write_intent_file(const QStringList& names,
                           const QString& daemon_revision = QStringLiteral("0.0.0"))
    {
        QJsonArray array;
        for (const auto& name : names)
            array.append(name);

        QJsonObject object;
        object["instances"] = array;
        object["daemon_revision"] = daemon_revision;
        object["written_at"] = 0;

        QFile file{intent_file_path()};
        ASSERT_TRUE(file.open(QIODevice::WriteOnly));
        file.write(QJsonDocument{object}.toJson());
    }

    mpt::MockVirtualMachineFactory* mock_factory{nullptr};
    std::unique_ptr<mpt::TempDir> temp_dir;

    const std::string mock_instance_name{"real-zebraphant"};

    mpt::MockPlatform::GuardedMock platform_attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = platform_attr.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};

bool has_name(const google::protobuf::RepeatedPtrField<std::string>& names, const std::string& name)
{
    return std::find(names.cbegin(), names.cend(), name) != names.cend();
}
} // namespace

TEST_F(TestDaemonRefresh, prepareRejectsRefreshWhenInstanceIsSuspended)
{
    plant_instance(mp::VirtualMachine::State::suspended);
    mp::Daemon daemon{config_builder.build()};

    auto result = call_prepare_refresh(daemon);

    ASSERT_TRUE(result.status.ok());
    EXPECT_TRUE(has_name(result.reply.suspended_instances(), mock_instance_name));
    EXPECT_FALSE(QFile::exists(intent_file_path()));
}

TEST_F(TestDaemonRefresh, prepareShutsDownRunningInstanceAndRecordsIntent)
{
    auto* mock_vm = plant_instance(mp::VirtualMachine::State::running);
    EXPECT_CALL(*mock_vm, shutdown);
    mp::Daemon daemon{config_builder.build()};

    auto result = call_prepare_refresh(daemon);

    ASSERT_TRUE(result.status.ok());
    EXPECT_TRUE(has_name(result.reply.stopped_instances(), mock_instance_name));
    EXPECT_THAT(intent_names(), Contains(QString::fromStdString(mock_instance_name)));
}

TEST_F(TestDaemonRefresh, prepareRetainsIntentOfPreviouslyStoppedInstances)
{
    // A previous preparation in this daemon's lifetime shut an instance down;
    // a retry must not lose its restart intent. (An intent surviving a daemon
    // restart is consumed at startup - see startupConsumesRestartIntent.)
    plant_instance(mp::VirtualMachine::State::stopped);
    mp::Daemon daemon{config_builder.build()};

    write_intent_file({QString::fromStdString(mock_instance_name)});

    auto result = call_prepare_refresh(daemon);

    ASSERT_TRUE(result.status.ok());
    EXPECT_TRUE(has_name(result.reply.stopped_instances(), mock_instance_name));
    EXPECT_THAT(intent_names(), Contains(QString::fromStdString(mock_instance_name)));
}

TEST_F(TestDaemonRefresh, prepareRejectsWhenShutdownFails)
{
    auto* mock_vm = plant_instance(mp::VirtualMachine::State::running);
    EXPECT_CALL(*mock_vm, shutdown)
        .WillOnce(Throw(std::runtime_error{"guest ignored the shutdown"}));
    mp::Daemon daemon{config_builder.build()};

    auto result = call_prepare_refresh(daemon);

    ASSERT_TRUE(result.status.ok());
    EXPECT_FALSE(result.reply.error_message().empty());
    EXPECT_FALSE(QFile::exists(intent_file_path()));
}

TEST_F(TestDaemonRefresh, prepareIsIdempotentWhilePrepared)
{
    auto* mock_vm = plant_instance(mp::VirtualMachine::State::running);
    EXPECT_CALL(*mock_vm, shutdown).Times(1); // no additional shutdown on re-run
    mp::Daemon daemon{config_builder.build()};
    const auto first = call_prepare_refresh(daemon);
    ASSERT_TRUE(first.status.ok());
    EXPECT_TRUE(has_name(first.reply.stopped_instances(), mock_instance_name));

    const auto second = call_prepare_refresh(daemon);
    ASSERT_TRUE(second.status.ok());
    EXPECT_TRUE(has_name(second.reply.stopped_instances(), mock_instance_name));
}

TEST_F(TestDaemonRefresh, preparationGateRejectsStateChangingOperations)
{
    plant_instance(mp::VirtualMachine::State::running);
    mp::Daemon daemon{config_builder.build()};

    const auto result = call_prepare_refresh(daemon);
    ASSERT_TRUE(result.status.ok());

    mp::StartRequest start_request;
    start_request.mutable_instance_names()->add_instance_name(mock_instance_name);
    const auto start_status = mpt::DaemonTestFixture::call_daemon_slot(
        daemon,
        &mp::Daemon::start,
        start_request,
        StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>{});
    EXPECT_EQ(start_status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);

    mp::SuspendRequest suspend_request;
    suspend_request.mutable_instance_names()->add_instance_name(mock_instance_name);
    const auto suspend_status = mpt::DaemonTestFixture::call_daemon_slot(
        daemon,
        &mp::Daemon::suspend,
        suspend_request,
        StrictMock<mpt::MockServerReaderWriter<mp::SuspendReply, mp::SuspendRequest>>{});
    EXPECT_EQ(suspend_status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);

    mp::RestartRequest restart_request;
    restart_request.mutable_instance_names()->add_instance_name(mock_instance_name);
    const auto restart_status = mpt::DaemonTestFixture::call_daemon_slot(
        daemon,
        &mp::Daemon::restart,
        restart_request,
        StrictMock<mpt::MockServerReaderWriter<mp::RestartReply, mp::RestartRequest>>{});
    EXPECT_EQ(restart_status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
}

TEST_F(TestDaemonRefresh, abortReleasesGateAndRestartsRecordedInstances)
{
    auto* mock_vm = plant_instance(mp::VirtualMachine::State::running);
    EXPECT_CALL(*mock_vm, shutdown).Times(1);
    EXPECT_CALL(*mock_vm, start).Times(AtMost(2)); // abort's restore + the explicit start below
    mp::Daemon daemon{config_builder.build()};

    const auto prepared = call_prepare_refresh(daemon);
    ASSERT_TRUE(prepared.status.ok());
    EXPECT_THAT(intent_names(), Contains(QString::fromStdString(mock_instance_name)));

    const auto aborted = call_abort_refresh(daemon);
    ASSERT_TRUE(aborted.status.ok());
    EXPECT_FALSE(QFile::exists(intent_file_path()));

    // The gate is released: operations work again. Progress writes are
    // streamed during start, so a nice mock suffices here.
    mp::StartRequest start_request;
    start_request.mutable_instance_names()->add_instance_name(mock_instance_name);
    const auto start_status = mpt::DaemonTestFixture::call_daemon_slot(
        daemon,
        &mp::Daemon::start,
        start_request,
        NiceMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>{});
    EXPECT_TRUE(start_status.ok());
}

TEST_F(TestDaemonRefresh, startupConsumesRestartIntent)
{
    plant_instance(mp::VirtualMachine::State::stopped, /*expect_start=*/true);
    write_intent_file({QString::fromStdString(mock_instance_name)});

    // The intent is consumed at construction time: the instance must be
    // started despite being persisted as stopped, and the file must be
    // retired.
    mp::Daemon daemon{config_builder.build()};

    EXPECT_FALSE(QFile::exists(intent_file_path()));
}

TEST_F(TestDaemonRefresh, startupWithoutIntentLeavesStoppedInstanceAlone)
{
    plant_instance(mp::VirtualMachine::State::stopped);
    mp::Daemon daemon{config_builder.build()};

    // No intent file, no persisted "running" state: nothing started it. The
    // absence of failures is asserted by the strict mock expectations.
    EXPECT_FALSE(QFile::exists(intent_file_path()));
}

TEST_F(TestDaemonRefresh, startupIgnoresIntentWrittenByThisRevision)
{
    // An intent written by the same revision means the refresh never happened
    // (its preparation was aborted): the daemon must not start the guest, and
    // the intent stays available for a possible retry.
    plant_instance(mp::VirtualMachine::State::stopped);
    write_intent_file({QString::fromStdString(mock_instance_name)},
                      QString::fromStdString(std::string{mp::version_string}));

    mp::Daemon daemon{config_builder.build()};

    EXPECT_TRUE(QFile::exists(intent_file_path()));
}

TEST_F(TestDaemonRefresh, explicitStartClearsTheRestartIntent)
{
    // A guest the user starts by hand is not starting again on its own after
    // a refresh; its intent entry must go away.
    plant_instance(mp::VirtualMachine::State::stopped);
    write_intent_file({QString::fromStdString(mock_instance_name)},
                      QString::fromStdString(std::string{mp::version_string}));
    mp::Daemon daemon{config_builder.build()};

    mp::StartRequest start_request;
    start_request.mutable_instance_names()->add_instance_name(mock_instance_name);
    const auto start_status = mpt::DaemonTestFixture::call_daemon_slot(
        daemon,
        &mp::Daemon::start,
        start_request,
        NiceMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>{});
    EXPECT_TRUE(start_status.ok());

    EXPECT_FALSE(QFile::exists(intent_file_path()));
}

TEST_F(TestDaemonRefresh, prepareClassifiesStartingInstanceAsUncertain)
{
    // A starting guest cannot be shut down cleanly; preparation must fail safe
    // before touching anything.
    plant_instance(mp::VirtualMachine::State::starting);
    mp::Daemon daemon{config_builder.build()};

    auto result = call_prepare_refresh(daemon);

    ASSERT_TRUE(result.status.ok());
    EXPECT_FALSE(result.reply.error_message().empty());
    EXPECT_TRUE(result.reply.stopped_instances().empty());

    // The gate was never engaged: operations still work.
    mp::StartRequest start_request;
    start_request.mutable_instance_names()->add_instance_name(mock_instance_name);
    const auto start_status = mpt::DaemonTestFixture::call_daemon_slot(
        daemon,
        &mp::Daemon::start,
        start_request,
        NiceMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>{});
    EXPECT_TRUE(start_status.ok());
}
