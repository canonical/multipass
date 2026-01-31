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

#include "mock_applevz_wrapper.h"
#include "tests/unit/common.h"
#include "tests/unit/mock_logger.h"
#include "tests/unit/mock_status_monitor.h"
#include "tests/unit/stub_ssh_key_provider.h"
#include "tests/unit/temp_dir.h"
#include "tests/unit/temp_file.h"

#include <applevz/applevz_virtual_machine.h>
#include <multipass/exceptions/virtual_machine_state_exceptions.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace multipass::test
{
struct AppleVZVirtualMachine_UnitTests : public testing::Test
{
    AppleVZVirtualMachine_UnitTests()
    {
        vm.state = mp::VirtualMachine::State::running;
    }

    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mpt::TempDir dummy_instances_dir;
    const std::string dummy_vm_name{"lord-of-the-pings"};

    mp::VirtualMachineDescription desc{2,
                                       mp::MemorySize{"3M"},
                                       mp::MemorySize{}, // not used
                                       dummy_vm_name,
                                       "aa:bb:cc:dd:ee:ff",
                                       {},
                                       "",
                                       {dummy_image.name(), "", "", "", {}, {}},
                                       dummy_cloud_init_iso.name(),
                                       {},
                                       {},
                                       {},
                                       {}};

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    mpt::StubSSHKeyProvider stub_key_provider{};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    mpt::MockAppleVZ::GuardedMock mock_applevz_injection{mpt::MockAppleVZ::inject<NiceMock>()};
    mpt::MockAppleVZ& mock_applevz = *mock_applevz_injection.first;

    mpt::TempDir instance_dir;
    mp::applevz::AppleVZVirtualMachine vm{desc,
                                          mock_monitor,
                                          stub_key_provider,
                                          instance_dir.path()};
};

TEST_F(AppleVZVirtualMachine_UnitTests, startFromStoppedStateCallsStartVm)
{
    EXPECT_CALL(mock_applevz, get_state(_))
        .WillOnce(Return(applevz::AppleVMState::stopped))
        .WillOnce(Return(applevz::AppleVMState::running));
    EXPECT_CALL(mock_applevz, can_start(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, start_vm(_)).WillOnce(Invoke([](auto&) {
        return applevz::CFError{};
    }));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::starting));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::running));

    vm.start();

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::running);
}

TEST_F(AppleVZVirtualMachine_UnitTests, startFromPausedStateCallsResumeVm)
{
    EXPECT_CALL(mock_applevz, get_state(_))
        .WillOnce(Return(applevz::AppleVMState::paused))
        .WillOnce(Return(applevz::AppleVMState::running));
    EXPECT_CALL(mock_applevz, can_resume(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, resume_vm(_)).WillOnce(Invoke([](auto&) {
        return applevz::CFError{};
    }));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, mp::VirtualMachine::State::starting));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, mp::VirtualMachine::State::running));

    vm.start();

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::running);
}

TEST_F(AppleVZVirtualMachine_UnitTests, startFromRunningStateNoOp)
{
    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::running));
    EXPECT_CALL(mock_applevz, can_resume(_)).WillOnce(Return(false));
    EXPECT_CALL(mock_applevz, can_start(_)).WillOnce(Return(false));
    EXPECT_CALL(mock_monitor, persist_state_for(_, _)).Times(AtLeast(1));

    vm.start();

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::running);
}

