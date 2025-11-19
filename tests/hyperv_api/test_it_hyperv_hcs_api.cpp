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

#include "tests/common.h"
#include <hyperv_api/hcs/hyperv_hcs_event_type.h>
#include <hyperv_api/hcs/hyperv_hcs_wrapper.h>

#include <fmt/xchar.h>

namespace multipass::test
{

using namespace hyperv::hcs;

struct HyperVHCSAPI_IntegrationTests : public ::testing::Test
{
    hyperv::hcs::HcsSystemHandle handle{nullptr};

    void SetUp() override
    {

        if (HCS().open_compute_system("test", handle))
            (void)HCS().terminate_compute_system(handle);
        handle.reset();
    }
};

TEST_F(HyperVHCSAPI_IntegrationTests, create_delete_compute_system)
{
    hyperv::hcs::ComputeSystemState state{hyperv::hcs::ComputeSystemState::unknown};

    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = "test";
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices.push_back(
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"});
    params.scsi_devices.push_back(
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"});

    const auto c_result = HCS().create_compute_system(params, handle);
    ASSERT_TRUE(c_result);
    ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
    ASSERT_EQ(state, decltype(state)::stopped);

    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    const auto d_result = HCS().terminate_compute_system(handle);
    ASSERT_TRUE(d_result);
    std::wprintf(L"%s\n", d_result.status_msg.c_str());
    handle.reset();
    // Older schema versions does not return anything.
    // ASSERT_FALSE(d_result.status_msg.empty());
}

TEST_F(HyperVHCSAPI_IntegrationTests, pause_resume_compute_system)
{
    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = "test";
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices.push_back(
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"});
    params.scsi_devices.push_back(
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"});

    hyperv::hcs::ComputeSystemState state{hyperv::hcs::ComputeSystemState::unknown};
    ASSERT_TRUE(HCS().create_compute_system(params, handle));
    ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
    ASSERT_EQ(state, decltype(state)::stopped);
    ASSERT_TRUE(HCS().start_compute_system(handle));
    ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
    ASSERT_EQ(state, decltype(state)::running);
    ASSERT_TRUE(HCS().pause_compute_system(handle));
    ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
    ASSERT_EQ(state, decltype(state)::paused);
    ASSERT_TRUE(HCS().resume_compute_system(handle));
    ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
    ASSERT_EQ(state, decltype(state)::running);

    bool called = false;
    ASSERT_TRUE(HCS().set_compute_system_callback(handle, &called, [](void* event, void* context) {
        ASSERT_NE(nullptr, event);
        ASSERT_NE(nullptr, context);
        if (hyperv::hcs::parse_event(event) == hyperv::hcs::HcsEventType::SystemExited)
        {
            *static_cast<bool*>(context) = true;
        }
    }));

    const auto d_result = HCS().terminate_compute_system(handle);
    ASSERT_TRUE(d_result);
    std::wprintf(L"%s\n\n", d_result.status_msg.c_str());
    handle.reset();
    ASSERT_TRUE(called);
    // Older schema versions does not return anything.
    // ASSERT_FALSE(d_result.status_msg.empty());

    ASSERT_FALSE(HCS().get_compute_system_state(handle, state));
}

TEST_F(HyperVHCSAPI_IntegrationTests, enumerate_properties)
{
    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = "test";
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices.push_back(
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"});
    params.scsi_devices.push_back(
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"});

    const auto c_result = HCS().create_compute_system(params, handle);

    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    const auto s_result = HCS().start_compute_system(handle);
    ASSERT_TRUE(s_result);
    ASSERT_TRUE(s_result.status_msg.empty());

    const auto p_result = HCS().get_compute_system_properties(handle);
    EXPECT_TRUE(p_result);
    std::wprintf(L"%s\n", p_result.status_msg.c_str());

    const auto d_result = HCS().terminate_compute_system(handle);
    ASSERT_TRUE(d_result);
    std::wprintf(L"%s\n", d_result.status_msg.c_str());
    handle.reset();
    // Older schema versions does not return anything.
    // ASSERT_FALSE(d_result.status_msg.empty());
}

TEST_F(HyperVHCSAPI_IntegrationTests, add_remove_plan9_share)
{
    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = "test";
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices.push_back(
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"});
    params.scsi_devices.push_back(
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"});

    const auto c_result = HCS().create_compute_system(params, handle);

    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    const auto s_result = HCS().start_compute_system(handle);
    ASSERT_TRUE(s_result);
    ASSERT_TRUE(s_result.status_msg.empty());

    const auto p_result = HCS().get_compute_system_properties(handle);
    EXPECT_TRUE(p_result);
    std::wprintf(L"%s\n", p_result.status_msg.c_str());

    const auto add_9p_req = []() {
        hyperv::hcs::HcsAddPlan9ShareParameters share{};
        share.access_name = "test";
        share.name = "test";
        share.host_path = "C://";
        return hyperv::hcs::HcsRequest{hyperv::hcs::HcsResourcePath::Plan9Shares(),
                                       hyperv::hcs::HcsRequestType::Add(),
                                       share};
    }();

    const auto sh_a_result = HCS().modify_compute_system(handle, add_9p_req);
    EXPECT_TRUE(sh_a_result);
    std::wprintf(L"%s\n", sh_a_result.status_msg.c_str());

    const auto remove_9p_req = []() {
        hyperv::hcs::HcsRemovePlan9ShareParameters share{};
        share.access_name = "test";
        share.name = "test";
        return hyperv::hcs::HcsRequest{hyperv::hcs::HcsResourcePath::Plan9Shares(),
                                       hyperv::hcs::HcsRequestType::Remove(),
                                       share};
    }();

    const auto sh_r_result = HCS().modify_compute_system(handle, remove_9p_req);
    EXPECT_TRUE(sh_r_result);
    std::wprintf(L"%s\n", sh_r_result.status_msg.c_str());

    const auto d_result = HCS().terminate_compute_system(handle);
    ASSERT_TRUE(d_result);
    std::wprintf(L"%s\n", d_result.status_msg.c_str());
    handle.reset();
    // Older schema versions does not return anything.
    // ASSERT_FALSE(d_result.status_msg.empty());
}

TEST_F(HyperVHCSAPI_IntegrationTests, instance_with_snapshots)
{
    hyperv::hcs::ComputeSystemState state{hyperv::hcs::ComputeSystemState::unknown};

    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = "test";
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices.push_back(
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"});
    params.scsi_devices.push_back(
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"});

    const auto c_result = HCS().create_compute_system(params, handle);
    ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
    ASSERT_EQ(state, decltype(state)::stopped);

    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    const auto d_result = HCS().terminate_compute_system(handle);
    ASSERT_TRUE(d_result);
    std::wprintf(L"%s\n", d_result.status_msg.c_str());
    // Older schema versions does not return anything.
    // ASSERT_FALSE(d_result.status_msg.empty());
    handle.reset();
}

} // namespace multipass::test
