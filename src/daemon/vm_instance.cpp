/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "vm_instance.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "daemon";
} // namespace

mp::VMInstance::VMInstance(const VirtualMachine::ShPtr vm, const SSHKeyProvider& key_provider)
    : vm_shptr(vm), key_provider(key_provider)
{
}

const multipass::VirtualMachine::ShPtr mp::VMInstance::vm() const
{
    return vm_shptr;
}

std::string mp::VMInstance::run_command(const std::string& cmd)
{
    mp::SSHSession session(vm_shptr->ssh_hostname(), vm_shptr->ssh_port(), vm_shptr->ssh_username(), key_provider);

    auto proc = session.exec(cmd);
    if (proc.exit_code() != 0)
    {
        auto error_msg = proc.read_std_error();
        mpl::log(mpl::Level::warning, category,
                 fmt::format("failed to run '{}', error message: '{}'", cmd, mp::utils::trim_end(error_msg)));
        return std::string{};
    }

    auto output = proc.read_std_output();
    if (output.empty())
    {
        mpl::log(mpl::Level::warning, category, fmt::format("no output after running '{}'", cmd));
        return std::string{};
    }

    return mp::utils::trim_end(output);
}
