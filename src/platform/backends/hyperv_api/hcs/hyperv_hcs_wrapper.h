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

#include <hyperv_api/hcs/hyperv_hcs_api.h>
#include <hyperv_api/hcs/hyperv_hcs_compute_system_state.h>
#include <hyperv_api/hcs/hyperv_hcs_create_compute_system_params.h>
#include <hyperv_api/hcs/hyperv_hcs_request.h>
#include <hyperv_api/hcs/hyperv_hcs_system_handle.h>
#include <hyperv_api/hyperv_api_operation_result.h>

#include <multipass/singleton.h>

#include <filesystem>
#include <string>

namespace multipass::hyperv::hcs
{

/**
 * A high-level wrapper class that defines the common operations that Host Compute System API
 * provide.
 */
struct HCSWrapper : public Singleton<HCSWrapper>
{
    HCSWrapper(const Singleton<HCSWrapper>::PrivatePass&) noexcept;
    [[nodiscard]] virtual OperationResult open_compute_system(
        const std::string& compute_system_name,
        HcsSystemHandle& out_hcs_system) const;
    [[nodiscard]] virtual OperationResult create_compute_system(
        const CreateComputeSystemParameters& params,
        HcsSystemHandle& out_hcs_system) const;
    [[nodiscard]] virtual OperationResult start_compute_system(
        const HcsSystemHandle& target_hcs_system) const;
    [[nodiscard]] virtual OperationResult shutdown_compute_system(
        const HcsSystemHandle& target_hcs_system) const;
    [[nodiscard]] virtual OperationResult terminate_compute_system(
        const HcsSystemHandle& target_hcs_system) const;
    [[nodiscard]] virtual OperationResult pause_compute_system(
        const HcsSystemHandle& target_hcs_system) const;
    [[nodiscard]] virtual OperationResult resume_compute_system(
        const HcsSystemHandle& target_hcs_system) const;
    [[nodiscard]] virtual OperationResult get_compute_system_properties(
        const HcsSystemHandle& target_hcs_system) const;
    [[nodiscard]] virtual OperationResult grant_vm_access(
        const std::string& compute_system_name,
        const std::filesystem::path& file_path) const;
    [[nodiscard]] virtual OperationResult revoke_vm_access(
        const std::string& compute_system_name,
        const std::filesystem::path& file_path) const;
    [[nodiscard]] virtual OperationResult get_compute_system_state(
        const HcsSystemHandle& target_hcs_system,
        ComputeSystemState& state_out) const;
    [[nodiscard]] virtual OperationResult modify_compute_system(
        const HcsSystemHandle& target_hcs_system,
        const HcsRequest& request) const;
    [[nodiscard]] virtual OperationResult set_compute_system_callback(
        const HcsSystemHandle& target_hcs_system,
        void* context,
        void (*callback)(void* hcs_event, void* context)) const;
};

inline const HCSWrapper& HCS()
{
    return HCSWrapper::instance();
}

} // namespace multipass::hyperv::hcs
