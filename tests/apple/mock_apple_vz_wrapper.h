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

#pragma once

#include "tests/common.h"
#include "tests/mock_singleton_helpers.h"

#include <apple/apple_vz_wrapper.h>

namespace multipass::test
{
class MockAppleVZ : public multipass::apple::AppleVZ
{
public:
    using AppleVZ::AppleVZ;
    MOCK_METHOD(apple::CFError,
                create_vm,
                (const multipass::VirtualMachineDescription& desc,
                 multipass::apple::VMHandle& out_handle),
                (const, override));
    MOCK_METHOD(apple::CFError,
                start_vm,
                (const multipass::apple::VMHandle& vm_handle),
                (const, override));
    MOCK_METHOD(apple::CFError,
                stop_vm,
                (const multipass::apple::VMHandle& vm_handle, bool force),
                (const, override));
    MOCK_METHOD(apple::CFError,
                pause_vm,
                (const multipass::apple::VMHandle& vm_handle),
                (const, override));
    MOCK_METHOD(apple::CFError,
                resume_vm,
                (const multipass::apple::VMHandle& vm_handle),
                (const, override));
    MOCK_METHOD(apple::AppleVMState,
                get_state,
                (const multipass::apple::VMHandle& vm_handle),
                (const, override));
    MOCK_METHOD(bool, can_start, (const multipass::apple::VMHandle& vm_handle), (const, override));
    MOCK_METHOD(bool, can_pause, (const multipass::apple::VMHandle& vm_handle), (const, override));
    MOCK_METHOD(bool, can_resume, (const multipass::apple::VMHandle& vm_handle), (const, override));
    MOCK_METHOD(bool, can_stop, (const multipass::apple::VMHandle& vm_handle), (const, override));
    MOCK_METHOD(bool,
                can_request_stop,
                (const multipass::apple::VMHandle& vm_handle),
                (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockAppleVZ, AppleVZ);
};
} // namespace multipass::test
