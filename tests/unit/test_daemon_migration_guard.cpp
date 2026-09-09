/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// The daemon test fixture contains premock code so it must be included first.
#include "daemon_test_fixture.h"

#include "common.h"
#include "mock_daemon.h"
#include "mock_daemon_rpc_context.h"
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"

#include <src/daemon/daemon.h>

#include <multipass/constants.h>

#include <QCoreApplication>
#include <QEvent>
#include <QThread>

#include <array>
#include <functional>
#include <thread>
#include <tuple>

#if defined(HYPERV_HCS_ENABLED)
#include "hyperv_api/mock_hyperv_hcn_wrapper.h"
#include "hyperv_api/mock_hyperv_hcs_wrapper.h"

#include <daemon/hyperv_driver_transition.h>
#include <hyperv_api/hcs_virtual_machine_resources.h>
#endif

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
template <typename Base>
struct MigrationTestDaemon : public Base
{
    using Base::Base;
    using Base::connect_rpc;

    void set_migrating(bool migrating)
    {
        this->migration_in_progress = migrating;
    }

    bool is_migrating() const
    {
        return this->migration_in_progress.load();
    }

    void add_instance(const std::string& name, mp::VirtualMachine::ShPtr vm)
    {
        this->operative_instances.emplace(name, std::move(vm));
        this->vm_instance_specs.emplace(name, mp::VMSpecs{});
    }
};

using GuardTestDaemon = MigrationTestDaemon<mp::Daemon>;
using MockGuardTestDaemon = MigrationTestDaemon<StrictMock<mpt::MockDaemon>>;

struct TestDaemonMigrationGuard : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
        config_builder.server_address = "localhost:0";
    }

    mpt::MockPlatform::GuardedMock platform_attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = platform_attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;
};

struct TestDaemonRpcMigrationGuard : public TestDaemonMigrationGuard
{
    void SetUp() override
    {
        TestDaemonMigrationGuard::SetUp();
        auto config = config_builder.build();
        const auto& rpc_config = *config;
        daemon = std::make_unique<MockGuardTestDaemon>(std::move(config));
        rpc = std::make_unique<mp::DaemonRpc>(rpc_config.server_address,
                                              *rpc_config.cert_provider,
                                              rpc_config.client_cert_store.get(),
                                              rpc_config.logger);
        daemon->connect_rpc(*rpc);
    }

    std::unique_ptr<MockGuardTestDaemon> daemon;
    std::unique_ptr<mp::DaemonRpc> rpc;
};

const auto finish_rpc = [](auto*, auto*, mp::DaemonRpcContext* context) {
    context->set_value(grpc::Status::OK);
};

struct RpcAdmissionCase
{
    const char* name;
    bool blocked_during_migration;
    std::function<void(mp::DaemonRpc&, mp::DaemonRpcContext*)> dispatch;
    std::function<void(mpt::MockDaemon&)> expect_dispatch;
};

template <typename Signal>
RpcAdmissionCase rpc_case(const char* name,
                          bool blocked_during_migration,
                          Signal signal,
                          std::function<void(mpt::MockDaemon&)> expect_dispatch)
{
    return {name,
            blocked_during_migration,
            [signal](mp::DaemonRpc& rpc, mp::DaemonRpcContext* context) {
                // Only mocked handlers receive these; neither request nor stream is accessed.
                (rpc.*signal)(nullptr, nullptr, context);
            },
            std::move(expect_dispatch)};
}

