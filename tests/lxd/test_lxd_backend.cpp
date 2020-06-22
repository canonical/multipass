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

#include <src/platform/backends/lxd/lxd_virtual_machine.h>
#include <src/platform/backends/lxd/lxd_virtual_machine_factory.h>

#include "mock_local_socket_reply.h"
#include "mock_lxd_server_responses.h"
#include "mock_network_access_manager.h"
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
    LXDBackend() : mock_network_access_manager{std::make_unique<NiceMock<mpt::MockNetworkAccessManager>>()}
    {
        mpl::set_logger(logger);

        EXPECT_CALL(*logger, log(Matcher<multipass::logging::Level>(_), Matcher<multipass::logging::CString>(_),
                                 Matcher<multipass::logging::CString>(_)))
            .WillRepeatedly(Return());
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
    std::unique_ptr<NiceMock<mpt::MockNetworkAccessManager>> mock_network_access_manager;
    QUrl base_url{"unix:///foo@1.0"};
};

struct LXDInstanceStatusTestSuite : LXDBackend, WithParamInterface<LXDInstanceStatusParamType>
{
};

const std::vector<LXDInstanceStatusParamType> lxd_instance_status_suite_inputs{
    {mpt::vm_state_stopped_data, mp::VirtualMachine::State::stopped},
    {mpt::vm_state_starting_data, mp::VirtualMachine::State::starting},
    {mpt::vm_state_freezing_data, mp::VirtualMachine::State::suspending},
    {mpt::vm_state_frozen_data, mp::VirtualMachine::State::suspended},
    {mpt::vm_state_cancelling_data, mp::VirtualMachine::State::unknown},
    {mpt::vm_state_other_data, mp::VirtualMachine::State::unknown},
    {mpt::vm_state_fully_running_data, mp::VirtualMachine::State::running}};
} // namespace

TEST_F(LXDBackend, creates_project_and_network_on_first_run)
{
    bool project_created{false};
    bool profile_updated{false};
    bool network_created{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&project_created, &profile_updated, &network_created](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && (url.contains("1.0/projects/multipass") || url.contains("1.0/networks/mpbr0")))
            {
                return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
            }
            else if (op == "POST" || op == "PUT")
            {
                if (url.contains("1.0/projects"))
                {
                    project_created = true;
                }
                else if (url.contains("1.0/profiles/default?project=multipass"))
                {
                    profile_updated = true;
                }
                else if (url.contains("1.0/networks"))
                {
                    network_created = true;
                }

                return new mpt::MockLocalSocketReply(mpt::post_no_error_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    EXPECT_TRUE(project_created);
    EXPECT_TRUE(profile_updated);
    EXPECT_TRUE(network_created);
}

TEST_F(LXDBackend, creates_in_stopped_state)
{
    mpt::StubVMStatusMonitor stub_monitor;

    bool vm_created{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&vm_created](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/operations/0020444c-2e4c-49d5-83ed-3275e3f6d005"))
                {
                    vm_created = true;
                }
                else if (vm_created && url.contains("1.0/virtual-machines/pied-piper-valley"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_info_data);
                }

                return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
            }
            else if (op == "POST" && url.contains("1.0/virtual-machines"))
            {
                return new mpt::MockLocalSocketReply(mpt::create_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description, stub_monitor, mock_network_access_manager.get(), base_url};

    EXPECT_TRUE(vm_created);
    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::stopped);
}

TEST_F(LXDBackend, machine_persists_and_sets_state_on_start)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley"))
            {
                if (url.contains("state"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_stopped_data);
                }
                else
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_info_data);
                }
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("start"))
            {
                return new mpt::MockLocalSocketReply(mpt::start_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description, mock_monitor, mock_network_access_manager.get(), base_url};

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine.start();

    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::starting);
}

TEST_F(LXDBackend, machine_persists_and_sets_state_on_shutdown)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    bool vm_shutdown{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&vm_shutdown](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/operations/b043d632-5c48-44b3-983c-a25660d61164"))
                {
                    vm_shutdown = true;
                    return new mpt::MockLocalSocketReply(mpt::vm_stop_wait_task_data);
                }
                else if (url.contains("1.0/virtual-machines/pied-piper-valley/state"))
                {
                    if (vm_shutdown)
                    {
                        return new mpt::MockLocalSocketReply(mpt::vm_state_stopped_data);
                    }
                    else
                    {
                        return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
                    }
                }
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("stop"))
            {
                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description, mock_monitor, mock_network_access_manager.get(), base_url};

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine.shutdown();

    EXPECT_TRUE(vm_shutdown);
    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::stopped);
}

