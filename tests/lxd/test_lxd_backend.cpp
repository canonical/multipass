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

#include "mock_local_socket_reply.h"
#include "mock_lxd_server_responses.h"
#include "mock_network_access_manager.h"

#include "tests/common.h"
#include "tests/mock_backend_utils.h"
#include "tests/mock_environment_helpers.h"
#include "tests/mock_logger.h"
#include "tests/mock_platform.h"
#include "tests/mock_status_monitor.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/stub_url_downloader.h"
#include "tests/temp_dir.h"

#include <src/platform/backends/lxd/lxd_virtual_machine.h>
#include <src/platform/backends/lxd/lxd_virtual_machine_factory.h>
#include <src/platform/backends/lxd/lxd_vm_image_vault.h>

#include <multipass/auto_join_thread.h>
#include <multipass/exceptions/local_socket_connection_exception.h>
#include <multipass/exceptions/start_exception.h>
#include <multipass/exceptions/virtual_machine_state_exceptions.h>
#include <multipass/format.h>
#include <multipass/memory_size.h>
#include <multipass/network_interface_info.h>
#include <multipass/virtual_machine_description.h>

#include <QJsonDocument>
#include <QString>
#include <QUrl>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;
using namespace std::chrono_literals;

namespace
{
using LXDInstanceStatusParamType = std::pair<QByteArray, mp::VirtualMachine::State>;
const QString bridge_name{"mpbr0"};

struct LXDBackend : public Test
{
    LXDBackend() : mock_network_access_manager{std::make_unique<StrictMock<mpt::MockNetworkAccessManager>>()}
    {
        logger_scope.mock_logger->screen_logs(mpl::Level::error);
    }

    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used
                                                      "pied-piper-valley",
                                                      "00:16:3e:fe:f2:b9",
                                                      {},
                                                      "yoda",
                                                      {},
                                                      "",
                                                      {},
                                                      {},
                                                      {},
                                                      {}};

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    mpt::TempDir data_dir;
    mpt::TempDir instance_dir;
    mpt::StubSSHKeyProvider key_provider;
    std::unique_ptr<StrictMock<mpt::MockNetworkAccessManager>> mock_network_access_manager;
    QUrl base_url{"unix:///foo@1.0"};
    const QString default_storage_pool{"default"};
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
    {mpt::vm_state_error_data, mp::VirtualMachine::State::unknown},
    {mpt::vm_state_other_data, mp::VirtualMachine::State::unknown},
    {mpt::vm_state_fully_running_data, mp::VirtualMachine::State::running}};
} // namespace

