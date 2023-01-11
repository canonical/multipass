/*
 * Copyright (C) 2022 Canonical, Ltd.
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
#include "daemon_test_fixture.h"
#include "mock_image_host.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"

#include <src/daemon/daemon.h>

#include <multipass/constants.h>
#include <multipass/format.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestDaemonStart : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("true"));
    }

    const std::string mock_instance_name{"real-zebraphant"};
    const std::string mac_addr{"52:54:00:73:76:28"};
    std::vector<mp::NetworkInterface> extra_interfaces;

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};

TEST_F(TestDaemonStart, successfulStartOkStatus)
{
    auto mock_factory = use_a_mock_vm_factory();
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });
    EXPECT_CALL(*instance_ptr, wait_until_ssh_up(_)).WillRepeatedly(Return());
    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::off));
    EXPECT_CALL(*instance_ptr, start()).Times(1);

    config_builder.data_directory = temp_dir->path();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::Daemon daemon{config_builder.build()};

    mp::StartRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);

    auto status = call_daemon_slot(daemon, &mp::Daemon::start, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>{});

    EXPECT_TRUE(status.ok());
}

TEST_F(TestDaemonStart, unknownStateDoesNotStart)
{
    auto mock_factory = use_a_mock_vm_factory();
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::unknown));
    EXPECT_CALL(*instance_ptr, start()).Times(0);

    config_builder.data_directory = temp_dir->path();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::Daemon daemon{config_builder.build()};

    mp::StartRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);

    auto status = call_daemon_slot(daemon, &mp::Daemon::start, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>{});

    EXPECT_FALSE(status.ok());
}

TEST_F(TestDaemonStart, suspendingStateDoesNotStartHasError)
{
    auto mock_factory = use_a_mock_vm_factory();
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    EXPECT_CALL(*instance_ptr, current_state()).WillRepeatedly(Return(mp::VirtualMachine::State::suspending));
    EXPECT_CALL(*instance_ptr, start()).Times(0);

    config_builder.data_directory = temp_dir->path();
    config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

    mp::Daemon daemon{config_builder.build()};

    mp::StartRequest request;
    request.mutable_instance_names()->add_instance_name(mock_instance_name);

    auto status = call_daemon_slot(daemon, &mp::Daemon::start, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::StartReply, mp::StartRequest>>{});

    EXPECT_FALSE(status.ok());

    EXPECT_THAT(status.error_message(),
                HasSubstr(fmt::format("Cannot start the instance \'{}\' while suspending", mock_instance_name)));
}