TEST_F(LXDBackend, posts_expected_data_when_creating_instance)
{
    mpt::StubVMStatusMonitor stub_monitor;

    default_description.meta_data_config = YAML::Load("Luke: Jedi");
    default_description.user_data_config = YAML::Load("Vader: Sith");
    default_description.vendor_data_config = YAML::Load("Solo: Scoundrel");

    QByteArray expected_data{"{\n"
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
                             "}\n"};

    bool vm_created{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&vm_created, &expected_data](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/operations/0020444c-2e4c-49d5-83ed-3275e3f6d005"))
                {
                    vm_created = true;
                }
                else if (vm_created && url.contains("1.0/virtual-machines/pied-piper-valley"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_info_data);
                }

                return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
            }
            else if (op == "POST" && url.contains("1.0/virtual-machines"))
            {
                // This is the test to ensure the expected data
                EXPECT_EQ(data, expected_data);
                return new mpt::MockLocalSocketReply(mpt::create_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description, stub_monitor, mock_network_access_manager.get(), base_url};
}

TEST_F(LXDBackend, posts_expected_data_including_disk_size_when_creating_instance)
{
    mpt::StubVMStatusMonitor stub_monitor;

    default_description.meta_data_config = YAML::Load("Luke: Jedi");
    default_description.user_data_config = YAML::Load("Vader: Sith");
    default_description.vendor_data_config = YAML::Load("Solo: Scoundrel");
    default_description.disk_space = mp::MemorySize("16000000000");

    QByteArray expected_data{"{\n"
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
                             "            \"size\": \"16000000000\",\n"
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
                             "}\n"};

    bool vm_created{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&vm_created, &expected_data](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/operations/0020444c-2e4c-49d5-83ed-3275e3f6d005"))
                {
                    vm_created = true;
                }
                else if (vm_created && url.contains("1.0/virtual-machines/pied-piper-valley"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_info_data);
                }

                return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
            }
            else if (op == "POST" && url.contains("1.0/virtual-machines"))
            {
                // This is the test to ensure the expected data
                EXPECT_EQ(data, expected_data);
                return new mpt::MockLocalSocketReply(mpt::create_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description, stub_monitor, mock_network_access_manager.get(), base_url};
}

TEST_F(LXDBackend, prepare_source_image_does_not_modify)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET")
        {
            if (url.contains("1.0/projects/multipass"))
            {
                return new mpt::MockLocalSocketReply(mpt::project_info_data);
            }
            else if (url.contains("1.0/networks/mpbr0"))
            {
                return new mpt::MockLocalSocketReply(mpt::network_info_data);
            }
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};
    const mp::VMImage original_image{
        "/path/to/image", "", "", "deadbeef", "http://foo.bar", "bin", "baz", "the past", {"fee", "fi", "fo", "fum"}};

    auto source_image = backend.prepare_source_image(original_image);

    EXPECT_EQ(source_image.image_path, original_image.image_path);
    EXPECT_EQ(source_image.kernel_path, original_image.kernel_path);
    EXPECT_EQ(source_image.initrd_path, original_image.initrd_path);
    EXPECT_EQ(source_image.id, original_image.id);
    EXPECT_EQ(source_image.stream_location, original_image.stream_location);
    EXPECT_EQ(source_image.original_release, original_image.original_release);
    EXPECT_EQ(source_image.current_release, original_image.current_release);
    EXPECT_EQ(source_image.release_date, original_image.release_date);
    EXPECT_EQ(source_image.aliases, original_image.aliases);
}

TEST_F(LXDBackend, returns_expected_network_info)
{
    mpt::StubVMStatusMonitor stub_monitor;

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVirtualMachine machine{default_description, stub_monitor, mock_network_access_manager.get(), base_url};

    EXPECT_EQ(machine.ipv4(), "10.217.27.168");
    EXPECT_TRUE(machine.ipv6().empty());
    EXPECT_EQ(machine.ssh_username(), default_description.ssh_username);
    EXPECT_EQ(machine.ssh_port(), 22);
    EXPECT_EQ(machine.ssh_hostname(), "10.217.27.168");
}

TEST_F(LXDBackend, no_ip_address_returns_unknown)
{
    mpt::StubVMStatusMonitor stub_monitor;

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return new mpt::MockLocalSocketReply(mpt::vm_state_partial_running_data);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVirtualMachine machine{default_description, stub_monitor, mock_network_access_manager.get(), base_url};

    EXPECT_EQ(machine.ipv4(), "UNKNOWN");
}

TEST_F(LXDBackend, lxd_request_timeout_aborts_and_throws)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto...) {
        QByteArray data;
        auto reply = new mpt::MockLocalSocketReply(data);
        reply->setFinished(false);

        return reply;
    });

    EXPECT_THROW(mp::lxd_request(mock_network_access_manager.get(), "GET", base_url, mp::nullopt, 3),
                 std::runtime_error);
}