TEST_F(LXDBackend, createsProjectStoragePoolAndNetworkOnHealthcheck)
{
    bool project_created{false};
    bool network_created{false};
    bool storage_pool_created{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly(
            [&project_created, &network_created, &storage_pool_created](auto, auto request, auto outgoingData) {
                outgoingData->open(QIODevice::ReadOnly);
                auto data = outgoingData->readAll();
                auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
                auto url = request.url().toString();

                if (op == "GET")
                {
                    if ((url.contains("1.0/projects/multipass") || url.contains("1.0/storage-pools") ||
                         url.contains("1.0/networks/mpbr0")))
                    {
                        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
                    }
                    else if (url.contains("1.0"))
                    {
                        return new mpt::MockLocalSocketReply(mpt::lxd_server_info_data);
                    }
                }
                else if (op == "POST" || op == "PUT")
                {
                    if (url.contains("1.0/projects"))
                    {
                        const QByteArray expected_data{"{"
                                                       "\"description\":\"Project for Multipass instances\","
                                                       "\"name\":\"multipass\""
                                                       "}"};

                        EXPECT_EQ(data, expected_data);
                        project_created = true;
                    }
                    else if (url.contains("1.0/storage-pools"))
                    {
                        const QByteArray expected_data{"{"
                                                       "\"description\":\"Storage pool for Multipass\","
                                                       "\"driver\":\"dir\","
                                                       "\"name\":\"multipass\""
                                                       "}"};
                        EXPECT_EQ(data, expected_data);
                        storage_pool_created = true;
                    }
                    else if (url.contains("1.0/networks"))
                    {
                        const QByteArray expected_data{"{"
                                                       "\"description\":\"Network bridge for Multipass\","
                                                       "\"name\":\"mpbr0\"}"};

                        EXPECT_EQ(data, expected_data);
                        network_created = true;
                    }

                    return new mpt::MockLocalSocketReply(mpt::post_no_error_data);
                }

                return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
            });

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    backend.hypervisor_health_check();

    EXPECT_TRUE(project_created);
    EXPECT_TRUE(storage_pool_created);
    EXPECT_TRUE(network_created);
}

TEST_F(LXDBackend, usesDefaultStoragePoolWhenItExistsAndNoMultipassPool)
{
    bool multipass_pool_checked{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&multipass_pool_checked](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/storage-pools/default"))
                {
                    const QByteArray storage_pool_data{"{"
                                                       "\"error\": \"\","
                                                       "\"error_code\": 0,"
                                                       "\"metadata\": {"
                                                       "    \"name\": \"default\""
                                                       "},"
                                                       "\"operation\": \"\","
                                                       "\"status\": \"Success\","
                                                       "\"status_code\": 200,"
                                                       "\"type\": \"sync\""
                                                       "}"};

                    return new mpt::MockLocalSocketReply(storage_pool_data);
                }
                else if (url.contains("1.0/storage-pools/multipass"))
                {
                    multipass_pool_checked = true;

                    return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
                }
                else if (url.contains("1.0"))
                {
                    return new mpt::MockLocalSocketReply(mpt::lxd_server_info_data);
                }
            }
            else if (op == "POST" || op == "PUT")
            {
                return new mpt::MockLocalSocketReply(mpt::post_no_error_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    logger_scope.mock_logger->expect_log(mpl::Level::debug, "Using the \'default\' storage pool.");

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    backend.hypervisor_health_check();

    EXPECT_TRUE(multipass_pool_checked);
}

TEST_F(LXDBackend, usesMultipassStoragePoolWhenItExists)
{
    bool multipass_pool_returned{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&multipass_pool_returned](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/storage-pools/multipass"))
                {
                    multipass_pool_returned = true;

                    const QByteArray storage_pool_data{"{"
                                                       "\"error\": \"\","
                                                       "\"error_code\": 0,"
                                                       "\"metadata\": {"
                                                       "    \"name\": \"multipass\""
                                                       "},"
                                                       "\"operation\": \"\","
                                                       "\"status\": \"Success\","
                                                       "\"status_code\": 200,"
                                                       "\"type\": \"sync\""
                                                       "}"};

                    return new mpt::MockLocalSocketReply(storage_pool_data);
                }
                else if (url.contains("1.0/storage-pools/default"))
                {
                    return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
                }
                else if (url.contains("1.0"))
                {
                    return new mpt::MockLocalSocketReply(mpt::lxd_server_info_data);
                }
            }
            else if (op == "POST" || op == "PUT")
            {
                return new mpt::MockLocalSocketReply(mpt::post_no_error_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    logger_scope.mock_logger->expect_log(mpl::Level::debug, "Using the \'multipass\' storage pool.");

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    backend.hypervisor_health_check();

    EXPECT_TRUE(multipass_pool_returned);
}

TEST_F(LXDBackend, usesMultipassPoolWhenDefaultPoolExists)
{
    bool default_pool_returned{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&default_pool_returned](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/storage-pools/multipass"))
                {
                    const QByteArray storage_pool_data{"{"
                                                       "\"error\": \"\","
                                                       "\"error_code\": 0,"
                                                       "\"metadata\": {"
                                                       "    \"name\": \"multipass\""
                                                       "},"
                                                       "\"operation\": \"\","
                                                       "\"status\": \"Success\","
                                                       "\"status_code\": 200,"
                                                       "\"type\": \"sync\""
                                                       "}"};

                    return new mpt::MockLocalSocketReply(storage_pool_data);
                }
                else if (url.contains("1.0/storage-pools/default"))
                {
                    default_pool_returned = true;
                    const QByteArray storage_pool_data{"{"
                                                       "\"error\": \"\","
                                                       "\"error_code\": 0,"
                                                       "\"metadata\": {"
                                                       "    \"name\": \"default\""
                                                       "},"
                                                       "\"operation\": \"\","
                                                       "\"status\": \"Success\","
                                                       "\"status_code\": 200,"
                                                       "\"type\": \"sync\""
                                                       "}"};

                    return new mpt::MockLocalSocketReply(storage_pool_data);
                }
                else if (url.contains("1.0"))
                {
                    return new mpt::MockLocalSocketReply(mpt::lxd_server_info_data);
                }
            }
            else if (op == "POST" || op == "PUT")
            {
                return new mpt::MockLocalSocketReply(mpt::post_no_error_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    logger_scope.mock_logger->expect_log(mpl::Level::debug, "Using the \'multipass\' storage pool.");

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    backend.hypervisor_health_check();
}

TEST_F(LXDBackend, factory_creates_valid_virtual_machine_ptr)
{
    mpt::StubVMStatusMonitor stub_monitor;

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("stop"))
            {
                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    EXPECT_NE(nullptr, machine);
}

TEST_F(LXDBackend, factory_creates_expected_image_vault)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mpt::StubURLDownloader stub_downloader;
    mpt::TempDir cache_dir;
    mpt::TempDir data_dir;
    std::vector<mp::VMImageHost*> hosts;

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    auto vault = backend.create_image_vault(hosts, &stub_downloader, cache_dir.path(), data_dir.path(), mp::days{0});

    EXPECT_TRUE(dynamic_cast<mp::LXDVMImageVault*>(vault.get()));
}

TEST_F(LXDBackend, factory_does_nothing_on_configure)
{
    mpt::TempDir data_dir;
    mp::VirtualMachineDescription vm_desc{default_description};

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    backend.configure(vm_desc);

    EXPECT_TRUE(vm_desc.cloud_init_iso.isEmpty());
}

TEST_F(LXDBackend, creates_in_stopped_state)
{
    mpt::StubVMStatusMonitor stub_monitor;

    bool vm_created{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&vm_created](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/operations/0020444c-2e4c-49d5-83ed-3275e3f6d005"))
                {
                    vm_created = true;
                    return new mpt::MockLocalSocketReply(mpt::create_vm_finished_data);
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

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    EXPECT_TRUE(vm_created);
    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::stopped);
}

TEST_F(LXDBackend, machine_persists_and_sets_state_on_start)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    bool start_called{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&start_called](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley"))
            {
                if (url.contains("state"))
                {
                    if (!start_called)
                    {
                        return new mpt::MockLocalSocketReply(mpt::vm_state_stopped_data);
                    }
                    else
                    {
                        return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
                    }
                }
                else
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_info_data);
                }
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("start"))
            {
                start_called = true;
                return new mpt::MockLocalSocketReply(mpt::start_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description,
                                  mock_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    EXPECT_CALL(mock_monitor, persist_state_for(_, _)).Times(2);
    machine.start();

    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::starting);
}

TEST_F(LXDBackend, machine_persists_and_sets_state_on_shutdown)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    bool vm_shutdown{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&vm_shutdown](auto, auto request, auto outgoingData) {
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

    mp::LXDVirtualMachine machine{default_description,
                                  mock_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    EXPECT_CALL(mock_monitor, persist_state_for(_, _)).Times(2);
    machine.shutdown();

    EXPECT_TRUE(vm_shutdown);
    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::stopped);
}

TEST_F(LXDBackend, machine_persists_internal_stopped_state_on_destruction)
{
    mpt::MockVMStatusMonitor mock_monitor;

    bool vm_created{false};
    auto vm_instance_state{mp::VirtualMachine::State::off};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&vm_created](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                if (!vm_created)
                {
                    vm_created = true;
                    return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
                }
                else
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_stopped_data);
                }
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    EXPECT_CALL(mock_monitor, persist_state_for(_, _)).WillRepeatedly([&vm_instance_state](auto, auto state) {
        vm_instance_state = state;
    });

    {
        mp::LXDVirtualMachine machine{default_description,
                                      mock_monitor,
                                      mock_network_access_manager.get(),
                                      base_url,
                                      bridge_name,
                                      default_storage_pool,
                                      key_provider,
                                      instance_dir.path()};

        ASSERT_EQ(machine.state, mp::VirtualMachine::State::running);
    } // Simulate multipass exiting by having the vm destruct

    EXPECT_TRUE(vm_created);
    EXPECT_EQ(vm_instance_state, mp::VirtualMachine::State::stopped);
}

TEST_F(LXDBackend, machine_does_not_update_state_in_dtor)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    bool vm_shutdown{false}, stop_requested{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&vm_shutdown, &stop_requested](auto, auto request, auto outgoingData) {
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
                stop_requested = true;
                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    EXPECT_CALL(mock_monitor, persist_state_for(_, _)).Times(0);

    // create in its own scope so the dtor is called
    {
        mp::LXDVirtualMachine machine{default_description,
                                      mock_monitor,
                                      mock_network_access_manager.get(),
                                      base_url,
                                      bridge_name,
                                      default_storage_pool,
                                      key_provider,
                                      instance_dir.path()};
    }

    EXPECT_TRUE(vm_shutdown);
    EXPECT_TRUE(stop_requested);
}

TEST_F(LXDBackend, machineLogsNotFoundExceptionInDtor)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    bool vm_shutdown{false}, stop_requested{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&vm_shutdown, &stop_requested](auto, auto request, auto outgoingData) {
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
                        throw mp::LXDNotFoundException();
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
                stop_requested = true;
                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::debug), mpt::MockLogger::make_cstring_matcher(StrEq("pied-piper-valley")),
                    mpt::MockLogger::make_cstring_matcher(StrEq("LXD object not found"))));
    EXPECT_CALL(mock_monitor, persist_state_for(_, _));

    // create in its own scope so the dtor is called
    {
        mp::LXDVirtualMachine machine{default_description,
                                      mock_monitor,
                                      mock_network_access_manager.get(),
                                      base_url,
                                      bridge_name,
                                      default_storage_pool,
                                      key_provider,
                                      instance_dir.path()};
        machine.shutdown();
    }

    EXPECT_TRUE(vm_shutdown);
    EXPECT_TRUE(stop_requested);
}

TEST_F(LXDBackend, does_not_call_stop_when_snap_refresh_is_detected)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    bool stop_requested{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&stop_requested](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/virtual-machines/pied-piper-valley/state"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
                }
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("stop"))
            {
                stop_requested = true;
                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    QTemporaryDir common_dir;
    mpt::SetEnvScope env("SNAP_COMMON", common_dir.path().toUtf8());
    mpt::SetEnvScope env2("SNAP_NAME", "multipass");
    QFile refresh_file{common_dir.path() + "/snap_refresh"};
    refresh_file.open(QIODevice::WriteOnly);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _)).Times(0);

    // create in its own scope so the dtor is called
    {
        mp::LXDVirtualMachine machine{default_description,
                                      mock_monitor,
                                      mock_network_access_manager.get(),
                                      base_url,
                                      bridge_name,
                                      default_storage_pool,
                                      key_provider,
                                      instance_dir.path()};
    }

    EXPECT_FALSE(stop_requested);
}

