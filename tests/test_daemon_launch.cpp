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

#include "blueprint_test_lambdas.h"
#include "common.h"
#include "daemon_test_fixture.h"
#include "mock_image_host.h"
#include "mock_json_utils.h"
#include "mock_platform.h"
#include "mock_server_reader_writer.h"
#include "mock_settings.h"
#include "mock_virtual_machine.h"
#include "mock_vm_blueprint_provider.h"
#include "mock_vm_image_vault.h"
#include "stub_virtual_machine.h"
#include "stub_vm_image_vault.h"

#include <src/daemon/daemon.h>

#include <multipass/constants.h>
#include <multipass/format.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestDaemonLaunch : public mpt::DaemonTestFixture
{
    void SetUp() override
    {
        EXPECT_CALL(mock_settings, register_handler).WillRepeatedly(Return(nullptr));
        EXPECT_CALL(mock_settings, unregister_handler).Times(AnyNumber());
        EXPECT_CALL(mock_settings, get(Eq(mp::mounts_key))).WillRepeatedly(Return("true"));
    }

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject<StrictMock>();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;

    const mpt::MockJsonUtils::GuardedMock mock_json_utils_injection = mpt::MockJsonUtils::inject<NiceMock>();
};

TEST_F(TestDaemonLaunch, blueprintFoundMountsWorkspaceWithNameOverride)
{
    const std::string name{"ultimo-blueprint"};
    const std::string command_line_name{"name-override"};
    static constexpr int num_cores = 4;
    const std::string mem_size_str{"4G"};
    const auto mem_size = mp::MemorySize(mem_size_str);
    const std::string disk_space_str{"25G"};
    const auto disk_space = mp::MemorySize(disk_space_str);
    const std::string remote{"release"};
    const std::string release{"focal"};

    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, command_line_name));

    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _)).WillOnce(mpt::fetch_image_lambda(release, remote));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(
            mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote, std::nullopt, name));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);

    mp::Daemon daemon{config_builder.build()};

    mp::LaunchRequest request;
    request.set_instance_name(command_line_name);
    request.set_image(name);

    mp::LaunchReply reply;
    StrictMock<mpt::MockServerReaderWriter<mp::LaunchReply, mp::LaunchRequest>> writer{};

    EXPECT_CALL(writer, Write(_, _)).WillRepeatedly([&reply](const mp::LaunchReply& written_reply, auto) -> bool {
        reply = written_reply;
        return true;
    });

    auto status = call_daemon_slot(daemon, &mp::Daemon::launch, request, writer);

    EXPECT_EQ(reply.workspaces_to_be_created_size(), 1);
    if (reply.workspaces_to_be_created_size() > 0)
    {
        EXPECT_EQ(reply.workspaces_to_be_created(0), command_line_name);
    }
}

TEST_F(TestDaemonLaunch, v2BlueprintFoundPropagatesSha)
{
    const std::string name{"ultimo-blueprint"};
    const std::string command_line_name{"name-override"};
    static constexpr int num_cores = 4;
    const std::string mem_size_str{"4G"};
    const auto mem_size = mp::MemorySize(mem_size_str);
    const std::string disk_space_str{"25G"};
    const auto disk_space = mp::MemorySize(disk_space_str);
    const std::string remote{"release"};
    const std::string release{"focal"};
    const std::string sha256{"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"};

    auto mock_factory = use_a_mock_vm_factory();
    auto mock_image_vault = std::make_unique<NiceMock<mpt::MockVMImageVault>>();
    auto mock_blueprint_provider = std::make_unique<NiceMock<mpt::MockVMBlueprintProvider>>();

    EXPECT_CALL(*mock_factory, create_virtual_machine)
        .WillOnce(mpt::create_virtual_machine_lambda(num_cores, mem_size, disk_space, command_line_name));

    // The expectation of this test is set in fetch_image_lambda().
    EXPECT_CALL(*mock_image_vault, fetch_image(_, _, _, _, _, _, _))
        .WillOnce(mpt::fetch_image_lambda(release, remote, true));

    EXPECT_CALL(*mock_blueprint_provider, fetch_blueprint_for(_, _, _))
        .WillOnce(mpt::fetch_blueprint_for_lambda(num_cores, mem_size, disk_space, release, remote, std::nullopt,
                                                  std::nullopt, sha256));

    EXPECT_CALL(*mock_blueprint_provider, name_from_blueprint(_)).WillOnce(Return(name));

    config_builder.blueprint_provider = std::move(mock_blueprint_provider);
    config_builder.vault = std::move(mock_image_vault);

    mp::Daemon daemon{config_builder.build()};

    mp::LaunchRequest request;
    request.set_instance_name(command_line_name);
    request.set_image(name);

    mp::LaunchReply reply;
    StrictMock<mpt::MockServerReaderWriter<mp::LaunchReply, mp::LaunchRequest>> writer{};

    EXPECT_CALL(writer, Write(_, _)).WillRepeatedly([&reply](const mp::LaunchReply& written_reply, auto) -> bool {
        reply = written_reply;
        return true;
    });

    auto status = call_daemon_slot(daemon, &mp::Daemon::launch, request, writer);
}
