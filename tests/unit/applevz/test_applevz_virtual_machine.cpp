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

    mpt::MockAppleVZWrapper::GuardedMock mock_applevz_wrapper_injection{
        mpt::MockAppleVZWrapper::inject<NiceMock>()};
    mpt::MockAppleVZWrapper& mock_applevz = *mock_applevz_wrapper_injection.first;

    mpt::TempDir instance_dir;

    inline static auto mock_handle_raw =
        reinterpret_cast<mp::applevz::VirtualMachineHandle*>(0xbadf00d);
    mp::applevz::VMHandle mock_handle{mock_handle_raw, [](mp::applevz::VirtualMachineHandle*) {}};

    auto construct_vm(applevz::AppleVMState initial_state = applevz::AppleVMState::stopped)
    {
        EXPECT_CALL(mock_applevz, create_vm(_, _))
            .WillOnce(DoAll(SetArgReferee<1>(mock_handle), Return(applevz::CFError{})));

        EXPECT_CALL(mock_applevz, get_state(_)).WillOnce(Return(initial_state));
        EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, _)).Times(AnyNumber());

        return std::make_shared<mp::applevz::AppleVZVirtualMachine>(desc,
                                                                    mock_monitor,
                                                                    stub_key_provider,
                                                                    instance_dir.path());
    }
};

TEST_F(AppleVZVirtualMachine_UnitTests, startVMFromStoppedSuccess)
{
    auto uut = construct_vm();

    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::running));

    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::starting));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::running));

    EXPECT_CALL(mock_applevz, can_start(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, start_vm(_));

    uut->start();

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::running);
}

TEST_F(AppleVZVirtualMachine_UnitTests, startVMFromPausedSuccess)
{
    auto uut = construct_vm(applevz::AppleVMState::paused);

    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::running));

    EXPECT_CALL(mock_applevz, can_resume(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, resume_vm(_));

    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, mp::VirtualMachine::State::starting));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, mp::VirtualMachine::State::running));

    uut->start();

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::running);
}

TEST_F(AppleVZVirtualMachine_UnitTests, startVMFromRunningStateNoOp)
{
    auto uut = construct_vm(applevz::AppleVMState::running);

    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::running));

    EXPECT_CALL(mock_applevz, can_resume(_)).WillOnce(Return(false));
    EXPECT_CALL(mock_applevz, can_start(_)).WillOnce(Return(false));

    uut->start();

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::running);
}

TEST_F(AppleVZVirtualMachine_UnitTests, startVMFromStoppedThrowsError)
{
    auto uut = construct_vm();

    EXPECT_CALL(mock_applevz, get_state(_))
        .WillOnce(Return(applevz::AppleVMState::error))
        .WillOnce(Return(applevz::AppleVMState{}));

    EXPECT_CALL(mock_applevz, can_start(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, start_vm(_))
        .WillOnce(Return(applevz::CFError{
            CFErrorCreate(kCFAllocatorDefault, CFSTR("UnitTestDomain"), 42, nullptr)}));

    EXPECT_CALL(mock_monitor, persist_state_for(_, _)).Times(AtLeast(1));

    MP_EXPECT_THROW_THAT(uut->start(),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("failed to start")));
    EXPECT_EQ(uut->current_state(), VirtualMachine::State::unknown);
}