TEST_F(LXDBackend, calls_stop_when_snap_refresh_does_not_exist)
{
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;

    bool stop_requested{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&stop_requested](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/virtual-machines/pied-piper-valley/state"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
                }
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("stop"))
            {
                stop_requested = true;
                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    QTemporaryDir common_dir;
    mpt::SetEnvScope env("SNAP_COMMON", common_dir.path().toUtf8());
    mpt::SetEnvScope env2("SNAP_NAME", "multipass");

    EXPECT_CALL(mock_monitor, persist_state_for(_, _)).Times(0);

    // create in its own scope so the dtor is called
    {
        mp::LXDVirtualMachine machine{default_description,
                                      mock_monitor,
                                      mock_network_access_manager.get(),
                                      base_url,
                                      bridge_name,
                                      default_storage_pool,
                                      key_provider,
                                      instance_dir.path()};
    }

    EXPECT_TRUE(stop_requested);
}

TEST_F(LXDBackend, posts_expected_data_when_creating_instance)
{
    mpt::StubVMStatusMonitor stub_monitor;

    default_description.meta_data_config = YAML::Load("Luke: Jedi");
    default_description.user_data_config = YAML::Load("Vader: Sith");
    default_description.vendor_data_config = YAML::Load("Solo: Scoundrel");
    default_description.disk_space = mp::MemorySize("16000000000");

    QByteArray expected_data{"{"
                             "\"config\":{"
                             "\"limits.cpu\":\"2\","
                             "\"limits.memory\":\"3145728\","
                             "\"security.secureboot\":\"false\","
                             "\"user.meta-data\":\"#cloud-config\\nLuke: Jedi\\n\","
                             "\"user.user-data\":\"#cloud-config\\nVader: Sith\\n\","
                             "\"user.vendor-data\":\"#cloud-config\\nSolo: Scoundrel\\n\""
                             "},"
                             "\"devices\":{"
                             "\"config\":{"
                             "\"source\":\"cloud-init:config\","
                             "\"type\":\"disk\""
                             "},"
                             "\"eth0\":{"
                             "\"hwaddr\":\"00:16:3e:fe:f2:b9\","
                             "\"name\":\"eth0\","
                             "\"nictype\":\"bridged\","
                             "\"parent\":\"mpbr0\","
                             "\"type\":\"nic\""
                             "},"
                             "\"root\":{"
                             "\"path\":\"/\","
                             "\"pool\":\"default\","
                             "\"size\":\"16000000000\","
                             "\"type\":\"disk\""
                             "}"
                             "},"
                             "\"name\":\"pied-piper-valley\","
                             "\"source\":{"
                             "\"fingerprint\":\"\","
                             "\"type\":\"image\""
                             "}"
                             "}"};

    bool vm_created{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&vm_created, &expected_data](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/operations/0020444c-2e4c-49d5-83ed-3275e3f6d005"))
                {
                    vm_created = true;
                    return new mpt::MockLocalSocketReply(mpt::create_vm_finished_data);
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

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};
}

TEST_F(LXDBackend, prepare_source_image_does_not_modify)
{
    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};
    const mp::VMImage original_image{"/path/to/image",          "deadbeef", "bin", "baz", "the past",
                                     {"fee", "fi", "fo", "fum"}};

    auto source_image = backend.prepare_source_image(original_image);

    EXPECT_EQ(source_image.image_path, original_image.image_path);
    EXPECT_EQ(source_image.id, original_image.id);
    EXPECT_EQ(source_image.original_release, original_image.original_release);
    EXPECT_EQ(source_image.current_release, original_image.current_release);
    EXPECT_EQ(source_image.release_date, original_image.release_date);
    EXPECT_EQ(source_image.aliases, original_image.aliases);
}

TEST_F(LXDBackend, returns_expected_backend_string)
{
    const QByteArray server_data{"{"
                                 "\"type\": \"sync\","
                                 "\"status\": \"Success\","
                                 "\"status_code\": 200,"
                                 "\"operation\": \"\","
                                 "\"error_code\": 0,"
                                 "\"error\": \"\","
                                 "\"metadata\": {"
                                 "  \"config\": {},"
                                 "  \"api_status\": \"stable\","
                                 "  \"api_version\": \"1.0\","
                                 "  \"auth\": \"untrusted\","
                                 "  \"public\": false,"
                                 "  \"auth_methods\": ["
                                 "    \"tls\""
                                 "    ],"
                                 "  \"environment\": {"
                                 "    \"server_version\": \"4.3\""
                                 "    }"
                                 "  }"
                                 "}\n"};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&server_data](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0"))
            {
                return new mpt::MockLocalSocketReply(server_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    const QString backend_string{"lxd-4.3"};

    EXPECT_EQ(backend.get_backend_version_string(), backend_string);
}

TEST_F(LXDBackend, unimplemented_functions_logs_trace_message)
{
    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    const std::string name{"foo"};

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace),
                    mpt::MockLogger::make_cstring_matcher(StrEq("lxd factory")),
                    mpt::MockLogger::make_cstring_matcher(
                        StrEq(fmt::format("No further resources to remove for \"{}\"", name)))));

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::trace), mpt::MockLogger::make_cstring_matcher(StrEq("lxd factory")),
                    mpt::MockLogger::make_cstring_matcher(StrEq("No driver preparation for instance image"))));

    mp::VMImage image;
    YAML::Node node;

    backend.remove_resources_for(name);
    backend.prepare_instance_image(image, default_description);
}

TEST_F(LXDBackend, factoryDoesNotSupportSuspend)
{
    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};
    MP_EXPECT_THROW_THAT(backend.require_suspend_support(),
                         mp::NotImplementedOnThisBackendException,
                         mpt::match_what(HasSubstr("suspend")));
}

TEST_F(LXDBackend, image_fetch_type_returns_expected_type)
{
    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    EXPECT_EQ(backend.fetch_type(), mp::FetchType::ImageOnly);
}

