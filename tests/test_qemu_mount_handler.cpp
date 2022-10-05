/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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

#include "common.h"
#include "mock_file_ops.h"
#include "mock_logger.h"
#include "mock_server_reader_writer.h"
#include "mock_ssh_process_exit_status.h"
#include "mock_ssh_test_fixture.h"
#include "mock_virtual_machine.h"
#include "qemu_mount_handler.h"
#include "stub_ssh_key_provider.h"
#include "stub_virtual_machine.h"

#include <multipass/utils.h>
#include <multipass/vm_mount.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

const multipass::logging::Level default_log_level = multipass::logging::Level::debug;

struct QemuMountHandlerTest : public ::Test
{
    mpt::StubSSHKeyProvider key_provider;
    std::string source_path{"/my/source/path"}, target_path{"/the/target/path"};
    mpt::MockFileOps::GuardedMock mock_file_ops_injection = mpt::MockFileOps::inject();
    mpt::MockFileOps& mock_file_ops = *mock_file_ops_injection.first;
    mp::id_mappings gid_mappings{{1, 2}}, uid_mappings{{5, -1}};
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject(default_log_level);
    mpt::MockServerReaderWriter<mp::MountReply, mp::MountRequest> server;
    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    mpt::ExitStatusMock exit_status_mock;
    mpt::MockVirtualMachine vm{"my_instance"};
};

TEST_F(QemuMountHandlerTest, mountFailsOnNotStoppedState)
{
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::running));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    const mp::VMMount mount{source_path, gid_mappings, uid_mappings, mp::VMMount::MountType::Performance};

    EXPECT_THROW(
        try { qemu_mount_handler.init_mount(&vm, target_path, mount); } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Please shutdown virtual machine before defining native mount.");
            throw;
        },
        std::runtime_error);
}

TEST_F(QemuMountHandlerTest, mountFailsOnNonExistentPath)
{
    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(false));
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::off));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    const mp::VMMount mount{source_path, gid_mappings, uid_mappings, mp::VMMount::MountType::Performance};

    EXPECT_THROW(
        try { qemu_mount_handler.init_mount(&vm, target_path, mount); } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Mount path \"/my/source/path\" does not exist.");
            throw;
        },
        std::runtime_error);
}

TEST_F(QemuMountHandlerTest, mountFailsOnMultipleUIDs)
{
    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(true));
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::off));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    const mp::VMMount mount{source_path, {{1, 2}, {3, 4}}, {{5, -1}, {6, 10}}, mp::VMMount::MountType::Performance};

    EXPECT_THROW(
        try { qemu_mount_handler.init_mount(&vm, target_path, mount); } catch (const std::runtime_error& e) {
            EXPECT_STREQ(e.what(), "Only one mapping per native mount allowed.");
            throw;
        },
        std::runtime_error);
}

TEST_F(QemuMountHandlerTest, hasInstanceAlreadyMountedReturnsTrueWhenFound)
{
    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(true));
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::off));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    const mp::VMMount mount{source_path, gid_mappings, uid_mappings, mp::VMMount::MountType::Performance};
    EXPECT_CALL(vm, add_vm_mount(target_path, mount)).WillOnce(Return());

    qemu_mount_handler.init_mount(&vm, target_path, mount);

    EXPECT_TRUE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, target_path));
}

TEST_F(QemuMountHandlerTest, hasInstanceAlreadyMountedReturnsFalseWhenFound)
{
    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).WillOnce(Return(true));
    EXPECT_CALL(vm, current_state()).WillOnce(Return(mp::VirtualMachine::State::off));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    const mp::VMMount mount{source_path, gid_mappings, uid_mappings, mp::VMMount::MountType::Performance};
    EXPECT_CALL(vm, add_vm_mount(target_path, mount)).WillOnce(Return());

    qemu_mount_handler.init_mount(&vm, target_path, mount);

    EXPECT_FALSE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, "/bad/path"));
}

TEST_F(QemuMountHandlerTest, stopNonExistentMountLogsMessageAndReturns)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::info);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::info), mpt::MockLogger::make_cstring_matcher(StrEq("qemu-mount-handler")),
                    mpt::MockLogger::make_cstring_matcher(StrEq(
                        fmt::format("No native mount defined for \"{}\" serving '{}'", vm.vm_name, target_path)))));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    qemu_mount_handler.stop_mount(vm.vm_name, target_path);
}

TEST_F(QemuMountHandlerTest, stopAllMountsForInstanceWithNoMountsLogsMessageAndReturns)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::info);
    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::info), mpt::MockLogger::make_cstring_matcher(StrEq("qemu-mount-handler")),
                    mpt::MockLogger::make_cstring_matcher(
                        StrEq(fmt::format("No native mounts to stop for instance \"{}\"", vm.vm_name)))));

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    qemu_mount_handler.stop_all_mounts_for_instance(vm.vm_name);
}

TEST_F(QemuMountHandlerTest, stopAllMountsDeletesAllMounts)
{
    const mp::VMMount mount1{"/source/one", gid_mappings, uid_mappings, mp::VMMount::MountType::Performance},
        mount2{"/source/two", gid_mappings, uid_mappings, mp::VMMount::MountType::Performance},
        mount3{"/source/three", gid_mappings, uid_mappings, mp::VMMount::MountType::Performance};

    EXPECT_CALL(mock_file_ops, exists(A<const QDir&>())).Times(3).WillRepeatedly(Return(true));
    EXPECT_CALL(vm, add_vm_mount(A<const std::string&>(), A<const mp::VMMount&>())).Times(3).WillRepeatedly(Return());

    mp::QemuMountHandler qemu_mount_handler(key_provider);

    qemu_mount_handler.init_mount(&vm, "/target/one", mount1);
    qemu_mount_handler.init_mount(&vm, "/target/two", mount2);
    qemu_mount_handler.init_mount(&vm, "/target/three", mount3);

    qemu_mount_handler.start_mount(&vm, &server, "/target/one");
    qemu_mount_handler.start_mount(&vm, &server, "/target/two");
    qemu_mount_handler.start_mount(&vm, &server, "/target/three");

    EXPECT_TRUE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, "/source/one"));
    EXPECT_TRUE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, "/source/two"));
    EXPECT_TRUE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, "/source/three"));

    qemu_mount_handler.stop_all_mounts_for_instance(vm.vm_name);

    EXPECT_FALSE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, "/source/one"));
    EXPECT_FALSE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, "/source/two"));
    EXPECT_FALSE(qemu_mount_handler.has_instance_already_mounted(vm.vm_name, "/source/three"));
}
