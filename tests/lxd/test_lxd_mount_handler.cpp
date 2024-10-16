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

#include "mock_lxd_server_responses.h"
#include "mock_network_access_manager.h"
#include "tests/mock_file_ops.h"
#include "tests/mock_logger.h"
#include "tests/mock_virtual_machine.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_dir.h"

#include "src/platform/backends/lxd/lxd_mount_handler.h"

#include <multipass/path.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_mount.h>

namespace mp = multipass;
namespace mpt = multipass::test;

namespace
{
class MockLXDVirtualMachine : public mpt::MockVirtualMachineT<mp::LXDVirtualMachine>

{
public:
    MockLXDVirtualMachine(const mp::VirtualMachineDescription& desc,
                          mp::VMStatusMonitor& monitor,
                          mp::NetworkAccessManager* manager,
                          const QUrl& base_url,
                          const QString& bridge_name,
                          const QString& storage_pool,
                          const mp::SSHKeyProvider& key_provider)
        : mpt::MockVirtualMachineT<mp::LXDVirtualMachine>{desc,
                                                          monitor,
                                                          manager,
                                                          base_url,
                                                          bridge_name,
                                                          storage_pool,
                                                          key_provider}
    {
    }
};

struct LXDMountHandlerTestFixture : public testing::Test
{
    LXDMountHandlerTestFixture()
    {
        EXPECT_CALL(mock_network_access_manager, createRequest(_, _, _))
            .Times(AtMost(6))
            .WillRepeatedly(
                [](QNetworkAccessManager::Operation, const QNetworkRequest& request, QIODevice* outgoingData) {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_stopped_data);
                });

        EXPECT_CALL(mock_file_ops, status)
            .Times(AtMost(1))
            .WillRepeatedly(Return(mp::fs::file_status{mp::fs::file_type::directory, mp::fs::perms::all}));

        logger_scope.mock_logger->screen_logs(mpl::Level::error);
    }

    const std::string source_path{"sourcePath"};
    const std::string target_path{"targetPath"};
    const mp::VMMount vm_mount{source_path, {}, {}, mp::VMMount::MountType::Native};

    mpt::MockFileOps::GuardedMock mock_file_ops_injection = mpt::MockFileOps::inject();
    mpt::MockFileOps& mock_file_ops = *mock_file_ops_injection.first;

    mpt::MockNetworkAccessManager mock_network_access_manager;

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    const mpt::StubSSHKeyProvider key_provider;
    const QUrl base_url{"unix:///foo@1.0"};
    const QString default_storage_pool{"default"};
    mpt::StubVMStatusMonitor stub_monitor;
    const QString bridge_name{"mpbr0"};
    const mp::VirtualMachineDescription default_description{2,
                                                            mp::MemorySize{"3M"},
                                                            mp::MemorySize{}, // not used
                                                            "pied-piper-valley",
                                                            "00:16:3e:fe:f2:b9",
                                                            {},
                                                            "yoda",
                                                            {},
                                                            "",
                                                            {},
                                                            {},
                                                            {},
                                                            {}};
};

struct LXDMountHandlerInvalidGidUidParameterTests : public LXDMountHandlerTestFixture,
                                                    public testing::WithParamInterface<std::tuple<int, int, int, int>>
{
};

struct LXDMountHandlerValidGidUidParameterTests : public LXDMountHandlerTestFixture,
                                                  public testing::WithParamInterface<std::tuple<int, int>>
{
};
} // namespace

TEST_F(LXDMountHandlerTestFixture, startDoesNotThrowIfVMIsStopped)
{
    NiceMock<MockLXDVirtualMachine> lxd_vm{default_description,
                                           stub_monitor,
                                           &mock_network_access_manager,
                                           base_url,
                                           bridge_name,
                                           default_storage_pool,
                                           key_provider};

    mp::LXDMountHandler lxd_mount_handler(&mock_network_access_manager, &lxd_vm, &key_provider, target_path, vm_mount);

    EXPECT_CALL(lxd_vm, current_state).WillOnce(Return(multipass::VirtualMachine::State::stopped));
    const mp::ServerVariant dummy_server;
    logger_scope.mock_logger->expect_log(mpl::Level::info, "initializing native mount ");
    EXPECT_NO_THROW(lxd_mount_handler.activate(dummy_server));
}