TEST_F(LXDBackend, healthcheck_throws_when_untrusted)
{
    const QByteArray untrusted_data{"{"
                                    "\"type\": \"sync\","
                                    "\"status\": \"Success\","
                                    "\"status_code\": 200,"
                                    "\"operation\": \"\","
                                    "\"error_code\": 0,"
                                    "\"error\": \"\","
                                    "\"metadata\": {"
                                    "  \"config\": {},"
                                    "  \"api_status\": \"stable\","
                                    "  \"api_version\": \"1.0\","
                                    "  \"auth\": \"untrusted\","
                                    "  \"public\": false,"
                                    "  \"auth_methods\": ["
                                    "    \"tls\""
                                    "    ]"
                                    "  }"
                                    "}\n"};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&untrusted_data](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0"))
            {
                return new mpt::MockLocalSocketReply(untrusted_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    MP_EXPECT_THROW_THAT(backend.hypervisor_health_check(), std::runtime_error,
                         mpt::match_what(StrEq("Failed to authenticate to LXD.")));
}

TEST_F(LXDBackend, healthcheck_connection_refused_error_throws_with_expected_message)
{
    const std::string exception_message{"Connection refused"};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillOnce([&exception_message](auto...) -> QNetworkReply* {
            throw mp::LocalSocketConnectionException(exception_message);
        });

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    MP_EXPECT_THROW_THAT(backend.hypervisor_health_check(), std::runtime_error,
                         mpt::match_what(StrEq(fmt::format("{}\n\nPlease ensure the LXD snap is installed and enabled.",
                                                           exception_message))));
}

TEST_F(LXDBackend, healthcheck_unknown_server_error_throws_with_expected_message)
{
    const std::string exception_message{"Unknown server"};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillOnce([&exception_message](auto...) -> QNetworkReply* {
            throw mp::LocalSocketConnectionException(exception_message);
        });

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    MP_EXPECT_THROW_THAT(backend.hypervisor_health_check(), std::runtime_error,
                         mpt::match_what(StrEq(fmt::format("{}\n\nPlease ensure the LXD snap is installed and enabled.",
                                                           exception_message))));
}

TEST_F(LXDBackend, healthcheck_error_advises_snap_connections_when_in_snap)
{
    EXPECT_CALL(*mock_network_access_manager, createRequest).WillOnce(Throw(mp::LocalSocketConnectionException("")));

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    mpt::SetEnvScope env{"SNAP_NAME", "multipass"};
    MP_EXPECT_THROW_THAT(backend.hypervisor_health_check(), std::runtime_error,
                         mpt::match_what(HasSubstr("snap connect multipass:lxd lxd")));
}

struct LXDNetworkInfoSuite : LXDBackend, WithParamInterface<QByteArray>
{
};

TEST_P(LXDNetworkInfoSuite, returns_expected_network_info)
{
    const auto leases_data = GetParam();

    mpt::StubVMStatusMonitor stub_monitor;

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&leases_data](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/virtual-machines/pied-piper-valley/state"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
                }
                else if (url.contains("1.0/networks/" + bridge_name + "/leases"))
                {
                    return new mpt::MockLocalSocketReply(leases_data);
                }
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("stop"))
            {
                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    EXPECT_EQ(machine.management_ipv4(), "10.217.27.168");
    EXPECT_TRUE(machine.ipv6().empty());
    EXPECT_EQ(machine.ssh_username(), default_description.ssh_username);
    EXPECT_EQ(machine.ssh_port(), 22);
    EXPECT_EQ(machine.VirtualMachine::ssh_hostname(), "10.217.27.168");
}

INSTANTIATE_TEST_SUITE_P(LXDBackend, LXDNetworkInfoSuite,
                         Values(mpt::network_leases_data, mpt::network_leases_data_with_ipv6,
                                mpt::network_leases_data_with_others));

TEST_F(LXDBackend, ssh_hostname_timeout_throws_and_sets_unknown_state)
{
    mpt::StubVMStatusMonitor stub_monitor;

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/virtual-machines/pied-piper-valley/state"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
                }
                else if (url.contains("1.0/networks/" + bridge_name + "/leases"))
                {
                    return new mpt::MockLocalSocketReply(mpt::network_no_leases_data);
                }
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("stop"))
            {
                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    EXPECT_THROW(machine.ssh_hostname(std::chrono::milliseconds(1)), std::runtime_error);
    EXPECT_EQ(machine.state, mp::VirtualMachine::State::unknown);
}

TEST_F(LXDBackend, no_ip_address_returns_unknown)
{
    mpt::StubVMStatusMonitor stub_monitor;

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/virtual-machines/pied-piper-valley/state"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_partial_running_data);
                }
                else if (url.contains("1.0/networks/" + bridge_name + "/leases"))
                {
                    return new mpt::MockLocalSocketReply(mpt::network_no_leases_data);
                }
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("stop"))
            {
                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    EXPECT_EQ(machine.management_ipv4(), "UNKNOWN");
}

TEST_F(LXDBackend, lxd_request_timeout_aborts_and_throws)
{
    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([](auto...) {
        QByteArray data;
        auto reply = new mpt::MockLocalSocketReply(data);
        reply->setFinished(false);

        return reply;
    });

    base_url.setHost("test");

    const std::string op{"GET"};
    const std::string error_string{
        fmt::format("Timeout getting response for {} operation on {}", op, base_url.toString().toStdString())};

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::error), mpt::MockLogger::make_cstring_matcher(StrEq("lxd request")),
                    mpt::MockLogger::make_cstring_matcher(HasSubstr(error_string))));

    MP_EXPECT_THROW_THAT(mp::lxd_request(mock_network_access_manager.get(), op, base_url, std::nullopt, 3),
                         std::runtime_error, mpt::match_what(HasSubstr(error_string)));
}

TEST_F(LXDBackend, lxd_request_empty_data_returned_throws_and_logs)
{
    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([](auto...) {
        QByteArray data;
        auto reply = new mpt::MockLocalSocketReply(data);

        return reply;
    });

    base_url.setHost("test");

    const std::string op{"GET"};
    const std::string error_string{
        fmt::format("Empty reply received for {} operation on {}", op, base_url.toString().toStdString())};

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::error), mpt::MockLogger::make_cstring_matcher(StrEq("lxd request")),
                    mpt::MockLogger::make_cstring_matcher(HasSubstr(error_string))));

    MP_EXPECT_THROW_THAT(mp::lxd_request(mock_network_access_manager.get(), op, base_url), std::runtime_error,
                         mpt::match_what(HasSubstr(error_string)));
}

TEST_F(LXDBackend, lxd_request_invalid_json_throws_and_logs)
{
    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();
        QByteArray invalid_json{"not json\r\n"};

        return new mpt::MockLocalSocketReply(invalid_json);
    });

    base_url.setHost("test");

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::debug), mpt::MockLogger::make_cstring_matcher(StrEq("lxd request")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr(base_url.toString().toStdString()), HasSubstr("illegal value")))));

    MP_EXPECT_THROW_THAT(
        mp::lxd_request(mock_network_access_manager.get(), "GET", base_url), std::runtime_error,
        mpt::match_what(AllOf(HasSubstr(base_url.toString().toStdString()), HasSubstr("illegal value"))));
}

TEST_F(LXDBackend, lxd_request_wrong_json_throws_and_logs)
{
    QByteArray invalid_json{"[]\r\n"};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillOnce([&invalid_json](auto, auto request, auto) {
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            return new mpt::MockLocalSocketReply(invalid_json);
        });

    base_url.setHost("test");

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::debug), mpt::MockLogger::make_cstring_matcher(StrEq("lxd request")),
                    mpt::MockLogger::make_cstring_matcher(
                        AllOf(HasSubstr(base_url.toString().toStdString()), HasSubstr(invalid_json.toStdString())))));

    MP_EXPECT_THROW_THAT(mp::lxd_request(mock_network_access_manager.get(), "GET", base_url), std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr(base_url.toString().toStdString()))));
}

TEST_F(LXDBackend, lxd_request_bad_request_throws_and_logs)
{
    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([](auto...) {
        const QByteArray error_data{"{"
                                    "\"type\": \"error\","
                                    "\"error\": \"Failure\","
                                    "\"error_code\": 400,"
                                    "\"metadata\": {}"
                                    "}"};

        return new mpt::MockLocalSocketReply(error_data, QNetworkReply::ProtocolInvalidOperationError);
    });

    base_url.setHost("test");

    auto error_matcher = AllOf(HasSubstr("Network error for"), HasSubstr(base_url.toString().toStdString()),
                               HasSubstr(": Error - Failure"));

    EXPECT_CALL(*logger_scope.mock_logger,
                log(_,
                    mpt::MockLogger::make_cstring_matcher(StrEq("lxd request")),
                    mpt::MockLogger::make_cstring_matcher(error_matcher)));

    MP_EXPECT_THROW_THAT(mp::lxd_request(mock_network_access_manager.get(), "GET", base_url), std::runtime_error,
                         mpt::match_what(error_matcher));
}

