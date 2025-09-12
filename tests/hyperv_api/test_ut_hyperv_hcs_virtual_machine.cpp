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
#include <hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <hyperv_api/hcs_virtual_machine.h>

#include "tests/common.h"
#include "tests/mock_hyperv_hcn_wrapper.h"
#include "tests/mock_hyperv_hcs_wrapper.h"
#include "tests/mock_hyperv_virtdisk_wrapper.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_dir.h"
#include "tests/temp_file.h"

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

using uut_t = multipass::hyperv::HCSVirtualMachine;
using hcs_handle_t = multipass::hyperv::hcs::HcsSystemHandle;
using hcs_op_result_t = multipass::hyperv::OperationResult;
using hcs_system_state_t = multipass::hyperv::hcs::ComputeSystemState;

struct HyperVHCSVirtualMachine_UnitTests : public ::testing::Test
{
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mpt::TempDir dummy_instances_dir;
    const std::string dummy_vm_name{"lord-of-the-pings"};
    /**
     *    int num_cores;
    MemorySize mem_size;
    MemorySize disk_space;
    std::string vm_name;
    std::string default_mac_address;
     *
     */
    mp::VirtualMachineDescription desc{2,
                                       mp::MemorySize{"3M"},
                                       mp::MemorySize{}, // not used
                                       dummy_vm_name,
                                       "aa:bb:cc:dd:ee:ff",
                                       {},
                                       "",
                                       {dummy_image.name(), "", "", "", {}, {}},
                                       dummy_cloud_init_iso.name(),
                                       {},
                                       {},
                                       {},
                                       {}};

    mpt::StubSSHKeyProvider stub_key_provider{};
    mpt::StubVMStatusMonitor stub_monitor{};
    std::shared_ptr<mpt::MockHCSWrapper> mock_hcs{std::make_shared<mpt::MockHCSWrapper>()};
    std::shared_ptr<mpt::MockHCNWrapper> mock_hcn{std::make_shared<mpt::MockHCNWrapper>()};
    std::shared_ptr<mpt::MockVirtDiskWrapper> mock_virtdisk{
        std::make_shared<mpt::MockVirtDiskWrapper>()};

    inline static auto mock_handle_raw = reinterpret_cast<void*>(0xbadf00d);
    hcs_handle_t mock_handle{mock_handle_raw, [](void*) {}};
};

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, construct_vm_class_exists_open)
{
    const auto try_construct = [&] {
        return std::make_shared<uut_t>(mock_hcs,
                                       mock_hcn,
                                       mock_virtdisk,
                                       "abcd",
                                       desc,
                                       stub_monitor,
                                       stub_key_provider,
                                       dummy_instances_dir.path());
    };

    EXPECT_CALL(*mock_hcs, open_compute_system(_, _))
        .WillOnce(DoAll(
            [this](const std::string& name, hcs_handle_t& out_handle) {
                ASSERT_EQ(dummy_vm_name, name);
                out_handle = mock_handle;
            },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(*mock_hcs, set_compute_system_callback(Eq(mock_handle), _, _))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    EXPECT_CALL(*mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(DoAll([this](const hcs_handle_t&,
                               hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                        Return(hcs_op_result_t{0, L""})));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = try_construct());

    EXPECT_EQ(uut->current_state(), multipass::VirtualMachine::State::running);
}

// ---------------------------------------------------------

TEST_F(HyperVHCSVirtualMachine_UnitTests, construct_vm_class_exists_create)
{
    const auto try_construct = [&] {
        return std::make_shared<uut_t>(mock_hcs,
                                       mock_hcn,
                                       mock_virtdisk,
                                       "abcd",
                                       desc,
                                       stub_monitor,
                                       stub_key_provider,
                                       dummy_instances_dir.path());
    };

    EXPECT_CALL(*mock_hcs, open_compute_system(_, _))
        .WillOnce(DoAll([this](const std::string& name,
                               hcs_handle_t& out_handle) { ASSERT_EQ(dummy_vm_name, name); },
                        Return(hcs_op_result_t{1, L""})));

    EXPECT_CALL(*mock_hcs, set_compute_system_callback(Eq(mock_handle), _, _))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    EXPECT_CALL(*mock_hcs, get_compute_system_state(Eq(mock_handle), _))
        .WillOnce(DoAll([this](const hcs_handle_t&,
                               hcs_system_state_t& state) { state = hcs_system_state_t::running; },
                        Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(*mock_hcn, delete_endpoint(EndsWith("aabbccddeeff")))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    EXPECT_CALL(*mock_hcn, create_endpoint(_))
        .WillOnce(DoAll(
            [this](const multipass::hyperv::hcn::CreateEndpointParameters& params) {
                // params.endpoint_guid
                ASSERT_TRUE(params.mac_address.has_value());
                ASSERT_EQ(params.mac_address.value(), "aa-bb-cc-dd-ee-ff");
                ASSERT_EQ(params.network_guid, "abcd");
            },
            Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(*mock_virtdisk, list_virtual_disk_chain(Eq(dummy_image.name().toStdString()), _, _))
        .WillOnce(
            DoAll([this](const std::filesystem::path& vhdx_path,
                         std::vector<std::filesystem::path>& chain,
                         std::optional<std::size_t> max_depth) { chain.push_back(vhdx_path); },
                  Return(hcs_op_result_t{0, L""})));

    EXPECT_CALL(*mock_hcs, grant_vm_access(Eq(dummy_vm_name), Eq(dummy_image.name().toStdString())))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    EXPECT_CALL(*mock_hcs,
                grant_vm_access(Eq(dummy_vm_name), Eq(dummy_cloud_init_iso.name().toStdString())))
        .WillOnce(Return(hcs_op_result_t{0, L""}));

    EXPECT_CALL(*mock_hcs, create_compute_system(_, _))
        .WillOnce(DoAll(
            [this](const multipass::hyperv::hcs::CreateComputeSystemParameters& params,
                   hcs_handle_t& out_hcs_system) {
                ASSERT_EQ(params.memory_size_mb, 3);
                ASSERT_EQ(params.name, dummy_vm_name);
                ASSERT_EQ(params.network_adapters.size(), 1);
                ASSERT_EQ(params.processor_count, 2);
                ASSERT_EQ(params.scsi_devices.size(), 2);
                ASSERT_EQ(params.shares.size(), 0);
                out_hcs_system = mock_handle;
            },
            Return(hcs_op_result_t{0, L""})));

    std::shared_ptr<uut_t> uut{nullptr};
    ASSERT_NO_THROW(uut = try_construct());

    EXPECT_EQ(uut->current_state(), multipass::VirtualMachine::State::running);
}
