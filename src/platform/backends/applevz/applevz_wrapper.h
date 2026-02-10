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

#include <applevz/applevz_bridge.h>
#include <applevz/cf_error.h>

#include <multipass/singleton.h>
#include <multipass/virtual_machine_description.h>

#define MP_APPLEVZ multipass::applevz::AppleVZ::instance()

namespace multipass::applevz
{
class AppleVZ : public Singleton<AppleVZ>
{
public:
    using Singleton<AppleVZ>::Singleton;

    virtual CFError create_vm(const VirtualMachineDescription& desc, VMHandle& out_handle) const;

    // Starting and stopping VM
    virtual CFError start_vm(const VMHandle& vm_handle) const;
    virtual CFError stop_vm(const VMHandle& vm_handle, bool force = false) const;
    virtual CFError pause_vm(const VMHandle& vm_handle) const;
    virtual CFError resume_vm(const VMHandle& vm_handle) const;

    // Getting VM state
    virtual AppleVMState get_state(const VMHandle& vm_handle) const;

    // Validate the state of VM
    virtual bool can_start(const VMHandle& vm_handle) const;
    virtual bool can_pause(const VMHandle& vm_handle) const;
    virtual bool can_resume(const VMHandle& vm_handle) const;
    virtual bool can_stop(const VMHandle& vm_handle) const;
    virtual bool can_request_stop(const VMHandle& vm_handle) const;

    virtual bool is_supported() const;
    virtual bool macos_at_least(int major, int minor, int patch = 0) const;
};
} // namespace multipass::applevz