TEST_F(LXDBackend, lxd_request_multipart_bbad_request_throws_and_logs)
{
    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([](auto...) {
        const QByteArray error_data{"{"
                                    "\"type\": \"error\","
                                    "\"error\": \"Failure\","
                                    "\"error_code\": 400,"
                                    "\"metadata\": {}"
                                    "}"};

        return new mpt::MockLocalSocketReply(error_data, QNetworkReply::ProtocolInvalidOperationError);
    });

    base_url.setHost("test");

    auto error_matcher = AllOf(HasSubstr("Network error for"), HasSubstr(base_url.toString().toStdString()),
                               HasSubstr(": Error - Failure"));
    QHttpMultiPart stub_multipart;

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::error), mpt::MockLogger::make_cstring_matcher(StrEq("lxd request")),
                    mpt::MockLogger::make_cstring_matcher(error_matcher)));

    MP_EXPECT_THROW_THAT(mp::lxd_request(mock_network_access_manager.get(), "GET", base_url, stub_multipart),
                         std::runtime_error, mpt::match_what(error_matcher));
}

TEST_F(LXDBackend, lxd_wait_error_returned_throws_and_logs)
{
    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET" && url.contains("1.0/operations/b043d632-5c48-44b3-983c-a25660d61164/wait"))
        {
            const QByteArray wait_reply_error{"{"
                                              "\"error\": \"Failure\","
                                              "\"error_code\": 400,"
                                              "\"metadata\": {"
                                              "  \"class\": \"task\","
                                              "  \"created_at\": \"2020-11-10T11:42:58.996868033-05:00\","
                                              "  \"description\": \"Stopping container\","
                                              "  \"err\": \"\","
                                              "  \"id\": \"b043d632-5c48-44b3-983c-a25660d61164\","
                                              "  \"location\": \"none\","
                                              "  \"may_cancel\": false,"
                                              "  \"metadata\": null,"
                                              "  \"resources\": {"
                                              "    \"containers\": ["
                                              "      \"/1.0/containers/test\""
                                              "    ]"
                                              "  },"
                                              "  \"status\": \"Success\","
                                              "  \"status_code\": 200,"
                                              "  \"updated_at\": \"2020-11-10T11:42:58.996868033-05:00\""
                                              "},"
                                              "\"operation\": \"\","
                                              "\"status\": \"\","
                                              "\"status_code\": 0,"
                                              "\"type\": \"sync\""
                                              "}"};

            return new mpt::MockLocalSocketReply(wait_reply_error);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    base_url.setHost("test");

    QJsonParseError json_error;
    auto json_reply = QJsonDocument::fromJson(mpt::stop_vm_data, &json_error);

    auto error_matcher = StrEq("Error waiting on operation: (400) Failure");

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::error), mpt::MockLogger::make_cstring_matcher(StrEq("lxd request")),
                    mpt::MockLogger::make_cstring_matcher(error_matcher)));

    MP_EXPECT_THROW_THAT(mp::lxd_wait(mock_network_access_manager.get(), base_url, json_reply.object(), 1000),
                         std::runtime_error, mpt::match_what(error_matcher));
}

TEST_F(LXDBackend, lxd_wait_status_code_failure_returned_throws_and_logs)
{
    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET" && url.contains("1.0/operations/b043d632-5c48-44b3-983c-a25660d61164/wait"))
        {
            const QByteArray wait_reply_error{"{"
                                              "\"error\": \"\","
                                              "\"error_code\": 0,"
                                              "\"metadata\": {"
                                              "  \"class\": \"task\","
                                              "  \"created_at\": \"2020-11-10T11:42:58.996868033-05:00\","
                                              "  \"description\": \"Stopping container\","
                                              "  \"err\": \"\","
                                              "  \"id\": \"b043d632-5c48-44b3-983c-a25660d61164\","
                                              "  \"location\": \"none\","
                                              "  \"may_cancel\": false,"
                                              "  \"metadata\": null,"
                                              "  \"resources\": {"
                                              "    \"containers\": ["
                                              "      \"/1.0/containers/test\""
                                              "    ]"
                                              "  },"
                                              "  \"status\": \"Success\","
                                              "  \"status_code\": 200,"
                                              "  \"updated_at\": \"2020-11-10T11:42:58.996868033-05:00\""
                                              "},"
                                              "\"operation\": \"\","
                                              "\"status\": \"Bad status\","
                                              "\"status_code\": 400,"
                                              "\"type\": \"sync\""
                                              "}"};

            return new mpt::MockLocalSocketReply(wait_reply_error);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    base_url.setHost("test");

    QJsonParseError json_error;
    auto json_reply = QJsonDocument::fromJson(mpt::stop_vm_data, &json_error);

    auto error_matcher = StrEq("Failure waiting on operation: (400) Bad status");

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::error), mpt::MockLogger::make_cstring_matcher(StrEq("lxd request")),
                    mpt::MockLogger::make_cstring_matcher(error_matcher)));

    MP_EXPECT_THROW_THAT(mp::lxd_wait(mock_network_access_manager.get(), base_url, json_reply.object(), 1000),
                         std::runtime_error, mpt::match_what(error_matcher));
}

TEST_F(LXDBackend, lxd_wait_metadata_status_code_failure_returned_throws_and_logs)
{
    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillOnce([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET" && url.contains("1.0/operations/b043d632-5c48-44b3-983c-a25660d61164/wait"))
        {
            const QByteArray wait_reply_error{"{"
                                              "\"error\": \"\","
                                              "\"error_code\": 0,"
                                              "\"metadata\": {"
                                              "  \"class\": \"task\","
                                              "  \"created_at\": \"2020-11-10T11:42:58.996868033-05:00\","
                                              "  \"description\": \"Stopping container\","
                                              "  \"err\": \"Failed to stop instance\","
                                              "  \"id\": \"b043d632-5c48-44b3-983c-a25660d61164\","
                                              "  \"location\": \"none\","
                                              "  \"may_cancel\": false,"
                                              "  \"metadata\": null,"
                                              "  \"resources\": {"
                                              "    \"containers\": ["
                                              "      \"/1.0/containers/test\""
                                              "    ]"
                                              "  },"
                                              "  \"status\": \"Failure\","
                                              "  \"status_code\": 400,"
                                              "  \"updated_at\": \"2020-11-10T11:42:58.996868033-05:00\""
                                              "},"
                                              "\"operation\": \"\","
                                              "\"status\": \"Success\","
                                              "\"status_code\": 0,"
                                              "\"type\": \"sync\""
                                              "}"};

            return new mpt::MockLocalSocketReply(wait_reply_error);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    base_url.setHost("test");

    QJsonParseError json_error;
    auto json_reply = QJsonDocument::fromJson(mpt::stop_vm_data, &json_error);

    auto error_matcher = StrEq("Operation completed with error: (400) Failed to stop instance");

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::error), mpt::MockLogger::make_cstring_matcher(StrEq("lxd request")),
                    mpt::MockLogger::make_cstring_matcher(error_matcher)));

    MP_EXPECT_THROW_THAT(mp::lxd_wait(mock_network_access_manager.get(), base_url, json_reply.object(), 1000),
                         std::runtime_error, mpt::match_what(error_matcher));
}

TEST_F(LXDBackend, unsupported_suspend_throws)
{
    mpt::StubVMStatusMonitor stub_monitor;

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                return new mpt::MockLocalSocketReply(mpt::vm_state_fully_running_data);
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("stop"))
            {
                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    MP_EXPECT_THROW_THAT(machine.suspend(),
                         mp::NotImplementedOnThisBackendException,
                         mpt::match_what(HasSubstr("suspend")));
}

