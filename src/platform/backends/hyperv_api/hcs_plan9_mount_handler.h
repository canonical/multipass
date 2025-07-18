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

#include <hyperv_api/hyperv_api_wrapper_fwdecl.h>
#include <multipass/mount_handler.h>

namespace multipass::hyperv::hcs
{

class Plan9MountHandler : public MountHandler
{
public:
    Plan9MountHandler(VirtualMachine* vm,
                      const multipass::SSHKeyProvider* ssh_key_provider,
                      VMMount mount_spec,
                      const std::string& target,
                      hcs_sptr_t hcs_w);

    ~Plan9MountHandler() override;

private:
    void activate_impl(ServerVariant server, std::chrono::milliseconds timeout) override;
    void deactivate_impl(bool force) override;

    hcs_sptr_t hcs{nullptr};
};

} // namespace multipass::hyperv::hcs