const auto rpc_cases = std::array{
    rpc_case("create",
             true,
             &mp::DaemonRpc::on_create,
             [](auto& daemon) { EXPECT_CALL(daemon, create).WillOnce(finish_rpc); }),
    rpc_case("launch",
             true,
             &mp::DaemonRpc::on_launch,
             [](auto& daemon) { EXPECT_CALL(daemon, launch).WillOnce(finish_rpc); }),
    rpc_case("purge",
             true,
             &mp::DaemonRpc::on_purge,
             [](auto& daemon) { EXPECT_CALL(daemon, purge).WillOnce(finish_rpc); }),
    rpc_case("find",
             false,
             &mp::DaemonRpc::on_find,
             [](auto& daemon) { EXPECT_CALL(daemon, find).WillOnce(finish_rpc); }),
    rpc_case("info",
             false,
             &mp::DaemonRpc::on_info,
             [](auto& daemon) { EXPECT_CALL(daemon, info).WillOnce(finish_rpc); }),
    rpc_case("list",
             false,
             &mp::DaemonRpc::on_list,
             [](auto& daemon) { EXPECT_CALL(daemon, list).WillOnce(finish_rpc); }),
    rpc_case("clone",
             true,
             &mp::DaemonRpc::on_clone,
             [](auto& daemon) { EXPECT_CALL(daemon, clone).WillOnce(finish_rpc); }),
    rpc_case("networks",
             false,
             &mp::DaemonRpc::on_networks,
             [](auto& daemon) { EXPECT_CALL(daemon, networks).WillOnce(finish_rpc); }),
    rpc_case("mount",
             true,
             &mp::DaemonRpc::on_mount,
             [](auto& daemon) { EXPECT_CALL(daemon, mount).WillOnce(finish_rpc); }),
    rpc_case("recover",
             true,
             &mp::DaemonRpc::on_recover,
             [](auto& daemon) { EXPECT_CALL(daemon, recover).WillOnce(finish_rpc); }),
    rpc_case("ssh_info",
             false,
             &mp::DaemonRpc::on_ssh_info,
             [](auto& daemon) { EXPECT_CALL(daemon, ssh_info).WillOnce(finish_rpc); }),
    rpc_case("start",
             true,
             &mp::DaemonRpc::on_start,
             [](auto& daemon) { EXPECT_CALL(daemon, start).WillOnce(finish_rpc); }),
    rpc_case("stop",
             true,
             &mp::DaemonRpc::on_stop,
             [](auto& daemon) { EXPECT_CALL(daemon, stop).WillOnce(finish_rpc); }),
    rpc_case("suspend",
             true,
             &mp::DaemonRpc::on_suspend,
             [](auto& daemon) { EXPECT_CALL(daemon, suspend).WillOnce(finish_rpc); }),
    rpc_case("restart",
             true,
             &mp::DaemonRpc::on_restart,
             [](auto& daemon) { EXPECT_CALL(daemon, restart).WillOnce(finish_rpc); }),
    rpc_case("delete",
             true,
             &mp::DaemonRpc::on_delete,
             [](auto& daemon) { EXPECT_CALL(daemon, delet).WillOnce(finish_rpc); }),
    rpc_case("umount",
             true,
             &mp::DaemonRpc::on_umount,
             [](auto& daemon) { EXPECT_CALL(daemon, umount).WillOnce(finish_rpc); }),
    rpc_case("version",
             false,
             &mp::DaemonRpc::on_version,
             [](auto& daemon) { EXPECT_CALL(daemon, version).WillOnce(finish_rpc); }),
    rpc_case("get",
             false,
             &mp::DaemonRpc::on_get,
             [](auto& daemon) { EXPECT_CALL(daemon, get).WillOnce(finish_rpc); }),
    rpc_case("set",
             true,
             &mp::DaemonRpc::on_set,
             [](auto& daemon) { EXPECT_CALL(daemon, set).WillOnce(finish_rpc); }),
    rpc_case("keys",
             false,
             &mp::DaemonRpc::on_keys,
             [](auto& daemon) { EXPECT_CALL(daemon, keys).WillOnce(finish_rpc); }),
    rpc_case("authenticate",
             false,
             &mp::DaemonRpc::on_authenticate,
             [](auto& daemon) { EXPECT_CALL(daemon, authenticate).WillOnce(finish_rpc); }),
    rpc_case("snapshot",
             true,
             &mp::DaemonRpc::on_snapshot,
             [](auto& daemon) { EXPECT_CALL(daemon, snapshot).WillOnce(finish_rpc); }),
    rpc_case("restore",
             true,
             &mp::DaemonRpc::on_restore,
             [](auto& daemon) { EXPECT_CALL(daemon, restore).WillOnce(finish_rpc); }),
    rpc_case("daemon_info",
             false,
             &mp::DaemonRpc::on_daemon_info,
             [](auto& daemon) { EXPECT_CALL(daemon, daemon_info).WillOnce(finish_rpc); }),
    rpc_case("wait_ready",
             false,
             &mp::DaemonRpc::on_wait_ready,
             [](auto& daemon) { EXPECT_CALL(daemon, wait_ready).WillOnce(finish_rpc); }),
    rpc_case("zones",
             false,
             &mp::DaemonRpc::on_zones,
             [](auto& daemon) { EXPECT_CALL(daemon, zones).WillOnce(finish_rpc); }),
    rpc_case("zones_state", true, &mp::DaemonRpc::on_zones_state, [](auto& daemon) {
        EXPECT_CALL(daemon, zones_state).WillOnce(finish_rpc);
    })};

