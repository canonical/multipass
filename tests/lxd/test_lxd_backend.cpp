/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include <src/platform/backends/lxd/lxd_virtual_machine_factory.h>

#include "mock_lxd_server_responses.h"
#include "tests/local_socket_server_test_fixture.h"
#include "tests/mock_logger.h"
#include "tests/mock_status_monitor.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_dir.h"

#include <multipass/logging/log.h>
#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/version.h>
#include <multipass/virtual_machine_description.h>

#include <QString>
#include <QUrl>

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
using LXDInstanceStatusParamType = std::pair<QByteArray, mp::VirtualMachine::State>;

struct LXDBackend : public Test
{
    LXDBackend()
        : socket_path{QString("%1/test_socket").arg(data_dir.path())},
          test_server{mpt::MockLocalSocketServer(socket_path)},
          base_url{QString("unix://%1@1.0").arg(socket_path)}
    {
        mpl::set_logger(logger);
    }

    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used
                                                      "pied-piper-valley",
                                                      "",
                                                      "yoda",
                                                      {},
                                                      "",
                                                      {},
                                                      {},
                                                      {}};

    std::shared_ptr<NiceMock<mpt::MockLogger>> logger = std::make_shared<NiceMock<mpt::MockLogger>>();
    mpt::TempDir data_dir;
    QString socket_path;
    mpt::MockLocalSocketServer test_server;
    QUrl base_url;
};

struct LXDInstanceStatusTestSuite : LXDBackend, WithParamInterface<LXDInstanceStatusParamType>
{
};

const std::vector<LXDInstanceStatusParamType> lxd_instance_status_suite_inputs{
    {mpt::vm_state_stopped_response, mp::VirtualMachine::State::stopped},
    {mpt::vm_state_starting_response, mp::VirtualMachine::State::starting},
    {mpt::vm_state_freezing_response, mp::VirtualMachine::State::suspending},
    {mpt::vm_state_frozen_response, mp::VirtualMachine::State::suspended},
    {mpt::vm_state_cancelling_response, mp::VirtualMachine::State::unknown},
    {mpt::vm_state_other_response, mp::VirtualMachine::State::unknown},
    {mpt::vm_state_fully_running_response, mp::VirtualMachine::State::running}};
} // namespace

