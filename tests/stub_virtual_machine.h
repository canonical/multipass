/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#ifndef MULTIPASS_STUB_VIRTUAL_MACHINE_H
#define MULTIPASS_STUB_VIRTUAL_MACHINE_H

#include <multipass/virtual_machine.h>

namespace multipass
{
namespace test
{
struct StubVirtualMachine final : public multipass::VirtualMachine
{
    StubVirtualMachine() : VirtualMachine{""}
    {
    }

    void start() override
    {
    }

    void stop(bool force = false) override
    {
    }

    void shutdown(bool force = false) override
    {
    }

    void suspend() override
    {
    }

    multipass::VirtualMachine::State current_state() override
    {
        return multipass::VirtualMachine::State::off;
    }

    int ssh_port() override
    {
        return 42;
    }

    std::string ssh_hostname(std::chrono::milliseconds) override
    {
        return "localhost";
    }

    std::string ssh_username() override
    {
        return "ubuntu";
    }

    std::string management_ipv4() override
    {
        return {};
    }

    std::vector<std::string> get_all_ipv4(const SSHKeyProvider& key_provider) override
    {
        return std::vector<std::string>{"192.168.2.123"};
    }

    std::string ipv6() override
    {
        return {};
    }

    void ensure_vm_is_running() override
    {
        throw std::runtime_error("Not running");
    }

    void wait_until_ssh_up(std::chrono::milliseconds) override
    {
    }

    void update_state() override
    {
    }
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_STUB_VIRTUAL_MACHINE_H