TEST_F(LXDBackend, start_while_frozen_unfreezes)
{
    mpt::StubVMStatusMonitor stub_monitor;
    bool unfreeze_called{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&unfreeze_called](auto, auto request, auto outgoingData) {
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

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::info), mpt::MockLogger::make_cstring_matcher(StrEq("pied-piper-valley")),
                    mpt::MockLogger::make_cstring_matcher(StrEq("Resuming from a suspended state"))));

    machine.start();

    EXPECT_TRUE(unfreeze_called);
}

TEST_F(LXDBackend, shutdown_while_stopped_does_nothing_and_logs_debug)
{
    mpt::MockVMStatusMonitor mock_monitor;

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillRepeatedly([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return new mpt::MockLocalSocketReply(mpt::vm_state_stopped_data);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVirtualMachine machine{default_description,
                                  mock_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    ASSERT_EQ(machine.current_state(), mp::VirtualMachine::State::stopped);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    EXPECT_CALL(
        *logger_scope.mock_logger,
        log(Eq(mpl::Level::info),
            mpt::MockLogger::make_cstring_matcher(StrEq("pied-piper-valley")),
            mpt::MockLogger::make_cstring_matcher(StrEq("Ignoring shutdown since instance is already stopped."))));

    machine.shutdown();

    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::stopped);
}

TEST_F(LXDBackend, shutdown_while_frozen_throws_and_logs_info)
{
    const std::string sub_error_msg{"Cannot shut down suspended instance"};
    mpt::MockVMStatusMonitor mock_monitor;

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillRepeatedly([](auto, auto request, auto) {
        auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
        auto url = request.url().toString();

        if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
        {
            return new mpt::MockLocalSocketReply(mpt::vm_state_frozen_data);
        }

        return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
    });

    mp::LXDVirtualMachine machine{default_description,
                                  mock_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    ASSERT_EQ(machine.current_state(), mp::VirtualMachine::State::suspended);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));

    MP_EXPECT_THROW_THAT(machine.shutdown(), mp::VMStateInvalidException, mpt::match_what(HasSubstr(sub_error_msg)));

    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::suspended);
}

TEST_F(LXDBackend, ensure_vm_running_does_not_throw_starting)
{
    mpt::StubVMStatusMonitor stub_monitor;

    bool start_called{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&start_called](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                if (!start_called)
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_stopped_data);
                }
                else
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_starting_data);
                }
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                if (data.contains("start"))
                {
                    start_called = true;
                    return new mpt::MockLocalSocketReply(mpt::start_vm_data);
                }
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    machine.start();

    ASSERT_EQ(machine.state, mp::VirtualMachine::State::starting);

    EXPECT_NO_THROW(machine.ensure_vm_is_running());

    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::starting);
}

TEST_F(LXDBackend, shutdown_while_starting_throws_and_sets_correct_state)
{
    mpt::StubVMStatusMonitor stub_monitor;

    bool stop_called{false}, start_called{false};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&stop_called, &start_called](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                if ((!stop_called && !start_called) || (stop_called && start_called))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_stopped_data);
                }
                else
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_starting_data);
                }
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                if (data.contains("start"))
                {
                    start_called = true;
                    return new mpt::MockLocalSocketReply(mpt::start_vm_data);
                }
                else if (data.contains("stop"))
                {
                    stop_called = true;
                    return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
                }
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    machine.start();

    ASSERT_EQ(machine.state, mp::VirtualMachine::State::starting);

    mp::AutoJoinThread thread = [&machine] {
        machine.shutdown(mp::VirtualMachine::ShutdownPolicy::Poweroff);
    }; // force shutdown

    while (machine.state != mp::VirtualMachine::State::off)
        std::this_thread::sleep_for(1ms);

    MP_EXPECT_THROW_THAT(machine.ensure_vm_is_running(1ms), mp::StartException,
                         mpt::match_what(StrEq("Instance shutdown during start")));

    EXPECT_TRUE(start_called);
    EXPECT_TRUE(stop_called);
    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::stopped);
}

TEST_F(LXDBackend, start_failure_while_starting_throws_and_sets_correct_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    bool start_called{false};
    int running_returned{0};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&start_called, &running_returned](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                if (!start_called || running_returned > 1)
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_stopped_data);
                }

                ++running_returned;
                return new mpt::MockLocalSocketReply(mpt::vm_state_partial_running_data);
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("start"))
            {
                start_called = true;
                return new mpt::MockLocalSocketReply(mpt::start_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    machine.start();

    ASSERT_EQ(machine.state, mp::VirtualMachine::State::starting);

    EXPECT_NO_THROW(machine.ensure_vm_is_running(1ms));

    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::starting);

    MP_EXPECT_THROW_THAT(machine.ensure_vm_is_running(1ms), mp::StartException,
                         mpt::match_what(StrEq("Instance shutdown during start")));

    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::stopped);
}

TEST_F(LXDBackend, reboots_while_starting_does_not_throw_and_sets_correct_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    bool start_called{false}, reboot_simulated{false};
    int running_returned{0};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&start_called, &running_returned, &reboot_simulated](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                if (!start_called || ++running_returned == 2)
                {
                    if (running_returned == 2)
                    {
                        reboot_simulated = true;
                    }

                    return new mpt::MockLocalSocketReply(mpt::vm_state_stopped_data);
                }

                return new mpt::MockLocalSocketReply(mpt::vm_state_partial_running_data);
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("start"))
            {
                start_called = true;
                return new mpt::MockLocalSocketReply(mpt::start_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    machine.start();

    ASSERT_EQ(machine.current_state(), mp::VirtualMachine::State::starting);

    EXPECT_NO_THROW(machine.ensure_vm_is_running(1ms));

    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::starting);
    EXPECT_TRUE(reboot_simulated);
}

TEST_F(LXDBackend, current_state_connection_error_logs_warning_and_sets_unknown_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    const std::string exception_message{"Cannot connect to socket"};

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&exception_message](auto...) -> QNetworkReply* {
            throw mp::LocalSocketConnectionException(exception_message);
        });

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    EXPECT_CALL(*logger_scope.mock_logger,
                log(Eq(mpl::Level::warning), mpt::MockLogger::make_cstring_matcher(StrEq("pied-piper-valley")),
                    mpt::MockLogger::make_cstring_matcher(StrEq(exception_message))))
        .Times(2);

    EXPECT_EQ(machine.current_state(), mp::VirtualMachine::State::unknown);
}

TEST_P(LXDInstanceStatusTestSuite, lxd_state_returns_expected_VirtualMachine_state)
{
    mpt::StubVMStatusMonitor stub_monitor;

    const auto status_data = GetParam().first;
    const auto expected_state = GetParam().second;

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&status_data](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET" && url.contains("1.0/virtual-machines/pied-piper-valley/state"))
            {
                return new mpt::MockLocalSocketReply(status_data);
            }
            else if (op == "PUT" && url.contains("1.0/virtual-machines/pied-piper-valley/state") &&
                     data.contains("stop"))
            {
                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    if (expected_state == multipass::VirtualMachine::State::unknown)
    {
        QJsonParseError json_error;
        const auto json_reply = QJsonDocument::fromJson(status_data, &json_error);
        ASSERT_EQ(json_error.error, QJsonParseError::NoError);

        const auto metadata = json_reply["metadata"].toObject();
        const auto code = metadata["status_code"].toInt();
        if (code > 112)
        {
            QString error_msg{"unexpected LXD state: "};
            error_msg += metadata["status"].toString() + " (" + QString::number(code) + ")";
            logger_scope.mock_logger->expect_log(mpl::Level::error, error_msg.toStdString(), AtLeast(1));
        }
    }

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};

    EXPECT_EQ(machine.current_state(), expected_state);
}