TEST_F(LXDBackend, creates_in_stopped_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    bool vm_created{false};

    auto server_handler = [&vm_created](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley"))
        {
            if (!vm_created)
            {
                return mpt::not_found_response;
            }
            else
            {
                return mpt::vm_created_response;
            }
        }
        else if (data.contains("POST") && data.contains("1.0/virtual-machines"))
        {
            return mpt::create_vm_response;
        }
        else if (data.contains("GET") && data.contains("1.0/operations/0020444c-2e4c-49d5-83ed-3275e3f6d005"))
        {
            vm_created = true;
            return mpt::not_found_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_TRUE(vm_created);
    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::stopped);
}

TEST_F(LXDBackend, machine_persists_and_sets_state_on_start)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    auto server_handler = [](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley"))
        {
            if (data.contains("state"))
            {
                return mpt::vm_state_stopped_response;
            }
            else
            {
                return mpt::vm_created_response;
            }
        }
        else if (data.contains("PUT") && data.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                 data.contains("start"))
        {
            return mpt::start_vm_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->start();

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::starting);
}

TEST_F(LXDBackend, machine_persists_and_sets_state_on_shutdown)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    bool vm_shutdown{false};

    auto server_handler = [&vm_shutdown](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley"))
        {
            if (data.contains("state") && !vm_shutdown)
            {
                return mpt::vm_state_fully_running_response;
            }
            else if (data.contains("state") && vm_shutdown)
            {
                return mpt::vm_state_stopped_response;
            }
            else
            {
                return mpt::vm_created_response;
            }
        }
        else if (data.contains("PUT") && data.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                 data.contains("stop"))
        {
            return mpt::stop_vm_response;
        }
        else if (data.contains("GET") && data.contains("1.0/operations/b043d632-5c48-44b3-983c-a25660d61164"))
        {
            vm_shutdown = true;
            return mpt::vm_stop_wait_task_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->shutdown();

    EXPECT_TRUE(vm_shutdown);
    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::stopped);
}

TEST_F(LXDBackend, posts_expected_data_when_creating_instance)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    default_description.meta_data_config = YAML::Load("Luke: Jedi");
    default_description.user_data_config = YAML::Load("Vader: Sith");
    default_description.vendor_data_config = YAML::Load("Solo: Scoundrel");

    QByteArray expected_data;
    expected_data += "POST /1.0/virtual-machines HTTP/1.1\r\n"
                     "Host: multipass\r\n"
                     "User-Agent: Multipass/";
    expected_data += mp::version_string;
    expected_data += "\r\n"
                     "Content-Type: application/x-www-form-urlencoded\r\n"
                     "Content-Length: 734\r\n\r\n"
                     "{\n"
                     "    \"config\": {\n"
                     "        \"limits.cpu\": \"2\",\n"
                     "        \"limits.memory\": \"3145728\",\n"
                     "        \"user.meta-data\": \"#cloud-config\\nLuke: Jedi\\n\\n\",\n"
                     "        \"user.user-data\": \"#cloud-config\\nVader: Sith\\n\\n\",\n"
                     "        \"user.vendor-data\": \"#cloud-config\\nSolo: Scoundrel\\n\\n\"\n"
                     "    },\n"
                     "    \"devices\": {\n"
                     "        \"config\": {\n"
                     "            \"source\": \"cloud-init:config\",\n"
                     "            \"type\": \"disk\"\n"
                     "        },\n"
                     "        \"root\": {\n"
                     "            \"path\": \"/\",\n"
                     "            \"pool\": \"default\",\n"
                     "            \"size\": \"10737418240\",\n"
                     "            \"type\": \"disk\"\n"
                     "        }\n"
                     "    },\n"
                     "    \"name\": \"pied-piper-valley\",\n"
                     "    \"source\": {\n"
                     "        \"fingerprint\": \"\",\n"
                     "        \"mode\": \"pull\",\n"
                     "        \"protocol\": \"simplestreams\",\n"
                     "        \"server\": \"\",\n"
                     "        \"type\": \"image\"\n"
                     "    }\n"
                     "}\n\r\n";

    bool vm_created{false};

    auto server_handler = [&vm_created, &expected_data](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley"))
        {
            if (!vm_created)
            {
                return mpt::not_found_response;
            }
            else
            {
                return mpt::vm_created_response;
            }
        }
        else if (data.contains("POST") && data.contains("1.0/virtual-machines"))
        {
            // This is the test to ensure the expected data
            EXPECT_EQ(data, expected_data);

            return mpt::create_vm_response;
        }
        else if (data.contains("GET") && data.contains("1.0/operations/0020444c-2e4c-49d5-83ed-3275e3f6d005"))
        {
            vm_created = true;
            return mpt::not_found_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);
}

TEST_F(LXDBackend, unimplemented_prepare_source_image_throws)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};
    mp::VMImage no_image;

    EXPECT_THROW(backend.prepare_source_image(no_image), std::runtime_error);
}

TEST_F(LXDBackend, unimplemented_prepare_instance_image_throws)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};
    mp::VMImage no_image;

    EXPECT_THROW(backend.prepare_instance_image(no_image, default_description), std::runtime_error);
}

TEST_F(LXDBackend, returns_expected_ipv4_address)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    auto server_handler = [](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return mpt::vm_state_fully_running_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_EQ(machine->ipv4(), "10.217.27.168");
}

TEST_F(LXDBackend, no_ip_address_returns_unknown)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    auto server_handler = [](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return mpt::vm_state_partial_running_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_EQ(machine->ipv4(), "UNKNOWN");
}

TEST_F(LXDBackend, returns_empty_ipv6_address)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    auto server_handler = [](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return mpt::vm_state_fully_running_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_TRUE(machine->ipv6().empty());
}

