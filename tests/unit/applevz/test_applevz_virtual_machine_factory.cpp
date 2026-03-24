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

#include "mock_applevz_utils.h"
#include "mock_applevz_wrapper.h"
#include "tests/unit/common.h"
#include "tests/unit/mock_logger.h"
#include "tests/unit/stub_availability_zone_manager.h"
#include "tests/unit/stub_ssh_key_provider.h"
#include "tests/unit/stub_status_monitor.h"
#include "tests/unit/temp_dir.h"

#include <applevz/applevz_virtual_machine_factory.h>
#include <multipass/memory_size.h>
#include <multipass/network_interface_info.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_image.h>

#include <QDir>

#include <stdexcept>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct AppleVZVirtualMachineFactory_UnitTests : public ::testing::Test
{
    mpt::TempDir dummy_data_dir;
    mpt::StubAvailabilityZoneManager stub_az_manager{};
    mpt::StubSSHKeyProvider stub_key_provider{};
    mpt::StubVMStatusMonitor stub_monitor{};

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();

    mpt::MockAppleVZWrapper::GuardedMock mock_applevz_wrapper_injection{
        mpt::MockAppleVZWrapper::inject<NiceMock>()};
    mpt::MockAppleVZWrapper& mock_applevz = *mock_applevz_wrapper_injection.first;

    mpt::MockAppleVZUtils::GuardedMock mock_applevz_utils_injection{
        mpt::MockAppleVZUtils::inject<NiceMock>()};
    mpt::MockAppleVZUtils& mock_applevz_utils = *mock_applevz_utils_injection.first;

    inline static auto mock_handle_raw =
        reinterpret_cast<mp::applevz::VirtualMachineHandle*>(0xbadf00d);
    mp::applevz::VMHandle mock_handle{mock_handle_raw, [](mp::applevz::VirtualMachineHandle*) {}};

    auto construct_factory()
    {
        return std::make_shared<mp::applevz::AppleVZVirtualMachineFactory>(dummy_data_dir.path(),
                                                                           stub_az_manager);
    }
};
} // namespace

// ---------------------------------------------------------

TEST_F(AppleVZVirtualMachineFactory_UnitTests, prepareSourceImage)
{
    const std::filesystem::path source_path = "/original/image.img";
    const std::filesystem::path converted_path = "/converted/image.raw";

    mp::VMImage source_image;
    source_image.image_path = source_path;

    EXPECT_CALL(mock_applevz_utils, convert_to_supported_format(source_path, _))
        .WillOnce(Return(converted_path));

    auto uut = construct_factory();
    const auto result = uut->prepare_source_image(source_image);

    EXPECT_EQ(result.image_path, converted_path);
}

TEST_F(AppleVZVirtualMachineFactory_UnitTests, prepareInstanceImage)
{
    mp::VMImage instance_image;
    instance_image.image_path = "/instance/image.raw";

    mp::VirtualMachineDescription desc;
    desc.disk_space = mp::MemorySize::from_bytes(10 * 1024 * 1024);

    EXPECT_CALL(mock_applevz_utils, resize_image(desc.disk_space, instance_image.image_path));

    auto uut = construct_factory();
    EXPECT_NO_THROW(uut->prepare_instance_image(instance_image, desc));
}

TEST_F(AppleVZVirtualMachineFactory_UnitTests, prepareInstanceImageResizeFails)
{
    mp::VMImage instance_image;
    instance_image.image_path = "/instance/image.raw";

    mp::VirtualMachineDescription desc;
    desc.disk_space = mp::MemorySize::from_bytes(10 * 1024 * 1024);

    EXPECT_CALL(mock_applevz_utils, resize_image(desc.disk_space, instance_image.image_path))
        .WillOnce(Throw(std::runtime_error{"resize failed"}));

    auto uut = construct_factory();
    EXPECT_THROW(uut->prepare_instance_image(instance_image, desc), std::runtime_error);
}

TEST_F(AppleVZVirtualMachineFactory_UnitTests, hypervisorHealthCheckSupported)
{
    EXPECT_CALL(mock_applevz, is_supported()).WillOnce(Return(true));

    auto uut = construct_factory();
    EXPECT_NO_THROW(uut->hypervisor_health_check());
}

TEST_F(AppleVZVirtualMachineFactory_UnitTests, hypervisorHealthCheckNotSupported)
{
    EXPECT_CALL(mock_applevz, is_supported()).WillOnce(Return(false));

    auto uut = construct_factory();
    EXPECT_THROW(uut->hypervisor_health_check(), std::runtime_error);
}

TEST_F(AppleVZVirtualMachineFactory_UnitTests, removeResourcesForDeletesInstanceDirectory)
{
    const std::string vm_name = "test-vm";
    auto uut = construct_factory();

    QDir instance_dir{uut->get_instance_directory(vm_name)};
    ASSERT_TRUE(instance_dir.mkpath("."));
    ASSERT_TRUE(instance_dir.exists());

    EXPECT_NO_THROW(uut->remove_resources_for(vm_name));

    EXPECT_FALSE(instance_dir.exists());
}

TEST_F(AppleVZVirtualMachineFactory_UnitTests, removeResourcesForNonExistentVmDoesNotThrow)
{
    auto uut = construct_factory();
    EXPECT_NO_THROW(uut->remove_resources_for("does-not-exist"));
}

TEST_F(AppleVZVirtualMachineFactory_UnitTests, networksReturnsBridgedInterfaces)
{
    const std::vector<mp::NetworkInterfaceInfo> expected_interfaces = {
        {.id = "en0", .type = "Ethernet", .description = "Built-in Ethernet"},
        {.id = "en1", .type = "Wi-Fi", .description = "Wi-Fi"}};

    EXPECT_CALL(mock_applevz, bridged_network_interfaces()).WillOnce(Return(expected_interfaces));

    auto uut = construct_factory();
    EXPECT_EQ(uut->networks(), expected_interfaces);
}

TEST_F(AppleVZVirtualMachineFactory_UnitTests, createVirtualMachine)
{
    mp::VirtualMachineDescription desc;
    desc.vm_name = "test-vm";

    EXPECT_CALL(mock_applevz_utils, convert_to_supported_format(_, _))
        .WillRepeatedly(ReturnArg<0>());

    EXPECT_CALL(mock_applevz, create_vm(_, _))
        .WillOnce(DoAll(SetArgReferee<1>(mock_handle), Return(mp::applevz::CFError{})));

    EXPECT_CALL(mock_applevz, get_state(_))
        .WillRepeatedly(Return(mp::applevz::AppleVMState::stopped));

    auto uut = construct_factory();
    auto vm = uut->create_virtual_machine(desc, stub_key_provider, stub_monitor);

    EXPECT_NE(vm, nullptr);
}