INSTANTIATE_TEST_SUITE_P(LXDBackend, LXDInstanceStatusTestSuite, ValuesIn(lxd_instance_status_suite_inputs));

namespace
{
auto custom_request_matcher(const QString& verb, const std::string& url_sub_str)
{
    auto get_verb = [](const auto& request) {
        return request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
    };
    auto get_url = [](const auto& request) { return request.url().toString().toStdString(); };

    return AllOf(ResultOf(get_verb, Eq(verb)), ResultOf(get_url, HasSubstr(url_sub_str)));
}

const auto network_request_matcher = custom_request_matcher("GET", "1.0/networks?recursion=1");
} // namespace

TEST_F(LXDBackend, requests_networks)
{
    EXPECT_CALL(*mock_network_access_manager,
                createRequest(QNetworkAccessManager::CustomOperation, network_request_matcher, _))
        .WillOnce(Return(new mpt::MockLocalSocketReply{mpt::networks_empty_data}));

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};
    EXPECT_THAT(backend.networks(), IsEmpty());
}

struct LXDNetworksBadJson : LXDBackend, WithParamInterface<QByteArray>
{
};

TEST_P(LXDNetworksBadJson, handles_gibberish_networks_reply)
{
    auto log_matcher =
        mpt::MockLogger::make_cstring_matcher(AnyOf(HasSubstr("Error parsing JSON"), HasSubstr("Empty reply")));
    EXPECT_CALL(*logger_scope.mock_logger, log(Eq(mpl::Level::debug), _, log_matcher));
    EXPECT_CALL(*mock_network_access_manager,
                createRequest(QNetworkAccessManager::CustomOperation, network_request_matcher, _))
        .WillOnce(Return(new mpt::MockLocalSocketReply{GetParam()}));

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    EXPECT_THROW(backend.networks(), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(LXDBackend, LXDNetworksBadJson,
                         Values("gibberish", "unstarted}", "{unfinished", "strange\"", "{noval}", "]["));

struct LXDNetworksBadFields : LXDBackend, WithParamInterface<QByteArray>
{
};

TEST_P(LXDNetworksBadFields, ignores_network_without_expected_fields)
{
    EXPECT_CALL(*mock_network_access_manager,
                createRequest(QNetworkAccessManager::CustomOperation, network_request_matcher, _))
        .WillOnce(Return(new mpt::MockLocalSocketReply{GetParam()}));

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};
    EXPECT_THAT(backend.networks(), IsEmpty());
}

INSTANTIATE_TEST_SUITE_P(LXDBackend, LXDNetworksBadFields,
                         Values("{}", "{\"other\": \"stuff\"}", "{\"metadata\": \"notarray\"}",
                                "{\"metadata\": [\"notdict\"]}",
                                "{\"metadata\": [{\"type\": \"bridge\", \"but\": \"noname\"}]}",
                                "{\"metadata\": [{\"name\": \"\", \"type\": \"bridge\", \"but\": \"empty name\"}]}",
                                "{\"metadata\": [{\"name\": \"bla\", \"but\": \"notype\"}]}",
                                "{\"metadata\": [{\"name\": 123, \"type\": \"bridge\"}]}",
                                "{\"metadata\": [{\"name\": \"eth0\", \"type\": 123}]}"));

struct LXDNetworksOnlySupportedTypes : LXDBackend, WithParamInterface<QByteArray>
{
};

TEST_P(LXDNetworksOnlySupportedTypes, reports_only_bridge_and_ethernet_networks)
{
    EXPECT_CALL(*mock_network_access_manager,
                createRequest(QNetworkAccessManager::CustomOperation, network_request_matcher, _))
        .WillOnce(Return(new mpt::MockLocalSocketReply{GetParam()}));

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, get_network_interfaces_info)
        .WillOnce(Return(std::map<std::string, mp::NetworkInterfaceInfo>{
            {"lxdbr0", {"lxdbr0", "bridge", "gobbledygook"}},
            {"mpbr0", {"mpbr0", "bridge", "gobbledygook"}},
            {"virbr0", {"virbr0", "bridge", "gobbledygook"}},
            {"mpqemubr0", {"mpqemubr0", "bridge", "gobbledygook"}},
            {"enxe4b97a832426", {"enxe4b97a832426", "ethernet", "gobbledygook"}}}));

    auto id_matcher = [](const std::string& expect) { return Field(&mp::NetworkInterfaceInfo::id, Eq(expect)); };
    EXPECT_THAT(backend.networks(),
                AllOf(Each(Field(&mp::NetworkInterfaceInfo::type, MatchesRegex("bridge|ethernet"))),
                      UnorderedElementsAre(id_matcher("enxe4b97a832426"), id_matcher("lxdbr0"), id_matcher("mpbr0"),
                                           id_matcher("virbr0"), id_matcher("mpqemubr0"))));
}

INSTANTIATE_TEST_SUITE_P(LXDBackend, LXDNetworksOnlySupportedTypes,
                         Values(mpt::networks_realistic_data, mpt::networks_faulty_data));

TEST_F(LXDBackend, honors_bridge_description_from_lxd_when_available)
{
    auto description = "Australopithecus";
    auto data_template = QStringLiteral(R"({"metadata": [{"type": "bridge", "name": "br0", "description": "%1"}]})");
    auto data = data_template.arg(description).toUtf8();
    EXPECT_CALL(*mock_network_access_manager,
                createRequest(QNetworkAccessManager::CustomOperation, network_request_matcher, _))
        .WillOnce(Return(new mpt::MockLocalSocketReply{{data}}));

    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, get_network_interfaces_info)
        .WillOnce(Return(std::map<std::string, mp::NetworkInterfaceInfo>{{"br0", {"br0", "bridge", "gibberish"}}}));

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    EXPECT_THAT(backend.networks(), ElementsAre(Field(&mp::NetworkInterfaceInfo::description, Eq(description))));
}

TEST_F(LXDBackend, falls_back_to_bridge_description_from_platform)
{
    auto data = QByteArrayLiteral(R"({"metadata": [{"type": "bridge", "name": "br0", "description": ""}]})");
    auto fallback_desc = "fallback";

    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, get_network_interfaces_info)
        .WillOnce(Return(std::map<std::string, mp::NetworkInterfaceInfo>{{"br0", {"br0", "bridge", fallback_desc}}}));

    EXPECT_CALL(*mock_network_access_manager,
                createRequest(QNetworkAccessManager::CustomOperation, network_request_matcher, _))
        .WillOnce(Return(new mpt::MockLocalSocketReply{{data}}));

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    EXPECT_THAT(backend.networks(), ElementsAre(Field(&mp::NetworkInterfaceInfo::description, Eq(fallback_desc))));
}

TEST_F(LXDBackend, skips_platform_network_inspection_when_lxd_reports_no_networks)
{
    auto data = QByteArrayLiteral(R"({"metadata": []})");

    auto [mock_platform, guard] = mpt::MockPlatform::inject();
    EXPECT_CALL(*mock_platform, get_network_interfaces_info).Times(0);

    EXPECT_CALL(*mock_network_access_manager,
                createRequest(QNetworkAccessManager::CustomOperation, network_request_matcher, _))
        .WillOnce(Return(new mpt::MockLocalSocketReply{{data}}));

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    EXPECT_THAT(backend.networks(), IsEmpty());
}