TEST_F(LXDBackend, returns_expected_ssh_username)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    auto server_handler = [](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return mpt::vm_state_fully_running_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_EQ(machine->ssh_username(), default_description.ssh_username);
}

TEST_F(LXDBackend, returns_ip_address_for_ssh_hostname)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    int tries{0};

    auto server_handler = [&tries](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            if (tries < 3)
            {
                tries++;
                return mpt::vm_state_partial_running_response;
            }

            return mpt::vm_state_fully_running_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    // This will take ~1 second to run since we want to cover the retry attempt
    EXPECT_EQ(machine->ssh_hostname(), "10.217.27.168");
    ASSERT_EQ(tries, 3);
}

TEST_F(LXDBackend, returns_expected_ssh_port)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    auto server_handler = [](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return mpt::vm_state_fully_running_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_EQ(machine->ssh_port(), 22);
}

TEST_F(LXDBackend, lxd_request_timeout_aborts_and_throws)
{
    auto manager = std::make_unique<mp::NetworkAccessManager>();

    EXPECT_THROW(mp::lxd_request(manager.get(), "GET", base_url, mp::nullopt, 3), std::runtime_error);
}

TEST_F(LXDBackend, lxd_request_invalid_json_throws)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    auto server_handler = [](auto data) -> QByteArray {
        QByteArray invalid_json_response{"HTTP/1.1 200 OK\r\n"
                                         "Content-Type: application/json\r\n\r\n"
                                         "not json\r\n"};

        return invalid_json_response;
    };

    test_server.local_socket_server_handler(server_handler);

    EXPECT_THROW(backend.create_virtual_machine(default_description, stub_monitor), std::runtime_error);
}

TEST_F(LXDBackend, lxd_request_wrong_json_throws)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    auto server_handler = [](auto data) -> QByteArray {
        QByteArray invalid_json_response{"HTTP/1.1 200 OK\r\n"
                                         "Content-Type: application/json\r\n\r\n"
                                         "[]\r\n"};

        return invalid_json_response;
    };

    test_server.local_socket_server_handler(server_handler);

    EXPECT_THROW(backend.create_virtual_machine(default_description, stub_monitor), std::runtime_error);
}

TEST_F(LXDBackend, suspend_while_stopped_keeps_same_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    auto server_handler = [](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return mpt::vm_state_stopped_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    ASSERT_EQ(machine->current_state(), mp::VirtualMachine::State::stopped);

    machine->suspend();

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::stopped);
}

TEST_F(LXDBackend, stop_while_suspended_keeps_same_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    auto server_handler = [](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return mpt::vm_state_frozen_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    ASSERT_EQ(machine->current_state(), mp::VirtualMachine::State::suspended);

    machine->stop();

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::suspended);
}

TEST_F(LXDBackend, start_while_running_does_nothing)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    bool put_called{false};

    auto server_handler = [&put_called](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return mpt::vm_state_fully_running_response;
        }
        else if (data.contains("PUT") && data.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                 data.contains("start"))
        {
            put_called = true;
            return mpt::start_vm_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    ASSERT_EQ(machine->current_state(), mp::VirtualMachine::State::running);

    machine->start();

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::running);
    EXPECT_FALSE(put_called);
}

TEST_P(LXDInstanceStatusTestSuite, lxd_state_returns_expected_VirtualMachine_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LXDVirtualMachineFactory backend{data_dir.path(), base_url};

    const auto status_response = GetParam().first;
    const auto expected_state = GetParam().second;

    auto server_handler = [&status_response](auto data) -> QByteArray {
        if (data.contains("GET") && data.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return status_response;
        }

        return mpt::not_found_response;
    };

    test_server.local_socket_server_handler(server_handler);

    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_EQ(machine->current_state(), expected_state);
}

INSTANTIATE_TEST_SUITE_P(LXDBackend, LXDInstanceStatusTestSuite, ValuesIn(lxd_instance_status_suite_inputs));