TEST_F(LXDBackend, lxd_request_invalid_json_throws)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();
        QByteArray invalid_json{"not json\r\n"};

        return new mpt::MockLocalSocketReply(invalid_json);
    });

    EXPECT_THROW(
        mp::LXDVirtualMachineFactory backend(std::move(mock_network_access_manager), data_dir.path(), base_url),
        std::runtime_error);
}

TEST_F(LXDBackend, lxd_request_wrong_json_throws)
{
    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();
        QByteArray invalid_json{"[]\r\n"};

        return new mpt::MockLocalSocketReply(invalid_json);
    });

    EXPECT_THROW(
        mp::LXDVirtualMachineFactory backend(std::move(mock_network_access_manager), data_dir.path(), base_url),
        std::runtime_error);
}

TEST_F(LXDBackend, unsupported_suspend_throws)
{
    mpt::StubVMStatusMonitor stub_monitor;

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVirtualMachine machine{default_description, stub_monitor, mock_network_access_manager.get(), base_url};

    EXPECT_THROW(machine.suspend(), std::runtime_error);
}

TEST_F(LXDBackend, start_while_suspending_throws)
{
    mpt::StubVMStatusMonitor stub_monitor;

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _)).WillByDefault([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return new mpt::MockLocalSocketReply(mpt::vm_state_freezing_data);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVirtualMachine machine{default_description, stub_monitor, mock_network_access_manager.get(), base_url};

    EXPECT_THROW(machine.start(), std::runtime_error);
}

TEST_F(LXDBackend, start_while_frozen_unfreezes)
{
    mpt::StubVMStatusMonitor stub_monitor;
    bool unfreeze_called{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&unfreeze_called](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                return new mpt::MockLocalSocketReply(mpt::vm_state_frozen_data);
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("unfreeze"))
            {
                unfreeze_called = true;
                return new mpt::MockLocalSocketReply(mpt::start_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description, stub_monitor, mock_network_access_manager.get(), base_url};

    EXPECT_CALL(*logger, log(Eq(mpl::Level::info), mpt::MockLogger::make_cstring_matcher(StrEq("pied-piper-valley")),
                             mpt::MockLogger::make_cstring_matcher(StrEq("Resuming from a suspended state"))));

    machine.start();

    EXPECT_TRUE(unfreeze_called);
}

TEST_F(LXDBackend, start_while_running_does_nothing)
{
    mpt::StubVMStatusMonitor stub_monitor;

    bool put_called{false};

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&put_called](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("start"))
            {
                put_called = true;
                return new mpt::MockLocalSocketReply(mpt::start_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description, stub_monitor, mock_network_access_manager.get(), base_url};

    ASSERT_EQ(machine.current_state(), mp::VirtualMachine::State::running);

    machine.start();

    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::running);
    EXPECT_FALSE(put_called);
}

TEST_P(LXDInstanceStatusTestSuite, lxd_state_returns_expected_VirtualMachine_state)
{
    mpt::StubVMStatusMonitor stub_monitor;

    const auto status_data = GetParam().first;
    const auto expected_state = GetParam().second;

    ON_CALL(*mock_network_access_manager.get(), createRequest(_, _, _))
        .WillByDefault([&status_data](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                return new mpt::MockLocalSocketReply(status_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description, stub_monitor, mock_network_access_manager.get(), base_url};

    EXPECT_EQ(machine.current_state(), expected_state);
}

INSTANTIATE_TEST_SUITE_P(LXDBackend, LXDInstanceStatusTestSuite, ValuesIn(lxd_instance_status_suite_inputs));
