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

#ifndef MULTIPASS_HYPERV_API_HCS_WRAPPER_INTERFACE_H
#define MULTIPASS_HYPERV_API_HCS_WRAPPER_INTERFACE_H

#include <hyperv_api/hcs/hyperv_hcs_add_endpoint_params.h>
#include <hyperv_api/hcs/hyperv_hcs_compute_system_state.h>
#include <hyperv_api/hcs/hyperv_hcs_create_compute_system_params.h>
#include <hyperv_api/hyperv_api_operation_result.h>

#include <filesystem>
#include <string>

namespace multipass::hyperv::hcs
{

/**
 * Abstract interface for the Host Compute System API wrapper.
 */
struct HCSWrapperInterface
{
    virtual OperationResult create_compute_system(const CreateComputeSystemParameters& params) const = 0;
    virtual OperationResult start_compute_system(const std::string& compute_system_name) const = 0;
    virtual OperationResult shutdown_compute_system(const std::string& compute_system_name) const = 0;
    virtual OperationResult pause_compute_system(const std::string& compute_system_name) const = 0;
    virtual OperationResult resume_compute_system(const std::string& compute_system_name) const = 0;
    virtual OperationResult terminate_compute_system(const std::string& compute_system_name) const = 0;
    virtual OperationResult get_compute_system_properties(const std::string& compute_system_name) const = 0;
    virtual OperationResult grant_vm_access(const std::string& compute_system_name,
                                            const std::filesystem::path& file_path) const = 0;
    virtual OperationResult revoke_vm_access(const std::string& compute_system_name,
                                             const std::filesystem::path& file_path) const = 0;
    virtual OperationResult add_endpoint(const AddEndpointParameters& params) const = 0;
    virtual OperationResult remove_endpoint(const std::string& compute_system_name,
                                            const std::string& endpoint_guid) const = 0;
    virtual OperationResult resize_memory(const std::string& compute_system_name,
                                          const std::uint32_t new_size_mib) const = 0;
    virtual OperationResult update_cpu_count(const std::string& compute_system_name,
                                             const std::uint32_t new_core_count) const = 0;
    virtual OperationResult get_compute_system_state(const std::string& compute_system_name,
                                                     ComputeSystemState& state_out) const = 0;
    virtual ~HCSWrapperInterface() = default;
};
} // namespace multipass::hyperv::hcs

#endif // MULTIPASS_HYPERV_API_HCS_WRAPPER_INTERFACE_H
