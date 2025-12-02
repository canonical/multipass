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

#include <applevz/applevz_wrapper.h>

namespace multipass::test
{
class MockAppleVZ : public multipass::applevz::AppleVZ
{
public:
    using AppleVZ::AppleVZ;
    MOCK_METHOD(applevz::CFError,
                create_vm,
                (const multipass::VirtualMachineDescription& desc,
                 multipass::applevz::VMHandle& out_handle),
                (const, override));
    MOCK_METHOD(applevz::CFError,
                start_vm,
                (multipass::applevz::VMHandle & vm_handle),
                (const, override));
    MOCK_METHOD(applevz::CFError,
                stop_vm,
                (bool force, multipass::applevz::VMHandle& vm_handle),
                (const, override));
};
} // namespace multipass::test
