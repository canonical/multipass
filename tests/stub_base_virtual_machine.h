/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#ifndef MULTIPASS_STUB_BASE_VIRTUAL_MACHINE_H
#define MULTIPASS_STUB_BASE_VIRTUAL_MACHINE_H

#include <shared/base_virtual_machine.h>

namespace multipass
{
namespace test
{
struct StubBaseVirtualMachine : public mp::BaseVirtualMachine
{
    StubBaseVirtualMachine() : mp::BaseVirtualMachine("stub")
    {
    }

    void stop()
    {
    }

    void start()
    {
    }

    void shutdown()
    {
    }

    void suspend()
    {
    }

    mp::VirtualMachine::State current_state()
    {
        return mp::VirtualMachine::State::running;
    }

    int ssh_port()
    {
        return 42;
    }

    std::string ssh_hostname(std::chrono::milliseconds timeout)
    {
        return "localhost";
    }

    std::string ssh_username()
    {
        return "ubuntu";
    }

    std::string management_ipv4()
    {
        return "1.2.3.4";
    }

    std::string ipv6()
    {
        return "";
    }

    void wait_until_ssh_up(std::chrono::milliseconds timeout)
    {
    }

    void ensure_vm_is_running()
    {
    }

    void update_state()
    {
    }
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_STUB_BASE_VIRTUAL_MACHINE_H
