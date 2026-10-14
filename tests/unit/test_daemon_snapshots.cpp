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

#include "daemon_test_fixture.h"
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_snapshot.h"
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"

#include <src/daemon/daemon.h>

#include <QDateTime>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestDaemonSnapshots : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    }

    std::shared_ptr<NiceMock<mpt::MockSnapshot>> make_mock_snapshot(const std::string& name,
                                                                    const std::string& parent,
                                                                    const std::string& comment)
    {
        auto snapshot = std::make_shared<NiceMock<mpt::MockSnapshot>>();
        EXPECT_CALL(*snapshot, get_name()).WillRepeatedly(Return(name));
        EXPECT_CALL(*snapshot, get_parents_name()).WillRepeatedly(Return(parent));
        EXPECT_CALL(*snapshot, get_comment()).WillRepeatedly(Return(comment));
        EXPECT_CALL(*snapshot, get_creation_timestamp())
            .WillRepeatedly(Return(QDateTime::fromSecsSinceEpoch(0)));
        return snapshot;
    }

    std::pair<std::unique_ptr<mp::Daemon>, mpt::MockVirtualMachine*>
    build_daemon_with_mock_instance(bool deleted = false)
    {
        mpt::fake_vm_properties props{};
        props.name = mock_instance_name;
        props.default_mac = mac_addr;
        props.deleted = deleted;
        props.state = mp::VirtualMachine::State::off;
        const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(props));

        auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
        auto* ret_instance = instance_ptr.get();
        EXPECT_CALL(*instance_ptr, get_name).WillRepeatedly(ReturnRef(mock_instance_name));
        EXPECT_CALL(mock_factory, create_virtual_machine).WillOnce(Return(std::move(instance_ptr)));

        config_builder.data_directory = temp_dir->path();
        return {std::make_unique<mp::Daemon>(config_builder.build()), ret_instance};
    }

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection =
        mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockPermissionUtils::GuardedMock mock_permission_utils_injection =
        mpt::MockPermissionUtils::inject<NiceMock>();
    mpt::MockPermissionUtils& mock_permission_utils = *mock_permission_utils_injection.first;

    mpt::MockVirtualMachineFactory& mock_factory = *use_a_mock_vm_factory();

    const std::string mac_addr{"52:54:00:73:76:28"};
    const std::string mock_instance_name{"real-zebraphant"};
};

TEST_F(TestDaemonSnapshots, snapshotsCmdReturnsStatusOk)
{
    mp::Daemon daemon{config_builder.build()};
    StrictMock<mpt::MockServerReaderWriter<mp::SnapshotsReply, mp::SnapshotsRequest>> mock_server;

    mp::SnapshotsRequest request;
    mp::SnapshotsReply reply;
    EXPECT_CALL(mock_server, Write(_, _)).WillOnce(DoAll(SaveArg<0>(&reply), Return(true)));

    const auto status = call_daemon_slot(daemon, &mp::Daemon::snapshots, request, mock_server);

    ASSERT_TRUE(status.ok());
    EXPECT_THAT(reply.snapshot_list().snapshots(), IsEmpty());
}

TEST_F(TestDaemonSnapshots, snapshotsCmdReturnsInstanceSnapshots)
{
    auto [daemon, instance] = build_daemon_with_mock_instance();

    EXPECT_CALL(*instance, view_snapshots(_))
        .WillOnce(Return(mp::VirtualMachine::SnapshotVista{
            make_mock_snapshot("snapshot1", "", "the first one"),
            make_mock_snapshot("snapshot2", "snapshot1", "the second one")}));

    StrictMock<mpt::MockServerReaderWriter<mp::SnapshotsReply, mp::SnapshotsRequest>> mock_server;
    mp::SnapshotsReply reply;
    EXPECT_CALL(mock_server, Write(_, _)).WillOnce(DoAll(SaveArg<0>(&reply), Return(true)));

    mp::SnapshotsRequest request;
    const auto status = call_daemon_slot(*daemon, &mp::Daemon::snapshots, request, mock_server);

    ASSERT_TRUE(status.ok());
    EXPECT_THAT(
        reply.snapshot_list().snapshots(),
        UnorderedElementsAre(
            AllOf(
                Property(&mp::ListVMSnapshot::name, Eq(mock_instance_name)),
                Property(&mp::ListVMSnapshot::fundamentals,
                         AllOf(Property(&mp::SnapshotFundamentals::snapshot_name, Eq("snapshot1")),
                               Property(&mp::SnapshotFundamentals::parent, IsEmpty()),
                               Property(&mp::SnapshotFundamentals::comment, Eq("the first one"))))),
            AllOf(Property(&mp::ListVMSnapshot::name, Eq(mock_instance_name)),
                  Property(
                      &mp::ListVMSnapshot::fundamentals,
                      AllOf(Property(&mp::SnapshotFundamentals::snapshot_name, Eq("snapshot2")),
                            Property(&mp::SnapshotFundamentals::parent, Eq("snapshot1")),
                            Property(&mp::SnapshotFundamentals::comment, Eq("the second one")))))));
}

TEST_F(TestDaemonSnapshots, snapshotsCmdIncludesDeletedInstanceSnapshots)
{
    auto [daemon, instance] = build_daemon_with_mock_instance(/* deleted = */ true);

    EXPECT_CALL(*instance, view_snapshots(_))
        .WillOnce(Return(
            mp::VirtualMachine::SnapshotVista{make_mock_snapshot("snapshot1", "", "on a corpse")}));

    StrictMock<mpt::MockServerReaderWriter<mp::SnapshotsReply, mp::SnapshotsRequest>> mock_server;
    mp::SnapshotsReply reply;
    EXPECT_CALL(mock_server, Write(_, _)).WillOnce(DoAll(SaveArg<0>(&reply), Return(true)));

    mp::SnapshotsRequest request;
    const auto status = call_daemon_slot(*daemon, &mp::Daemon::snapshots, request, mock_server);

    EXPECT_TRUE(status.ok());
    EXPECT_THAT(reply.snapshot_list().snapshots(),
                ElementsAre(AllOf(Property(&mp::ListVMSnapshot::name, Eq(mock_instance_name)),
                                  Property(&mp::ListVMSnapshot::fundamentals,
                                           Property(&mp::SnapshotFundamentals::snapshot_name,
                                                    Eq("snapshot1"))))));
}