TEST_F(LXDMountHandlerTestFixture, startThrowsIfVMIsRunning)
{
    NiceMock<MockLXDVirtualMachine> lxd_vm{default_description,
                                           stub_monitor,
                                           &mock_network_access_manager,
                                           base_url,
                                           bridge_name,
                                           default_storage_pool,
                                           key_provider};
    mp::LXDMountHandler lxd_mount_handler(&mock_network_access_manager, &lxd_vm, &key_provider, target_path, vm_mount);

    EXPECT_CALL(lxd_vm, current_state).WillOnce(Return(multipass::VirtualMachine::State::running));
    const mp::ServerVariant dummy_server;
    MP_EXPECT_THROW_THAT(
        lxd_mount_handler.activate(dummy_server), mp::NativeMountNeedsStoppedVMException,
        mpt::match_what(AllOf(HasSubstr("Please stop the instance"), HasSubstr("before attempting native mounts."))));
}

TEST_F(LXDMountHandlerTestFixture, stopDoesNotThrowIfVMIsStopped)
{
    NiceMock<MockLXDVirtualMachine> lxd_vm{default_description,
                                           stub_monitor,
                                           &mock_network_access_manager,
                                           base_url,
                                           bridge_name,
                                           default_storage_pool,
                                           key_provider};
    mp::LXDMountHandler lxd_mount_handler(&mock_network_access_manager, &lxd_vm, &key_provider, target_path, vm_mount);

    EXPECT_CALL(lxd_vm, current_state)
        .Times(AtMost(2))
        .WillRepeatedly(Return(multipass::VirtualMachine::State::stopped));
    const mp::ServerVariant dummy_server;
    lxd_mount_handler.activate(dummy_server);
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Stopping native mount ");
    EXPECT_NO_THROW(lxd_mount_handler.deactivate());
}

TEST_F(LXDMountHandlerTestFixture, stopThrowsIfVMIsRunning)
{
    NiceMock<MockLXDVirtualMachine> lxd_vm{default_description,
                                           stub_monitor,
                                           &mock_network_access_manager,
                                           base_url,
                                           bridge_name,
                                           default_storage_pool,
                                           key_provider};

    mp::LXDMountHandler lxd_mount_handler(&mock_network_access_manager, &lxd_vm, &key_provider, target_path, vm_mount);

    EXPECT_CALL(lxd_vm, current_state).WillOnce(Return(multipass::VirtualMachine::State::stopped));
    const mp::ServerVariant dummy_server;
    lxd_mount_handler.activate(dummy_server);

    EXPECT_CALL(lxd_vm, current_state).WillOnce(Return(multipass::VirtualMachine::State::running));
    MP_EXPECT_THROW_THAT(
        lxd_mount_handler.deactivate(), std::runtime_error,
        mpt::match_what(AllOf(HasSubstr("Please stop the instance"), HasSubstr("before unmount it natively."))));
}

TEST_P(LXDMountHandlerInvalidGidUidParameterTests, mountWithGidOrUid)
{
    mpt::TempDir instance_dir{};
    mp::LXDVirtualMachine lxd_vm{default_description,
                                 stub_monitor,
                                 &mock_network_access_manager,
                                 base_url,
                                 bridge_name,
                                 default_storage_pool,
                                 key_provider,
                                 instance_dir.path()};
    const auto& [host_gid, instance_gid, host_uid, instance_uid] = GetParam();
    const mp::VMMount vm_mount{
        source_path, {{host_gid, instance_gid}}, {{host_uid, instance_uid}}, mp::VMMount::MountType::Native};

    MP_EXPECT_THROW_THAT(
        lxd_vm.make_native_mount_handler(target_path, vm_mount);
        , std::runtime_error, mpt::match_what(StrEq("LXD native mount does not accept custom ID mappings.")));
}

INSTANTIATE_TEST_SUITE_P(mountWithGidOrUidInstantiation, LXDMountHandlerInvalidGidUidParameterTests,
                         testing::Values(std::make_tuple(1000, -1, 1000, 1), std::make_tuple(2000, 1, 2000, 1),
                                         std::make_tuple(2000, -1, 2000, 1)));

TEST_P(LXDMountHandlerValidGidUidParameterTests, mountWithGidOrUid)
{
    mpt::TempDir instance_dir{};
    mp::LXDVirtualMachine lxd_vm{default_description,
                                 stub_monitor,
                                 &mock_network_access_manager,
                                 base_url,
                                 bridge_name,
                                 default_storage_pool,
                                 key_provider,
                                 instance_dir.path()};
    const auto& [host_gid, host_uid] = GetParam();
    const int default_instance_id = -1;
    const mp::VMMount vm_mount{source_path,
                               {{host_gid, default_instance_id}},
                               {{host_uid, default_instance_id}},
                               mp::VMMount::MountType::Native};

    EXPECT_NO_THROW(lxd_vm.make_native_mount_handler(target_path, vm_mount));
}

INSTANTIATE_TEST_SUITE_P(mountWithGidOrUidInstantiation, LXDMountHandlerValidGidUidParameterTests,
                         testing::Values(std::make_tuple(1000, 1000), std::make_tuple(2000, 2000),
                                         std::make_tuple(2000, 2000)));
