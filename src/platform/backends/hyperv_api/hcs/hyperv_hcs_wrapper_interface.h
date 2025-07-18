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

#include <hyperv_api/hcs/hyperv_hcs_compute_system_state.h>
#include <hyperv_api/hcs/hyperv_hcs_create_compute_system_params.h>
#include <hyperv_api/hcs/hyperv_hcs_network_adapter.h>
#include <hyperv_api/hcs/hyperv_hcs_plan9_share_params.h>
#include <hyperv_api/hcs/hyperv_hcs_request.h>
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
    virtual OperationResult create_compute_system(
        const CreateComputeSystemParameters& params) const = 0;
    virtual OperationResult start_compute_system(const std::string& compute_system_name) const = 0;
    virtual OperationResult shutdown_compute_system(
        const std::string& compute_system_name) const = 0;
    virtual OperationResult pause_compute_system(const std::string& compute_system_name) const = 0;
    virtual OperationResult resume_compute_system(const std::string& compute_system_name) const = 0;
    virtual OperationResult terminate_compute_system(
        const std::string& compute_system_name) const = 0;
    virtual OperationResult get_compute_system_properties(
        const std::string& compute_system_name) const = 0;
    virtual OperationResult grant_vm_access(const std::string& compute_system_name,
                                            const std::filesystem::path& file_path) const = 0;
    virtual OperationResult revoke_vm_access(const std::string& compute_system_name,
                                             const std::filesystem::path& file_path) const = 0;
    virtual OperationResult get_compute_system_state(const std::string& compute_system_name,
                                                     ComputeSystemState& state_out) const = 0;
    virtual OperationResult modify_compute_system(const std::string& compute_system_name,
                                                  const HcsRequest& request) const = 0;
    virtual ~HCSWrapperInterface() = default;
};
} // namespace multipass::hyperv::hcs
