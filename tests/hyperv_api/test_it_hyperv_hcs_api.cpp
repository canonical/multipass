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

#include "hyperv_test_utils.h"
#include "multipass/test_data_path.h"
#include "tests/common.h"
#include <hyperv_api/hcs/hyperv_hcs_event_type.h>
#include <hyperv_api/hcs/hyperv_hcs_wrapper.h>

#include <fmt/xchar.h>

namespace multipass::test
{

using namespace hyperv::hcs;

struct HyperVHCSAPI_IntegrationTests : public ::testing::Test
{
    constexpr static auto test_vm_name = "mp-hvhcs-4493-9555-b423966e78e7";
    hyperv::hcs::HcsSystemHandle handle{nullptr};

    std::optional<decltype(make_tempfile_path({}))> vhdx_path;
    std::filesystem::path cloud_init_iso_path{
        fmt::format("{}/cloud-init/cloud-init.iso", test_data_path)};

    // Something unique to this test.

    void SetUp() override
    {
        cleanup();
        copy_test_vhdx_for_vm();

        ASSERT_TRUE(std::filesystem::exists(*vhdx_path));
        ASSERT_TRUE(std::filesystem::exists(cloud_init_iso_path));
        // Numbers are arbitrary -- just check if non-empty.,
        ASSERT_TRUE(std::filesystem::file_size(*vhdx_path) > 4096);
        ASSERT_TRUE(std::filesystem::file_size(cloud_init_iso_path) > 256);
    }

