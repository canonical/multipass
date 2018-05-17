/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#ifndef MULTIPASS_STUB_VIRTUAL_MACHINE_H
#define MULTIPASS_STUB_VIRTUAL_MACHINE_H

#include <multipass/virtual_machine.h>

#include "stub_ssh_key_provider.h"

namespace multipass
{
namespace test
{
struct StubVirtualMachine final : public multipass::VirtualMachine
{
    StubVirtualMachine() : VirtualMachine{StubSSHKeyProvider(), ""}
    {
    }

    void start() override
    {
    }

    void stop() override
    {
    }

    void shutdown() override
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

    std::string ssh_hostname() override
    {
        return "localhost";
    }

    std::string ssh_username() override
    {
        return "ubuntu";
    }

    std::string ipv4() override
    {
        return {};
    }

    std::string ipv6() override
    {
        return {};
    }

    void wait_until_ssh_up(std::chrono::milliseconds) override
    {
    }

    void wait_for_cloud_init(std::chrono::milliseconds) override
    {
    }
};
}
}
#endif // MULTIPASS_STUB_VIRTUAL_MACHINE_H
