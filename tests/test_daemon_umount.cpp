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
#include "mock_logger.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_virtual_machine.h"
#include "mock_vm_image_vault.h"

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct TestDaemonUmount : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler(_)).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());

        config_builder.vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();

        mock_factory = use_a_mock_vm_factory();
    }

    mpt::MockVirtualMachineFactory* mock_factory;

    const std::string mock_instance_name{"real-zebraphant"};
    const std::string mac_addr{"52:54:00:73:76:28"};
    const std::string fake_target_path{"/home/ubuntu/foo"};

    std::vector<mp::NetworkInterface> extra_interfaces;

    mpt::MockPlatform::GuardedMock platform_attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = platform_attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
};
} // namespace

TEST_F(TestDaemonUmount, missingInstanceFails)
{
    const std::string& fake_instance{"fake"};

    mp::Daemon daemon{config_builder.build()};

    mp::UmountRequest request;
    auto entry = request.add_target_paths();
    entry->set_instance_name(fake_instance);

    auto status = call_daemon_slot(daemon, &mp::Daemon::umount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::UmountReply, mp::UmountRequest>>{});

    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
    EXPECT_THAT(status.error_message(), HasSubstr(fmt::format("instance '{}' does not exist", fake_instance)));
}

TEST_F(TestDaemonUmount, noTargetsUnmountsAll)
{
    const auto [temp_dir, filename] = plant_instance_json(fake_json_contents(mac_addr, extra_interfaces));
    config_builder.data_directory = temp_dir->path();

    auto instance_ptr = std::make_unique<NiceMock<mpt::MockVirtualMachine>>(mock_instance_name);
    EXPECT_CALL(*mock_factory, create_virtual_machine(_, _)).WillOnce([&instance_ptr](const auto&, auto&) {
        return std::move(instance_ptr);
    });

    mp::Daemon daemon{config_builder.build()};

    mp::UmountRequest request;
    auto entry = request.add_target_paths();
    entry->set_instance_name(mock_instance_name);

    auto status = call_daemon_slot(daemon, &mp::Daemon::umount, request,
                                   StrictMock<mpt::MockServerReaderWriter<mp::UmountReply, mp::UmountRequest>>{});

    EXPECT_TRUE(status.ok());
}
