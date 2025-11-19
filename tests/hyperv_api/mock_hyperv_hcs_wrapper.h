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

#pragma once

#include <hyperv_api/hcs/hyperv_hcs_api_wrapper.h>

#include "../common.h"
#include "../mock_singleton_helpers.h"

namespace multipass::test
{

/**
 * Mock Host Compute System API wrapper for testing.
 */
struct MockHCSWrapper : public hyperv::hcs::HCSWrapper
{
    using HCSWrapper::HCSWrapper;

    MOCK_METHOD(hyperv::OperationResult,
                open_compute_system,
                (const std::string& compute_system_name,
                 hyperv::hcs::HcsSystemHandle& out_hcs_system),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                create_compute_system,
                (const hyperv::hcs::CreateComputeSystemParameters& params,
                 hyperv::hcs::HcsSystemHandle& out_hcs_system),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                start_compute_system,
                (const hyperv::hcs::HcsSystemHandle& target_hcs_system),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                shutdown_compute_system,
                (const hyperv::hcs::HcsSystemHandle& target_hcs_system),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                pause_compute_system,
                (const hyperv::hcs::HcsSystemHandle& target_hcs_system),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                resume_compute_system,
                (const hyperv::hcs::HcsSystemHandle& target_hcs_system),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                terminate_compute_system,
                (const hyperv::hcs::HcsSystemHandle& target_hcs_system),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                get_compute_system_properties,
                (const hyperv::hcs::HcsSystemHandle& target_hcs_system),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                grant_vm_access,
                (const std::string& compute_system_name, const std::filesystem::path& file_path),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                revoke_vm_access,
                (const std::string& compute_system_name, const std::filesystem::path& file_path),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                get_compute_system_state,
                (const hyperv::hcs::HcsSystemHandle& target_hcs_system,
                 hyperv::hcs::ComputeSystemState& state_out),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                modify_compute_system,
                (const hyperv::hcs::HcsSystemHandle& target_hcs_system,
                 const hyperv::hcs::HcsRequest& request),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                set_compute_system_callback,
                (const hyperv::hcs::HcsSystemHandle& target_hcs_system,
                 void* context,
                 void (*callback)(void* hcs_event, void* context)),
                (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockHCSWrapper, HCSWrapper);
};
} // namespace multipass::test