    void TearDown() override
    {
        if (handle)
        {
            bool called = false;
            ASSERT_TRUE(HCS().set_compute_system_callback(
                handle,
                &called,
                [](HCS_EVENT* event, void* context) {
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
        }
    }

private:
    std::vector<std::filesystem::path> find_split_parts(const std::filesystem::path& dir,
                                                        const std::string& prefix)
    {
        std::vector<std::filesystem::path> parts;
        for (const auto& entry : std::filesystem::directory_iterator(dir))
        {
            if (entry.path().filename().string().starts_with(prefix))
                parts.push_back(entry.path());
        }
        std::ranges::sort(parts); // aa < ab < ac lexicographically
        return parts;
    }

    void merge_files(std::span<const std::filesystem::path> parts,
                     const std::filesystem::path& output)
    {
        std::ofstream out(output, std::ios::binary);
        for (const auto& part : parts)
        {
            std::ifstream in(part, std::ios::binary);
            out << in.rdbuf();
        }
    }

    void copy_test_vhdx_for_vm()
    {
        vhdx_path.emplace(make_tempfile_path(".vhdx"));
        const auto parts =
            find_split_parts(fmt::format("{}/cloud-vhdx", test_data_path), "alpine.vhdx.part-");
        ASSERT_EQ(parts.size(), 3);
        merge_files(parts, *vhdx_path);
    }

    void cleanup()
    {
        // Ensure that the test vm does not exists
        if (HCS().open_compute_system(test_vm_name, handle))
            (void)HCS().terminate_compute_system(handle);
        handle.reset();
    }
};

TEST_F(HyperVHCSAPI_IntegrationTests, create_delete_compute_system)
{
    hyperv::hcs::ComputeSystemState state{hyperv::hcs::ComputeSystemState::unknown};
    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = test_vm_name;
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices = {
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"},
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"}};

    const auto c_result = HCS().create_compute_system(params, handle);
    ASSERT_TRUE(c_result);
    ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
    ASSERT_EQ(state, decltype(state)::stopped);

    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());
}

TEST_F(HyperVHCSAPI_IntegrationTests, pause_resume_compute_system)
{
    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = test_vm_name;
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices = {
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"},
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"}};

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
}

TEST_F(HyperVHCSAPI_IntegrationTests, pause_save_and_resume_compute_system)
{
    const auto temp_savedstate_vmrs_path = make_tempfile_path(".SavedState.vmrs");

    // Create the compute system, pause and save it to the disk
    {
        hyperv::hcs::CreateComputeSystemParameters params{};
        params.name = test_vm_name;
        params.memory_size_mb = 1024;
        params.processor_count = 1;
        params.scsi_devices = {
            hyperv::hcs::HcsScsiDevice{.type = hyperv::hcs::HcsScsiDeviceType::VirtualDisk(),
                                       .name = "Primary disk",
                                       .path = *vhdx_path},
            hyperv::hcs::HcsScsiDevice{.type = hyperv::hcs::HcsScsiDeviceType::Iso(),
                                       .name = "Cloud-init ISO",
                                       .path = cloud_init_iso_path,
                                       .read_only = true}};
        hyperv::hcs::ComputeSystemState state{hyperv::hcs::ComputeSystemState::unknown};
        ASSERT_TRUE(HCS().create_compute_system(params, handle));
        ASSERT_TRUE(HCS().grant_vm_access(params.name, *vhdx_path));
        ASSERT_TRUE(HCS().grant_vm_access(params.name, cloud_init_iso_path));

        ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
        ASSERT_EQ(state, decltype(state)::stopped);
        ASSERT_TRUE(HCS().start_compute_system(handle));
        ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
        ASSERT_EQ(state, decltype(state)::running);
        ASSERT_TRUE(HCS().pause_compute_system(handle));
        ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
        ASSERT_EQ(state, decltype(state)::paused);
        ASSERT_TRUE(HCS().grant_vm_access(
            params.name,
            static_cast<std::filesystem::path>(temp_savedstate_vmrs_path).parent_path()));
        ASSERT_TRUE(HCS().save_compute_system(handle, temp_savedstate_vmrs_path));
        ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
        ASSERT_EQ(state, decltype(state)::paused);
        // Terminate the compute system.
        ASSERT_TRUE(HCS().terminate_compute_system(handle));
        handle.reset();
    }

    // Create the compute system again
    {
        hyperv::hcs::CreateComputeSystemParameters params{};
        params.name = test_vm_name;
        params.memory_size_mb = 1024;
        params.processor_count = 1;
        params.guest_state.save_state_file_path = temp_savedstate_vmrs_path;
        params.scsi_devices = {
            hyperv::hcs::HcsScsiDevice{.type = hyperv::hcs::HcsScsiDeviceType::VirtualDisk(),
                                       .name = "Primary disk",
                                       .path = *vhdx_path},
            hyperv::hcs::HcsScsiDevice{.type = hyperv::hcs::HcsScsiDeviceType::Iso(),
                                       .name = "Cloud-init ISO",
                                       .path = cloud_init_iso_path,
                                       .read_only = true}};
        hyperv::hcs::ComputeSystemState state{hyperv::hcs::ComputeSystemState::unknown};
        ASSERT_TRUE(HCS().create_compute_system(params, handle));
        ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
        ASSERT_TRUE(HCS().start_compute_system(handle));
        ASSERT_EQ(state, decltype(state)::stopped);
    }
}

TEST_F(HyperVHCSAPI_IntegrationTests, enumerate_properties)
{
    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = test_vm_name;
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices = {
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"},
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"}};

    const auto c_result = HCS().create_compute_system(params, handle);

    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());

    const auto s_result = HCS().start_compute_system(handle);
    ASSERT_TRUE(s_result);
    ASSERT_TRUE(s_result.status_msg.empty());

    const auto p_result = HCS().get_compute_system_properties(handle);
    EXPECT_TRUE(p_result);
    std::wprintf(L"%s\n", p_result.status_msg.c_str());
}

// Later.
// TEST_F(HyperVHCSAPI_IntegrationTests, add_remove_plan9_share)
// {
//     hyperv::hcs::CreateComputeSystemParameters params{};
//     params.name = test_vm_name;
//     params.memory_size_mb = 1024;
//     params.processor_count = 1;
//     params.scsi_devices = {
//         hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"},
//         hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"}

//     };

//     const auto c_result = HCS().create_compute_system(params, handle);

//     ASSERT_TRUE(c_result);
//     ASSERT_TRUE(c_result.status_msg.empty());

//     const auto s_result = HCS().start_compute_system(handle);
//     ASSERT_TRUE(s_result);
//     ASSERT_TRUE(s_result.status_msg.empty());

//     const auto p_result = HCS().get_compute_system_properties(handle);
//     EXPECT_TRUE(p_result);
//     std::wprintf(L"%s\n", p_result.status_msg.c_str());

//     const auto add_9p_req = []() {
//         hyperv::hcs::HcsAddPlan9ShareParameters share{};
//         share.access_name = test_vm_name;
//         share.name = test_vm_name;
//         share.host_path = "C://";
//         return hyperv::hcs::HcsRequest{hyperv::hcs::HcsResourcePath::Plan9Shares(),
//                                        hyperv::hcs::HcsRequestType::Add(),
//                                        share};
//     }();

//     const auto sh_a_result = HCS().modify_compute_system(handle, add_9p_req);
//     EXPECT_TRUE(sh_a_result);
//     std::wprintf(L"%s\n", sh_a_result.status_msg.c_str());

//     const auto remove_9p_req = []() {
//         hyperv::hcs::HcsRemovePlan9ShareParameters share{};
//         share.access_name = test_vm_name;
//         share.name = test_vm_name;
//         return hyperv::hcs::HcsRequest{hyperv::hcs::HcsResourcePath::Plan9Shares(),
//                                        hyperv::hcs::HcsRequestType::Remove(),
//                                        share};
//     }();

//     const auto sh_r_result = HCS().modify_compute_system(handle, remove_9p_req);
//     EXPECT_TRUE(sh_r_result);
//     std::wprintf(L"%s\n", sh_r_result.status_msg.c_str());
// }

TEST_F(HyperVHCSAPI_IntegrationTests, instance_with_snapshots)
{
    hyperv::hcs::ComputeSystemState state{hyperv::hcs::ComputeSystemState::unknown};

    hyperv::hcs::CreateComputeSystemParameters params{};
    params.name = test_vm_name;
    params.memory_size_mb = 1024;
    params.processor_count = 1;
    params.scsi_devices = {
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::Iso(), "cloud-init"},
        hyperv::hcs::HcsScsiDevice{hyperv::hcs::HcsScsiDeviceType::VirtualDisk(), "primary"}};

    const auto c_result = HCS().create_compute_system(params, handle);
    ASSERT_TRUE(HCS().get_compute_system_state(handle, state));
    ASSERT_EQ(state, decltype(state)::stopped);

    ASSERT_TRUE(c_result);
    ASSERT_TRUE(c_result.status_msg.empty());
}

} // namespace multipass::test
