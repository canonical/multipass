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

#include <hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <hyperv_api/hcs_virtual_machine_exceptions.h>
#include <hyperv_api/hcs_virtual_machine_factory.h>
#include <multipass/network_interface.h>
#include <multipass/virtual_machine_description.h>
#include <multipass/vm_image.h>

#include "tests/common.h"
#include "tests/hyperv_api/mock_hyperv_hcs_wrapper.h"
#include "tests/mock_hyperv_hcn_wrapper.h"
#include "tests/mock_hyperv_virtdisk_wrapper.h"
#include "tests/mock_platform.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_dir.h"

#include <memory>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mhv = multipass::hyperv;

using hcs_handle_t = mhv::hcs::HcsSystemHandle;
using hcs_op_result_t = mhv::OperationResult;
using uut_t = mhv::HCSVirtualMachineFactory;
using namespace testing;

struct HyperVHCSVirtualMachineFactory_UnitTests : public ::testing::Test
{
    mpt::TempDir dummy_data_dir;
    mpt::StubSSHKeyProvider stub_key_provider{};
    mpt::StubVMStatusMonitor stub_monitor{};

    mpt::MockHCSWrapper::GuardedMock mock_hcs_wrapper_injection =
        mpt::MockHCSWrapper::inject<StrictMock>();
    mpt::MockHCSWrapper& mock_hcs = *mock_hcs_wrapper_injection.first;

    std::shared_ptr<mpt::MockHCNWrapper> mock_hcn{std::make_shared<mpt::MockHCNWrapper>()};
    std::shared_ptr<mpt::MockVirtDiskWrapper> mock_virtdisk{
        std::make_shared<mpt::MockVirtDiskWrapper>()};

    mpt::MockPlatform::GuardedMock attr{mpt::MockPlatform::inject<NiceMock>()};
    mpt::MockPlatform* mock_platform = attr.first;

    inline static auto mock_handle_raw = reinterpret_cast<void*>(0xbadf00d);
    hcs_handle_t mock_handle{mock_handle_raw, [](void*) {}};

    auto construct_factory()
    {
        return std::make_shared<uut_t>(dummy_data_dir.path(), mock_hcn, mock_virtdisk);
    }
};

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, remove_resources_for_impl_vm_exists)
{
    auto vm_name = "test-vm";
    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillOnce(DoAll(
            [&](const std::string& name, hcs_handle_t& out_handle) {
                ASSERT_EQ(vm_name, name);
                out_handle = mock_handle;
            },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, terminate_compute_system(Eq(mock_handle)))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_factory());
    uut->remove_resources_for(vm_name);
}

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, remove_resources_for_impl_does_not_exists)
{
    auto vm_name = "test-vm";
    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillOnce(DoAll(
            [&](const std::string& name, hcs_handle_t& out_handle) {
                ASSERT_EQ(vm_name, name);
                out_handle = mock_handle;
            },
            Return(hcs_op_result_t{1, L""})));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = construct_factory());
    uut->remove_resources_for(vm_name);
}

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, prepare_instance_image)
{
    std::shared_ptr<uut_t> uut{nullptr};

    multipass::VMImage img;
    img.image_path = "abcdef";
    multipass::VirtualMachineDescription desc;
    desc.disk_space = multipass::MemorySize::from_bytes(123456);

    EXPECT_CALL(
        *mock_virtdisk,
        resize_virtual_disk(Eq(img.image_path.toStdString()), Eq(desc.disk_space.in_bytes())))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    ASSERT_NO_THROW(uut = construct_factory());
    uut->prepare_instance_image(img, desc);
}

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, prepare_instance_image_failed)
{
    std::shared_ptr<uut_t> uut{nullptr};

    multipass::VMImage img;
    img.image_path = "abcdef";
    multipass::VirtualMachineDescription desc;
    desc.disk_space = multipass::MemorySize::from_bytes(123456);

    EXPECT_CALL(
        *mock_virtdisk,
        resize_virtual_disk(Eq(img.image_path.toStdString()), Eq(desc.disk_space.in_bytes())))
        .WillOnce(Return(hcs_op_result_t{1, L""}));

    ASSERT_NO_THROW(uut = construct_factory());
    EXPECT_THROW(uut->prepare_instance_image(img, desc), multipass::hyperv::ImageResizeException);
}