namespace
{
Matcher<QIODevice*> request_data_matcher(Matcher<QJsonObject> json_matcher)
{
    auto extract_json = [](QIODevice* device) {
        device->open(QIODevice::ReadOnly);
        return QJsonDocument::fromJson(device->readAll()).object();
    };

    return ResultOf(extract_json, json_matcher);
}

std::vector<QJsonObject> extract_devices(const QJsonObject& request_json)
{
    std::vector<QJsonObject> ret; // we need an stl collection that gmock can work with
    for (const auto& device : request_json["devices"].toObject())
        ret.push_back(device.toObject());
    return ret;
}

bool device_json_matches_interface(const QJsonObject& device, const mp::NetworkInterface& interface)
{
    return device["type"] == "nic" && device["nictype"] == "bridged" &&
           device["parent"].toString().toStdString() == interface.id &&
           device["hwaddr"].toString().toStdString() == interface.mac_address;
}

std::vector<Matcher<QJsonObject>> device_json_matchers_from(const std::vector<mp::NetworkInterface>& interfaces)
{
    std::vector<Matcher<QJsonObject>> device_matchers;
    for (const auto& interface : interfaces)
    {
        device_matchers.push_back(Truly(
            [&interface](const QJsonObject& device) { return device_json_matches_interface(device, interface); }));
    }

    return device_matchers;
}

void setup_vm_creation_expectations(mpt::MockNetworkAccessManager& mock_network_access_mgr,
                                    Matcher<QIODevice*> request_contents_matcher)
{
    EXPECT_CALL(mock_network_access_mgr, createRequest(QNetworkAccessManager::CustomOperation,
                                                       custom_request_matcher("GET", "pied-piper-valley/state"), _))
        .Times(3)
        .WillOnce(Return(new mpt::MockLocalSocketReply{mpt::not_found_data, QNetworkReply::ContentNotFoundError}))
        .WillRepeatedly([] { return new mpt::MockLocalSocketReply{mpt::vm_info_data}; });

    EXPECT_CALL(mock_network_access_mgr,
                createRequest(QNetworkAccessManager::CustomOperation,
                              custom_request_matcher("POST", "virtual-machines"), request_contents_matcher))
        .WillOnce(Return(new mpt::MockLocalSocketReply{mpt::create_vm_data}));

    EXPECT_CALL(mock_network_access_mgr,
                createRequest(QNetworkAccessManager::CustomOperation,
                              custom_request_matcher("GET", "operations/0020444c-2e4c-49d5-83ed-3275e3f6d005/wait"), _))
        .WillOnce(Return(new mpt::MockLocalSocketReply{mpt::create_vm_finished_data}));
}
} // namespace

TEST_F(LXDBackend, posts_extra_network_devices)
{
    mpt::StubVMStatusMonitor stub_monitor;

    default_description.extra_interfaces.push_back({"parent1", "ab:cd:ef:01:23:45", true});
    default_description.extra_interfaces.push_back({"parent2", "01:23:45:ab:cd:ef", false});
    default_description.extra_interfaces.push_back({"parent3", "ba:ba:ca:ca:ca:ba", true});

    auto devices_matcher = IsSupersetOf(device_json_matchers_from(default_description.extra_interfaces));
    auto json_matcher = ResultOf(&extract_devices, devices_matcher);
    setup_vm_creation_expectations(*mock_network_access_manager, request_data_matcher(json_matcher));

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};
}

TEST_F(LXDBackend, posts_network_data_config_if_available)
{
    mpt::StubVMStatusMonitor stub_monitor;

    static constexpr auto config = "Leia: Princess";
    default_description.network_data_config = config;

    auto get_config = [](const auto& json) {
        return json["config"].toObject()["user.network-config"].toString().toStdString();
    };
    auto json_matcher = ResultOf(get_config, HasSubstr(config));

    setup_vm_creation_expectations(*mock_network_access_manager, request_data_matcher(json_matcher));

    mp::LXDVirtualMachine machine{default_description,
                                  stub_monitor,
                                  mock_network_access_manager.get(),
                                  base_url,
                                  bridge_name,
                                  default_storage_pool,
                                  key_provider,
                                  instance_dir.path()};
}

namespace
{
class CustomLXDFactory : public mp::LXDVirtualMachineFactory
{
public:
    using mp::LXDVirtualMachineFactory::create_bridge_with;
    using mp::LXDVirtualMachineFactory::LXDVirtualMachineFactory;

    MOCK_METHOD(void, prepare_networking, (std::vector<mp::NetworkInterface>&), (override));
};
} // namespace

TEST_F(LXDBackend, prepares_networking_via_base_factory)
{
    CustomLXDFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};
    std::vector<mp::NetworkInterface> extra_networks{{"netid", "mac", false}};

    auto address = [](const auto& v) { return &v; }; // replace with Address matcher once available
    EXPECT_CALL(backend, prepare_networking(ResultOf(address, Eq(&extra_networks))));
    backend.prepare_networking(extra_networks);
}

TEST_F(LXDBackend, createsBridgesViaBackendUtils)
{
    const auto net = mp::NetworkInterfaceInfo{"bla", "ethernet", "ble"};
    const auto bridge = "bli";
    auto [mock_backend, guard] = mpt::MockBackend::inject();

    CustomLXDFactory factory{std::move(mock_network_access_manager), data_dir.path(), base_url};

    EXPECT_CALL(*mock_backend, create_bridge_with(net.id)).WillOnce(Return(bridge));
    EXPECT_EQ(factory.create_bridge_with(net), bridge);
}

TEST_F(LXDBackend, addsNetworkInterface)
{
    mpt::StubVMStatusMonitor stub_monitor;
    unsigned patch_times_called = 0;

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _))
        .WillRepeatedly([&patch_times_called](auto, auto request, auto outgoingData) {
            outgoingData->open(QIODevice::ReadOnly);
            auto data = outgoingData->readAll();
            auto op = request.attribute(QNetworkRequest::CustomVerbAttribute).toString();
            auto url = request.url().toString();

            if (op == "GET")
            {
                if (url.contains("1.0/virtual-machines/pied-piper-valley/state"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_state_stopped_data);
                }
                if (url.contains("1.0/virtual-machines/pied-piper-valley"))
                {
                    return new mpt::MockLocalSocketReply(mpt::vm_info_data);
                }
            }
            if (op == "PUT")
            {
                if (url.contains("1.0/virtual-machines/pied-piper-valley/state") && data.contains("stop"))
                {
                    return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
                }
                if (url.contains("1.0/virtual-machines"))
                {
                    return new mpt::MockLocalSocketReply(mpt::delete_vm_wait_task_data);
                }
            }
            if (op == "PATCH")
            {
                ++patch_times_called;

                EXPECT_EQ(data.toStdString(),
                          "{\"devices\":{\"eth2\":{\"hwaddr\":\"52:54:00:56:78:90\",\"name\":"
                          "\"eth2\",\"nictype\":\"bridged\",\"parent\":\"id\",\"type\":\"nic\"}}}");

                return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
            }

            return new mpt::MockLocalSocketReply(mpt::not_found_data, QNetworkReply::ContentNotFoundError);
        });

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), base_url};

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    machine->shutdown();

    machine->add_network_interface(1, "", {"id", "52:54:00:56:78:90", true});

    EXPECT_EQ(patch_times_called, 1u);
}

TEST_F(LXDBackend, converts_http_to_https)
{
    mpt::StubVMStatusMonitor stub_monitor;

    EXPECT_CALL(*mock_network_access_manager, createRequest(_, _, _)).WillRepeatedly([](auto, auto request, auto) {
        EXPECT_EQ(request.url().scheme(), "https");
        return new mpt::MockLocalSocketReply(mpt::stop_vm_data);
    });

    mp::LXDVirtualMachineFactory backend{std::move(mock_network_access_manager), data_dir.path(), QUrl{"http://bar"}};
    backend.create_virtual_machine(default_description, key_provider, stub_monitor);
}
