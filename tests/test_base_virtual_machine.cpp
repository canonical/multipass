/*
 * Copyright (C) 2021-2022 Canonical, Ltd.
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

#include "common.h"
#include "dummy_ssh_key_provider.h"
#include "mock_ssh_test_fixture.h"

#include <shared/base_virtual_machine.h>

#include <multipass/exceptions/ssh_exception.h>
#include <multipass/ssh/ssh_session.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct StubBaseVirtualMachine : public mp::BaseVirtualMachine
{
    StubBaseVirtualMachine(const mp::VirtualMachine::State s = mp::VirtualMachine::State::off)
        : mp::BaseVirtualMachine("stub")
    {
        state = s;
    }

    void stop()
    {
        state = mp::VirtualMachine::State::off;
    }

    void start()
    {
        state = mp::VirtualMachine::State::running;
    }

    void shutdown()
    {
        state = mp::VirtualMachine::State::off;
    }

    void suspend()
    {
        state = mp::VirtualMachine::State::suspended;
    }

    mp::VirtualMachine::State current_state()
    {
        return state;
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

    void update_cpus(int num_cores) override
    {
    }

    void resize_memory(const mp::MemorySize&) override
    {
    }

    void resize_disk(const mp::MemorySize&) override
    {
    }
};

struct BaseVM : public Test
{
    mpt::MockSSHTestFixture mock_ssh_test_fixture;
    const mpt::DummyKeyProvider key_provider{"keeper of the seven keys"};
};

TEST_F(BaseVM, get_all_ipv4_works_when_ssh_throws_opening_a_session)
{
    StubBaseVirtualMachine base_vm(mp::VirtualMachine::State::running);

    REPLACE(ssh_new, []() { return nullptr; }); // This makes SSH throw when opening a new session.

    auto ip_list = base_vm.get_all_ipv4(key_provider);
    EXPECT_EQ(ip_list.size(), 0u);
}

TEST_F(BaseVM, get_all_ipv4_works_when_ssh_throws_executing)
{
    StubBaseVirtualMachine base_vm(mp::VirtualMachine::State::running);

    // Make SSH throw when trying to execute something.
    mock_ssh_test_fixture.request_exec.returnValue(SSH_ERROR);

    auto ip_list = base_vm.get_all_ipv4(key_provider);
    EXPECT_EQ(ip_list.size(), 0u);
}

TEST_F(BaseVM, get_all_ipv4_works_when_instance_is_off)
{
    StubBaseVirtualMachine base_vm(mp::VirtualMachine::State::off);

    EXPECT_EQ(base_vm.get_all_ipv4(key_provider).size(), 0u);
}

struct IpTestParams
{
    int exit_status;
    std::string output;
    std::vector<std::string> expected_ips;
};

struct IpExecution : public BaseVM, public WithParamInterface<IpTestParams>
{
};

TEST_P(IpExecution, get_all_ipv4_works_when_ssh_works)
{
    StubBaseVirtualMachine base_vm(mp::VirtualMachine::State::running);

    auto test_params = GetParam();
    auto remaining = test_params.output.size();

    ssh_channel_callbacks callbacks{nullptr};
    auto add_channel_cbs = [&callbacks](ssh_channel, ssh_channel_callbacks cb) {
        callbacks = cb;
        return SSH_OK;
    };
    REPLACE(ssh_add_channel_callbacks, add_channel_cbs);

    auto event_dopoll = [&callbacks, &test_params](ssh_event, int timeout) {
        EXPECT_TRUE(callbacks);
        callbacks->channel_exit_status_function(nullptr, nullptr, test_params.exit_status, callbacks->userdata);
        return SSH_OK;
    };
    REPLACE(ssh_event_dopoll, event_dopoll);

    auto channel_read = [&test_params, &remaining](ssh_channel, void* dest, uint32_t count, int is_stderr, int) {
        const auto num_to_copy = std::min(count, static_cast<uint32_t>(remaining));
        const auto begin = test_params.output.begin() + test_params.output.size() - remaining;
        std::copy_n(begin, num_to_copy, reinterpret_cast<char*>(dest));
        remaining -= num_to_copy;
        return num_to_copy;
    };
    REPLACE(ssh_channel_read_timeout, channel_read);

    auto ip_list = base_vm.get_all_ipv4(key_provider);
    EXPECT_EQ(ip_list, test_params.expected_ips);
}

INSTANTIATE_TEST_SUITE_P(
    BaseVM, IpExecution,
    Values(IpTestParams{0, "eth0             UP             192.168.2.168/24 \n", {"192.168.2.168"}},
           IpTestParams{0, "eth1             UP             192.168.2.169/24 metric 100 \n", {"192.168.2.169"}},
           IpTestParams{0,
                        "wlp4s0           UP             192.168.2.8/24 \n"
                        "virbr0           DOWN           192.168.3.1/24 \n"
                        "tun0             UNKNOWN        10.172.66.5/18 \n",
                        {"192.168.2.8", "192.168.3.1", "10.172.66.5"}},
           IpTestParams{0, "", {}}));

} // namespace