TEST_F(HyperVHCSVirtualMachineFactory_UnitTests, create_virtual_machine)
{
    std::shared_ptr<uut_t> uut{nullptr};
    multipass::VirtualMachineDescription desc;
    multipass::NetworkInterfaceInfo interface1, interface2;
    interface1.id = "aabb";
    interface2.id = "bbaa";
    multipass::NetworkInterface if1, if2;
    if1.id = "Multipass vSwitch (aabb)";
    if2.id = "Multipass vSwitch (bbaa)";
    desc.extra_interfaces.push_back(if1);
    desc.extra_interfaces.push_back(if2);

    EXPECT_CALL(*mock_platform, get_network_interfaces_info())
        .WillRepeatedly(
            Return(std::map<std::string, multipass::NetworkInterfaceInfo>{{"aabb", interface1},
                                                                          {"bbaa", interface2}}));

    EXPECT_CALL(*mock_hcn, create_network(_))
        .WillOnce(DoAll(
            [&](const multipass::hyperv::hcn::CreateNetworkParameters& params) {
                EXPECT_EQ(params.name, if1.id);
                EXPECT_EQ(params.type, mhv::hcn::HcnNetworkType::Transparent());
                EXPECT_EQ(params.guid, multipass::utils::make_uuid(if1.id).toStdString());
                ASSERT_EQ(params.policies.size(), 1);
                EXPECT_EQ(params.policies[0].type,
                          mhv::hcn::HcnNetworkPolicyType::NetAdapterName());
                ASSERT_TRUE(
                    std::holds_alternative<multipass::hyperv::hcn::HcnNetworkPolicyNetAdapterName>(
                        params.policies[0].settings));
                const auto& net_adapter_name =
                    std::get<multipass::hyperv::hcn::HcnNetworkPolicyNetAdapterName>(
                        params.policies[0].settings);
                EXPECT_EQ(net_adapter_name.net_adapter_name, interface1.id);
            },
            Return(hcs_op_result_t{0, L""})))
        .WillOnce(DoAll(
            [&](const multipass::hyperv::hcn::CreateNetworkParameters& params) {
                EXPECT_EQ(params.name, if2.id);
                EXPECT_EQ(params.type, mhv::hcn::HcnNetworkType::Transparent());
                EXPECT_EQ(params.guid, multipass::utils::make_uuid(if2.id).toStdString());
                ASSERT_EQ(params.policies.size(), 1);
                EXPECT_EQ(params.policies[0].type,
                          mhv::hcn::HcnNetworkPolicyType::NetAdapterName());
                ASSERT_TRUE(
                    std::holds_alternative<multipass::hyperv::hcn::HcnNetworkPolicyNetAdapterName>(
                        params.policies[0].settings));
                const auto& net_adapter_name =
                    std::get<multipass::hyperv::hcn::HcnNetworkPolicyNetAdapterName>(
                        params.policies[0].settings);
                EXPECT_EQ(net_adapter_name.net_adapter_name, interface2.id);
            },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, open_compute_system(_, _))
        .WillRepeatedly(DoAll(
            [this](const std::string& name, hcs_handle_t& out_handle) { out_handle = mock_handle; },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, set_compute_system_callback(Eq(mock_handle), _, _))
        .WillRepeatedly(DoAll(
            [this](const hcs_handle_t& target_hcs_system,
                   void* context,
                   void (*callback)(void* hcs_event, void* context)) {

            },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillRepeatedly(DoAll(
            [this](const hcs_handle_t&, mhv::hcs::ComputeSystemState& state) {
                state = mhv::hcs::ComputeSystemState::running;
            },
            Return(hcs_op_result_t{0, L""})));

    ASSERT_NO_THROW(uut = construct_factory());

    auto ptr = uut->create_virtual_machine(desc, stub_key_provider, stub_monitor);
}