TEST_F(AppleVZVirtualMachine_UnitTests, startVMFromPausedThrowsError)
{
    auto uut = construct_vm(applevz::AppleVMState::paused);

    EXPECT_CALL(mock_applevz, get_state(_))
        .WillOnce(Return(applevz::AppleVMState::error))
        .WillOnce(Return(applevz::AppleVMState{}));

    EXPECT_CALL(mock_applevz, can_resume(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, resume_vm(_))
        .WillOnce(Return(applevz::CFError{
            CFErrorCreate(kCFAllocatorDefault, CFSTR("UnitTestDomain"), 42, nullptr)}));

    EXPECT_CALL(mock_monitor, persist_state_for(_, _)).Times(AtLeast(1));

    MP_EXPECT_THROW_THAT(uut->start(),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("failed to start")));
    EXPECT_EQ(uut->current_state(), VirtualMachine::State::unknown);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownVMFromRunningWithPowerdownSuccess)
{
    auto uut = construct_vm(applevz::AppleVMState::running);

    EXPECT_CALL(mock_applevz, get_state(_))
        .WillOnce(Return(applevz::AppleVMState::running))
        .WillOnce(Return(applevz::AppleVMState::stopping))
        .WillRepeatedly(Return(applevz::AppleVMState::stopped));

    EXPECT_CALL(mock_applevz, can_request_stop(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, stop_vm(_, false)).WillOnce(Return(applevz::CFError{}));

    uut->shutdown(VirtualMachine::ShutdownPolicy::Powerdown);

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::stopped);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownVMFromRunningWithPoweroffSuccess)
{
    auto uut = construct_vm(applevz::AppleVMState::running);

    EXPECT_CALL(mock_applevz, get_state(_))
        .WillOnce(Return(applevz::AppleVMState::running))
        .WillRepeatedly(Return(applevz::AppleVMState::stopped));

    EXPECT_CALL(mock_applevz, can_stop(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, stop_vm(_, true)).WillOnce(Return(applevz::CFError{}));

    uut->shutdown(VirtualMachine::ShutdownPolicy::Poweroff);

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::stopped);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownFromStoppedStateIsIdempotent)
{
    auto uut = construct_vm(applevz::AppleVMState::stopped);

    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::stopped));
    EXPECT_CALL(mock_applevz, stop_vm(_, _)).Times(0);

    uut->shutdown();

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::stopped);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownGracefulStopError)
{
    auto uut = construct_vm(applevz::AppleVMState::running);

    EXPECT_CALL(mock_applevz, get_state(_))
        .WillOnce(Return(applevz::AppleVMState::running))
        .WillRepeatedly(Return(applevz::AppleVMState::error));

    EXPECT_CALL(mock_applevz, can_request_stop(_)).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_applevz, stop_vm(_, false)).WillRepeatedly(Invoke([](auto&, bool) {
        return applevz::CFError{
            CFErrorCreate(kCFAllocatorDefault, CFSTR("TestDomain"), 123, nullptr)};
    }));

    MP_EXPECT_THROW_THAT(uut->shutdown(),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("failed to stop")));
    EXPECT_EQ(uut->current_state(), VirtualMachine::State::unknown);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownForcedStopError)
{
    auto uut = construct_vm(applevz::AppleVMState::running);

    EXPECT_CALL(mock_applevz, get_state(_)).WillOnce(Return(applevz::AppleVMState::running));

    EXPECT_CALL(mock_applevz, can_stop(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, stop_vm(_, true))
        .WillOnce(Return(applevz::CFError{
            CFErrorCreate(kCFAllocatorDefault, CFSTR("UnitTestDomain"), 42, nullptr)}));

    EXPECT_NO_THROW(uut->shutdown(VirtualMachine::ShutdownPolicy::Poweroff));
    EXPECT_EQ(uut->current_state(), VirtualMachine::State::stopped);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownCannotStopReturnsEarly)
{
    auto uut = construct_vm(applevz::AppleVMState::running);

    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::running));
    EXPECT_CALL(mock_applevz, can_request_stop(_)).WillOnce(Return(false));
    EXPECT_CALL(mock_applevz, stop_vm(_, _)).Times(0);

    uut->shutdown();

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::running);
}

TEST_F(AppleVZVirtualMachine_UnitTests, shutdownFromSuspendedStateWithHaltIsIdempotent)
{
    auto uut = construct_vm(applevz::AppleVMState::paused);

    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::paused));
    EXPECT_CALL(mock_applevz, stop_vm(_, _)).Times(0);

    EXPECT_NO_THROW(uut->shutdown(VirtualMachine::ShutdownPolicy::Halt));
    EXPECT_EQ(uut->current_state(), VirtualMachine::State::suspended);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendFromRunningStateCallsPauseVm)
{
    auto uut = construct_vm(applevz::AppleVMState::running);

    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::paused));

    EXPECT_CALL(mock_applevz, can_pause(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, pause_vm(_));

    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::suspending));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::suspended));

    uut->suspend();

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::suspended);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendFromStoppedStateReturnsEarly)
{
    auto uut = construct_vm(applevz::AppleVMState::stopped);

    EXPECT_CALL(mock_applevz, can_pause(_)).Times(AtLeast(1)).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_applevz, pause_vm(_)).Times(0);
    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::stopped));

    uut->suspend();

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::stopped);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendCannotPauseReturnsEarly)
{
    auto uut = construct_vm(applevz::AppleVMState::running);

    EXPECT_CALL(mock_applevz, can_pause(_)).Times(AtLeast(1)).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_applevz, pause_vm(_)).Times(0);
    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::running));

    uut->suspend();

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::running);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendErrorLogsWarning)
{
    auto uut = construct_vm(applevz::AppleVMState::running);

    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::error));

    EXPECT_CALL(mock_applevz, can_pause(_)).Times(AtLeast(1)).WillRepeatedly(Return(true));
    EXPECT_CALL(mock_applevz, can_request_stop(_)).Times(AtLeast(0)).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_applevz, pause_vm(_)).Times(AtLeast(1)).WillRepeatedly(Invoke([](auto&) {
        return applevz::CFError{
            CFErrorCreate(kCFAllocatorDefault, CFSTR("TestDomain"), 789, nullptr)};
    }));

    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::suspending));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::unknown));

    uut->suspend();

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::unknown);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendTransitionsToSuspendingState)
{
    auto uut = construct_vm(applevz::AppleVMState::running);

    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::paused));

    EXPECT_CALL(mock_applevz, can_pause(_)).WillOnce(Return(true));
    EXPECT_CALL(mock_applevz, pause_vm(_));

    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::suspending));
    EXPECT_CALL(mock_monitor, persist_state_for(desc.vm_name, VirtualMachine::State::suspended));

    uut->suspend();

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::suspended);
}

TEST_F(AppleVZVirtualMachine_UnitTests, suspendFromAlreadySuspendedStateIsIdempotent)
{
    auto uut = construct_vm(applevz::AppleVMState::paused);

    EXPECT_CALL(mock_applevz, can_pause(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(mock_applevz, pause_vm(_)).Times(0);
    EXPECT_CALL(mock_applevz, get_state(_)).WillRepeatedly(Return(applevz::AppleVMState::paused));

    uut->suspend();

    EXPECT_EQ(uut->current_state(), VirtualMachine::State::suspended);
}
}; // namespace multipass::test