TEST_F(AppleVZVirtualMachine_UnitTests, startVmErrorThrowsRuntimeError)
{
    CFErrorRef error_ref = CFErrorCreate(kCFAllocatorDefault, CFSTR("TestDomain"), 42, nullptr);
    applevz::CFError error{error_ref};

    EXPECT_CALL(mock_applevz, get_state(_))
        .WillOnce(Return(applevz::AppleVMState::stopped))
        .WillOnce(Return(applevz::AppleVMState::error));
    EXPECT_CALL(mock_applevz, can_start(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, start_vm(_)).WillOnce(Return(ByMove(std::move(error))));
    EXPECT_CALL(mock_monitor, persist_state_for(_, _)).Times(AtLeast(1));

    EXPECT_THROW(vm.start(), std::runtime_error);
    EXPECT_EQ(vm.current_state(), VirtualMachine::State::unknown);
}

TEST_F(AppleVZVirtualMachine_UnitTests, startResumeErrorThrowsRuntimeError)
{
    CFErrorRef error_ref = CFErrorCreate(kCFAllocatorDefault, CFSTR("TestDomain"), 42, nullptr);
    applevz::CFError error{error_ref};

    EXPECT_CALL(mock_applevz, get_state(_))
        .WillOnce(Return(applevz::AppleVMState::paused))
        .WillOnce(Return(applevz::AppleVMState::error));
    EXPECT_CALL(mock_applevz, can_resume(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, resume_vm(_)).WillOnce(Return(ByMove(std::move(error))));
    EXPECT_CALL(mock_monitor, persist_state_for(_, _)).Times(AtLeast(1));

    EXPECT_THROW(vm.start(), std::runtime_error);
    EXPECT_EQ(vm.current_state(), VirtualMachine::State::unknown);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownFromRunningStateWithPowerdownCallsStopVm)
{
    EXPECT_CALL(mock_applevz, get_state(_))
        .WillOnce(Return(applevz::AppleVMState::running))
        .WillOnce(Return(applevz::AppleVMState::stopping))
        .WillRepeatedly(Return(applevz::AppleVMState::stopped));

    EXPECT_CALL(mock_applevz, can_request_stop(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, stop_vm(_, false)).WillOnce(Invoke([](auto&, bool) {
        return applevz::CFError{};
    }));

    vm.shutdown(VirtualMachine::ShutdownPolicy::Powerdown);

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::stopped);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownWithPoweroffCallsStopVmWithForce)
{
    EXPECT_CALL(mock_applevz, get_state(_))
        .WillOnce(Return(applevz::AppleVMState::running))
        .WillOnce(Return(applevz::AppleVMState::stopping))
        .WillRepeatedly(Return(applevz::AppleVMState::stopped));

    EXPECT_CALL(mock_applevz, can_stop(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, stop_vm(_, true)).WillOnce(Invoke([](auto&, bool) {
        return applevz::CFError{};
    }));

    vm.shutdown(VirtualMachine::ShutdownPolicy::Poweroff);

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::stopped);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownFromStoppedStateIsIdempotent)
{
    vm.state = mp::VirtualMachine::State::stopped;
    EXPECT_CALL(mock_applevz, stop_vm(_, _)).Times(0);

    vm.shutdown();

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::stopped);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownGracefulStopErrorThrowsRuntimeError)
{
    EXPECT_CALL(mock_applevz, get_state(_)).WillOnce(Return(applevz::AppleVMState::error));

    EXPECT_CALL(mock_applevz, can_request_stop(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_applevz, stop_vm(_, false)).WillRepeatedly(Invoke([](auto&, bool) {
        return applevz::CFError{
            CFErrorCreate(kCFAllocatorDefault, CFSTR("TestDomain"), 123, nullptr)};
    }));

    EXPECT_THROW(vm.shutdown(), std::runtime_error);
    EXPECT_EQ(vm.current_state(), VirtualMachine::State::unknown);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownForcedStopErrorThrowsRuntimeError)
{
    EXPECT_CALL(mock_applevz, get_state(_)).WillOnce(Return(applevz::AppleVMState::error));

    EXPECT_CALL(mock_applevz, can_stop(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, stop_vm(_, true)).WillOnce(Invoke([](auto&, bool) {
        return applevz::CFError{
            CFErrorCreate(kCFAllocatorDefault, CFSTR("TestDomain"), 456, nullptr)};
    }));

    EXPECT_THROW(vm.shutdown(VirtualMachine::ShutdownPolicy::Poweroff), std::runtime_error);
    EXPECT_EQ(vm.current_state(), VirtualMachine::State::unknown);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownCannotStopReturnsEarly)
{
    EXPECT_CALL(mock_applevz, can_request_stop(_)).WillOnce(Return(false));
    EXPECT_CALL(mock_applevz, stop_vm(_, _)).Times(0);

    vm.shutdown();

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::running);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownFromSuspendedStateWithHaltIsIdempotent)
{
    vm.state = mp::VirtualMachine::State::suspended;

    EXPECT_CALL(mock_applevz, stop_vm(_, _)).Times(0);
    EXPECT_CALL(mock_applevz, can_stop(_)).Times(0);

    EXPECT_NO_THROW(vm.shutdown(VirtualMachine::ShutdownPolicy::Halt));
    EXPECT_EQ(vm.current_state(), VirtualMachine::State::suspended);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendFromRunningStateCallsPauseVm)
{
    EXPECT_CALL(mock_applevz, get_state(_)).WillOnce(Return(applevz::AppleVMState::paused));

    EXPECT_CALL(mock_applevz, can_pause(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, pause_vm(_)).WillOnce(Invoke([](auto&) {
        return applevz::CFError{};
    }));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::suspending));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::suspended));

    vm.suspend();

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::suspended);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendFromStoppedStateLogsWarning)
{
    vm.state = mp::VirtualMachine::State::stopped;

    EXPECT_CALL(mock_applevz, can_pause(_)).WillOnce(Return(false));
    EXPECT_CALL(mock_applevz, pause_vm(_)).Times(0);

    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    logger_scope.mock_logger->expect_log(mpl::Level::warning,
                                         fmt::format("VM `{}` cannot be suspended from state `{}`",
                                                     desc.vm_name,
                                                     mp::VirtualMachine::State::stopped));

    vm.suspend();

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::stopped);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendCannotPauseReturnsEarly)
{
    EXPECT_CALL(mock_applevz, can_pause(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_applevz, pause_vm(_)).Times(0);
    EXPECT_CALL(mock_applevz, get_state(_)).WillOnce(Return(applevz::AppleVMState::running));

    vm.suspend();

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::running);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendErrorThrowsRuntimeError)
{
    EXPECT_CALL(mock_applevz, get_state(_)).WillOnce(Return(applevz::AppleVMState::error));

    EXPECT_CALL(mock_applevz, can_pause(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, pause_vm(_)).WillOnce(Invoke([](auto&) {
        return applevz::CFError{
            CFErrorCreate(kCFAllocatorDefault, CFSTR("TestDomain"), 789, nullptr)};
    }));

    EXPECT_THROW(vm.suspend(), std::runtime_error);
    EXPECT_EQ(vm.current_state(), VirtualMachine::State::unknown);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendTransitionsToSuspendingState)
{
    EXPECT_CALL(mock_applevz, get_state(_)).WillOnce(Return(applevz::AppleVMState::paused));

    EXPECT_CALL(mock_applevz, can_pause(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, pause_vm(_)).WillOnce(Invoke([](auto&) {
        return applevz::CFError{};
    }));

    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::suspending));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::suspended));

    vm.suspend();

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::suspended);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendFromAlreadySuspendedStateIsIdempotent)
{
    vm.state = mp::VirtualMachine::State::suspended;

    EXPECT_CALL(mock_applevz, can_pause(_)).WillOnce(Return(false));
    EXPECT_CALL(mock_applevz, pause_vm(_)).Times(0);
    EXPECT_CALL(mock_applevz, get_state(_)).WillOnce(Return(applevz::AppleVMState::paused));

    vm.suspend();

    EXPECT_EQ(vm.current_state(), VirtualMachine::State::suspended);
}
}; // namespace multipass::test
