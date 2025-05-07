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

#include <fmt/xchar.h>

#include <src/platform/backends/hyperv_api/hcs/hyperv_hcs_api_wrapper.h>

namespace multipass::test
{

using uut_t = hyperv::hcs::HCSWrapper;

struct HyperVHCSAPI_IntegrationTests : public ::testing::Test
{
    void SetUp() override
    {
        uut_t uut{};
        (void)uut.terminate_compute_system("test");
    }
};

TEST_F(HyperVHCSAPI_IntegrationTests, create_delete_compute_system)
{

    uut_t uut{};

    hyperv::hcs::ComputeSystemState state{hyperv::hcs::ComputeSystemState::unknown};

    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = "test";
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices.push_back(hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"});
    params.scsi_devices.push_back(hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"});

    const auto c_result = uut.create_compute_system(params);
    ASSERT_TRUE(uut.get_compute_system_state(params.name, state));
    ASSERT_EQ(state, decltype(state)::stopped);

    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    const auto d_result = uut.terminate_compute_system(params.name);
    ASSERT_TRUE(d_result);
    std::wprintf(L"%s\n", d_result.status_msg.c_str());
    ASSERT_FALSE(d_result.status_msg.empty());
}

TEST_F(HyperVHCSAPI_IntegrationTests, pause_resume_compute_system)
{

    uut_t uut{};

    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = "test";
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices.push_back(hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"});
    params.scsi_devices.push_back(hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"});

    hyperv::hcs::ComputeSystemState state{hyperv::hcs::ComputeSystemState::unknown};
    ASSERT_TRUE(uut.create_compute_system(params));
    ASSERT_TRUE(uut.get_compute_system_state(params.name, state));
    ASSERT_EQ(state, decltype(state)::stopped);
    ASSERT_TRUE(uut.start_compute_system(params.name));
    ASSERT_TRUE(uut.get_compute_system_state(params.name, state));
    ASSERT_EQ(state, decltype(state)::running);
    ASSERT_TRUE(uut.pause_compute_system(params.name));
    ASSERT_TRUE(uut.get_compute_system_state(params.name, state));
    ASSERT_EQ(state, decltype(state)::paused);
    ASSERT_TRUE(uut.resume_compute_system(params.name));
    ASSERT_TRUE(uut.get_compute_system_state(params.name, state));
    ASSERT_EQ(state, decltype(state)::running);
    const auto d_result = uut.terminate_compute_system(params.name);
    ASSERT_TRUE(d_result);
    std::wprintf(L"%s\n\n", d_result.status_msg.c_str());
    ASSERT_FALSE(d_result.status_msg.empty());

    ASSERT_FALSE(uut.get_compute_system_state(params.name, state));
}

TEST_F(HyperVHCSAPI_IntegrationTests, enumerate_properties)
{

    uut_t uut{};

    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = "test";
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices.push_back(hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"});
    params.scsi_devices.push_back(hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"});

    const auto c_result = uut.create_compute_system(params);

    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    const auto s_result = uut.start_compute_system(params.name);
    ASSERT_TRUE(s_result);
    ASSERT_TRUE(s_result.status_msg.empty());

    const auto p_result = uut.get_compute_system_properties(params.name);
    EXPECT_TRUE(p_result);
    std::wprintf(L"%s\n", p_result.status_msg.c_str());

    // const auto e_result = uut.enumerate_all_compute_systems();
    // EXPECT_TRUE(e_result);
    // std::wprintf(L"%s\n", e_result.status_msg.c_str());

    const auto d_result = uut.terminate_compute_system(params.name);
    ASSERT_TRUE(d_result);
    std::wprintf(L"%s\n", d_result.status_msg.c_str());
    ASSERT_FALSE(d_result.status_msg.empty());
}

TEST_F(HyperVHCSAPI_IntegrationTests, DISABLED_update_cpu_count)
{

    uut_t uut{};

    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = "test";
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices.push_back(hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"});
    params.scsi_devices.push_back(hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"});

    const auto c_result = uut.create_compute_system(params);

    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    const auto s_result = uut.start_compute_system(params.name);
    ASSERT_TRUE(s_result);
    ASSERT_TRUE(s_result.status_msg.empty());

    const auto p_result = uut.get_compute_system_properties(params.name);
    EXPECT_TRUE(p_result);
    std::wprintf(L"%s\n", p_result.status_msg.c_str());

    const auto u_result = uut.update_cpu_count(params.name, 8);
    EXPECT_TRUE(u_result);
    auto v = fmt::format("{}", u_result.code);
    std::wprintf(L"%s\n", u_result.status_msg.c_str());
    std::printf("%s \n", v.c_str());

    // const auto e_result = uut.enumerate_all_compute_systems();
    // EXPECT_TRUE(e_result);
    // std::wprintf(L"%s\n", e_result.status_msg.c_str());

    const auto d_result = uut.terminate_compute_system(params.name);
    ASSERT_TRUE(d_result);
    std::wprintf(L"%s\n", d_result.status_msg.c_str());
    ASSERT_FALSE(d_result.status_msg.empty());
}

TEST_F(HyperVHCSAPI_IntegrationTests, add_remove_plan9_share)
{

    uut_t uut{};

    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = "test";
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices.push_back(hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"});
    params.scsi_devices.push_back(hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"});

    const auto c_result = uut.create_compute_system(params);

    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    const auto s_result = uut.start_compute_system(params.name);
    ASSERT_TRUE(s_result);
    ASSERT_TRUE(s_result.status_msg.empty());

    const auto p_result = uut.get_compute_system_properties(params.name);
    EXPECT_TRUE(p_result);
    std::wprintf(L"%s\n", p_result.status_msg.c_str());

    hyperv::hcs::Plan9ShareParameters share{};
    share.access_name = "test";
    share.name = "test";
    share.host_path = "C://";

    const auto sh_a_result = uut.add_plan9_share(params.name, share);
    EXPECT_TRUE(sh_a_result);
    std::wprintf(L"%s\n", sh_a_result.status_msg.c_str());

    const auto sh_r_result = uut.remove_plan9_share(params.name, share);
    EXPECT_TRUE(sh_r_result);
    std::wprintf(L"%s\n", sh_r_result.status_msg.c_str());

    const auto d_result = uut.terminate_compute_system(params.name);
    ASSERT_TRUE(d_result);
    std::wprintf(L"%s\n", d_result.status_msg.c_str());
    ASSERT_FALSE(d_result.status_msg.empty());
}

} // namespace multipass::test
