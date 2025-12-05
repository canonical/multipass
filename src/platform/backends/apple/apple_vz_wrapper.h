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

#include <apple/apple_vz_bridge.h>
#include <apple/cf_error.h>

#include <multipass/singleton.h>
#include <multipass/virtual_machine_description.h>

#define MP_APPLE_VZ multipass::apple::AppleVZ::instance()

namespace multipass::apple
{
class AppleVZ : public Singleton<AppleVZ>
{
public:
    using Singleton<AppleVZ>::Singleton;

    virtual CFError create_vm(const VirtualMachineDescription& desc, VMHandle& out_handle) const;
    virtual CFError start_vm(VMHandle& vm_handle) const;
    virtual CFError stop_vm(bool force, VMHandle& vm_handle) const;
    virtual CFError pause_vm(VMHandle& vm_handle) const;
    virtual CFError resume_vm(VMHandle& vm_handle) const;

    virtual AppleVMState get_state(VMHandle& vm_handle) const;

    virtual bool can_start(VMHandle& vm_handle) const;
    virtual bool can_pause(VMHandle& vm_handle) const;
    virtual bool can_resume(VMHandle& vm_handle) const;
    virtual bool can_stop(VMHandle& vm_handle) const;
    virtual bool can_request_stop(VMHandle& vm_handle) const;
};
} // namespace multipass::apple