struct TestDaemonRpcAdmission : public TestDaemonRpcMigrationGuard,
                                public WithParamInterface<std::tuple<RpcAdmissionCase, bool>>
{
};
} // namespace

TEST_P(TestDaemonRpcAdmission, dispatchHonoursMigrationPolicy)
{
    const auto& [rpc_case, migrating] = GetParam();
    daemon->set_migrating(migrating);
    StrictMock<mpt::MockDaemonRpcContext> context;

    if (migrating && rpc_case.blocked_during_migration)
    {
        EXPECT_CALL(context, set_value).WillOnce([](const grpc::Status& status) {
            EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
            EXPECT_EQ(status.error_message(),
                      "Cannot perform this operation while a Hyper-V instance migration is in "
                      "progress. Please wait for the migration to finish and try again.");
        });
    }
    else
    {
        rpc_case.expect_dispatch(*daemon);
        EXPECT_CALL(context, set_value).WillOnce([](const grpc::Status& status) {
            EXPECT_TRUE(status.ok());
        });
    }

    rpc_case.dispatch(*rpc, &context);
}

INSTANTIATE_TEST_SUITE_P(AllRpcs,
                         TestDaemonRpcAdmission,
                         Combine(ValuesIn(rpc_cases), Bool()),
                         [](const auto& info) {
                             return std::string{std::get<0>(info.param).name} +
                                    (std::get<1>(info.param) ? "DuringMigration"
                                                             : "WithoutMigration");
                         });

TEST_F(TestDaemonRpcMigrationGuard, rejectsQueuedRpcIfMigrationStartsBeforeDispatch)
{
    StrictMock<mpt::MockDaemonRpcContext> context;
    bool completed = false;
    EXPECT_CALL(context, set_value).WillOnce([&](const grpc::Status& status) {
        EXPECT_EQ(QThread::currentThread(), daemon->thread());
        EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
        completed = true;
    });

    std::thread{[&] { rpc->on_start(nullptr, nullptr, &context); }}.join();
    EXPECT_FALSE(completed);
    daemon->set_migrating(true);
    QCoreApplication::sendPostedEvents(daemon.get(), QEvent::MetaCall);

    EXPECT_TRUE(completed);
}

TEST_F(TestDaemonRpcMigrationGuard, allowsQueuedRpcIfMigrationEndsBeforeDispatch)
{
    daemon->set_migrating(true);
    StrictMock<mpt::MockDaemonRpcContext> context;
    bool completed = false;
    EXPECT_CALL(*daemon, start).WillOnce([&](auto* request, auto* server, auto* context) {
        EXPECT_EQ(QThread::currentThread(), daemon->thread());
        finish_rpc(request, server, context);
    });
    EXPECT_CALL(context, set_value).WillOnce([&](const grpc::Status& status) {
        EXPECT_TRUE(status.ok());
        completed = true;
    });

    std::thread{[&] { rpc->on_start(nullptr, nullptr, &context); }}.join();
    EXPECT_FALSE(completed);
    daemon->set_migrating(false);
    QCoreApplication::sendPostedEvents(daemon.get(), QEvent::MetaCall);

    EXPECT_TRUE(completed);
}

TEST_F(TestDaemonRpcMigrationGuard, initiatingSettingCanAcquireMigrationGuard)
{
    StrictMock<mpt::MockDaemonRpcContext> context;
    EXPECT_CALL(*daemon, set).WillOnce([&](auto* request, auto* server, auto* context) {
        daemon->set_migrating(true);
        finish_rpc(request, server, context);
    });
    EXPECT_CALL(context, set_value).WillOnce([](const grpc::Status& status) {
        EXPECT_TRUE(status.ok());
    });

    rpc->on_set(nullptr, nullptr, &context);

    EXPECT_TRUE(daemon->is_migrating());
}

#if defined(HYPERV_HCS_ENABLED)
TEST_F(TestDaemonMigrationGuard, driverChangeRejectsRunningHcsInstance)
{
    GuardTestDaemon daemon{config_builder.build()};
    auto vm = std::make_shared<NiceMock<mpt::MockVirtualMachine>>();
    ON_CALL(*vm, current_state).WillByDefault(Return(mp::VirtualMachine::State::running));
    daemon.add_instance("running", vm);

    EXPECT_CALL(*mock_platform, is_backend_supported(QStringLiteral("hyperv")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));

    mp::SetRequest request;
    request.set_key(mp::driver_key);
    request.set_val("hyperv");
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;

    const auto status = call_daemon_slot(daemon, &mp::Daemon::set, request, server);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
    EXPECT_THAT(status.error_message(), HasSubstr("instance is not stopped"));
    EXPECT_FALSE(daemon.is_migrating());
}

TEST_F(TestDaemonMigrationGuard, driverChangeHoldsGuardAcrossSettingsWrite)
{
    GuardTestDaemon daemon{config_builder.build()};
    EXPECT_CALL(*mock_platform, is_backend_supported(QStringLiteral("hyperv")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    EXPECT_CALL(mock_settings, set(Eq(mp::driver_key), Eq("hyperv"), _)).WillOnce([&daemon] {
        EXPECT_TRUE(daemon.is_migrating());
    });

    mp::SetRequest request;
    request.set_key(mp::driver_key);
    request.set_val("HYPER-V");
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;

    EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::set, request, server).ok());
    EXPECT_FALSE(daemon.is_migrating());
}

TEST_F(TestDaemonMigrationGuard, driverChangeReleasesGuardWhenSettingsWriteFails)
{
    GuardTestDaemon daemon{config_builder.build()};
    EXPECT_CALL(*mock_platform, is_backend_supported(QStringLiteral("virtualbox")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    EXPECT_CALL(mock_settings, set(Eq(mp::driver_key), Eq("virtualbox"), _)).WillOnce([&daemon] {
        EXPECT_TRUE(daemon.is_migrating());
        throw std::runtime_error{"settings write failed"};
    });

    mp::SetRequest request;
    request.set_key(mp::driver_key);
    request.set_val("virtualbox");
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;

    const auto status = call_daemon_slot(daemon, &mp::Daemon::set, request, server);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_EQ(status.error_message(), "settings write failed");
    EXPECT_FALSE(daemon.is_migrating());
}

TEST_F(TestDaemonMigrationGuard, driverChangeReleasesHcsResourcesBeforeSettingsWrite)
{
    const auto hcs_mock = mpt::MockHCSWrapper::inject<StrictMock>();
    const auto hcn_mock = mpt::MockHCNWrapper::inject<StrictMock>();
    GuardTestDaemon daemon{config_builder.build()};
    auto vm = std::make_shared<StrictMock<mpt::MockVirtualMachine>>();
    daemon.add_instance("stopped", vm);

    InSequence sequence;
    EXPECT_CALL(*mock_platform, is_backend_supported(QStringLiteral("hyperv")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    EXPECT_CALL(*vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::stopped));
    EXPECT_CALL(*hcs_mock.first, open_compute_system("stopped", _))
        .WillOnce(Return(mp::hyperv::OperationResult{HCS_E_SYSTEM_NOT_FOUND, L""}));
    EXPECT_CALL(*hcn_mock.first, delete_endpoint(mp::hyperv::endpoint_guid_for_mac("")))
        .WillOnce(Return(mp::hyperv::OperationResult{HCN_E_ENDPOINT_NOT_FOUND, L""}));
    EXPECT_CALL(mock_settings, set(Eq(mp::driver_key), Eq("hyperv"), _)).WillOnce([&daemon] {
        EXPECT_TRUE(daemon.is_migrating());
    });

    mp::SetRequest request;
    request.set_key(mp::driver_key);
    request.set_val("hyperv");
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;

    EXPECT_TRUE(call_daemon_slot(daemon, &mp::Daemon::set, request, server).ok());
    EXPECT_FALSE(daemon.is_migrating());
}

TEST_F(TestDaemonMigrationGuard, driverChangeDoesNotWriteSettingsWhenResourceCleanupFails)
{
    const auto hcs_mock = mpt::MockHCSWrapper::inject<StrictMock>();
    GuardTestDaemon daemon{config_builder.build()};
    auto vm = std::make_shared<StrictMock<mpt::MockVirtualMachine>>();
    daemon.add_instance("stopped", vm);

    EXPECT_CALL(*mock_platform, is_backend_supported(QStringLiteral("hyperv")))
        .WillOnce(Return(true));
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    EXPECT_CALL(*vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::stopped));
    EXPECT_CALL(*hcs_mock.first, open_compute_system("stopped", _))
        .WillOnce(Return(mp::hyperv::OperationResult{E_ACCESSDENIED, L"access denied"}));

    mp::SetRequest request;
    request.set_key(mp::driver_key);
    request.set_val("hyperv");
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;

    const auto status = call_daemon_slot(daemon, &mp::Daemon::set, request, server);
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_EQ(status.error_message(), "Could not release hyperv_api resources for 'stopped'");
    EXPECT_FALSE(daemon.is_migrating());
}

namespace
{
struct TestHyperVDriverTransition : public TestDaemonMigrationGuard
{
    mp::hyperv::DriverTransition transition(const mp::DaemonConfig& config)
    {
        return {config, specs, instances, deleted_instances, migrating, preparing};
    }

    std::unordered_map<std::string, mp::VMSpecs> specs;
    mp::hyperv::DriverTransition::InstanceTable instances;
    mp::hyperv::DriverTransition::InstanceTable deleted_instances;
    std::atomic<bool> migrating{false};
    std::atomic_size_t preparing{0};
    StrictMock<mpt::MockServerReaderWriter<mp::SetReply, mp::SetRequest>> server;
};
} // namespace

TEST_F(TestHyperVDriverTransition, unrelatedSettingDoesNotAcquireGuardOrReadDriver)
{
    const auto config = config_builder.build();
    auto change = transition(*config);

    EXPECT_TRUE(change.prepare("local.mounts", "true").ok());
    EXPECT_FALSE(migrating);
    EXPECT_TRUE(change.complete(&server).ok());
}

TEST_F(TestHyperVDriverTransition, unrelatedDriverChangeDoesNotAcquireGuard)
{
    const auto config = config_builder.build();
    auto change = transition(*config);
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("virtualbox")));

    EXPECT_TRUE(change.prepare(mp::driver_key, "hyperv").ok());
    EXPECT_FALSE(migrating);
    EXPECT_TRUE(change.complete(&server).ok());
}

TEST_F(TestHyperVDriverTransition, unchangedHcsDriverDoesNotAcquireGuard)
{
    const auto config = config_builder.build();
    auto change = transition(*config);
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));

    EXPECT_TRUE(change.prepare(mp::driver_key, "hyperv_api").ok());
    EXPECT_FALSE(migrating);
    EXPECT_TRUE(change.complete(&server).ok());
}

TEST_F(TestHyperVDriverTransition, conflictingTransitionDoesNotReleaseAnotherOwnersGuard)
{
    const auto config = config_builder.build();
    migrating = true;
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    {
        auto change = transition(*config);
        const auto status = change.prepare(mp::driver_key, "hyperv");
        EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
        EXPECT_EQ(status.error_message(),
                  mp::hyperv::migration_conflict_status("change settings").error_message());
    }
    EXPECT_TRUE(migrating);
}

TEST_F(TestHyperVDriverTransition, preparationConflictReleasesAcquiredGuard)
{
    const auto config = config_builder.build();
    preparing = 1;
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    {
        auto change = transition(*config);
        const auto status = change.prepare(mp::driver_key, "hyperv");
        EXPECT_EQ(status.error_code(), grpc::StatusCode::FAILED_PRECONDITION);
        EXPECT_EQ(status.error_message(),
                  "Cannot change driver while an instance is being prepared");
    }
    EXPECT_FALSE(migrating);
    EXPECT_EQ(preparing.load(), 1);
}

TEST_F(TestHyperVDriverTransition, deletedHcsInstancesAreCheckedBeforeReleasingResources)
{
    const auto config = config_builder.build();
    auto vm = std::make_shared<StrictMock<mpt::MockVirtualMachine>>();
    deleted_instances.emplace("deleted", vm);
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key)))
        .WillOnce(Return(QStringLiteral("hyperv_api")));
    EXPECT_CALL(*vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::running));

    MP_EXPECT_THROW_THAT(
        {
            auto change = transition(*config);
            (void)change.prepare(mp::driver_key, "hyperv");
        },
        std::exception,
        Property(&std::exception::what, HasSubstr("instance is not stopped")));
    EXPECT_FALSE(migrating);
}
#endif
