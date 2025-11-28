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

#include <hyperv_api/hcs/hyperv_hcs_api_table.h>
#include <hyperv_api/hcs/hyperv_hcs_compute_system_state.h>
#include <hyperv_api/hcs/hyperv_hcs_create_compute_system_params.h>
#include <hyperv_api/hcs/hyperv_hcs_request.h>
#include <hyperv_api/hyperv_api_operation_result.h>

#include <multipass/singleton.h>

#include <filesystem>
#include <string>

namespace multipass::hyperv::hcs
{

using HcsSystemHandle = std::shared_ptr<void>;

/**
 * A high-level wrapper class that defines
 * the common operations that Host Compute System
 * API provide.
 */
struct HCSWrapper : public Singleton<HCSWrapper>
{

    /**
     * Construct a new HCNWrapper
     *
     */
    HCSWrapper(const Singleton<HCSWrapper>::PrivatePass&) noexcept;

    // ---------------------------------------------------------

    /**
     * Open an existing Host Compute System
     *
     * @param api The HCS API table
     * @param name Target Host Compute System's name
     *
     * @return auto UniqueHcsSystem non-nullptr on success.
     */
    [[nodiscard]] virtual OperationResult open_compute_system(
        const std::string& compute_system_name,
        HcsSystemHandle& out_hcs_system) const;

    // ---------------------------------------------------------

    /**
     * Create a new Host Compute System
     *
     * @param [in] params Parameters for the new compute system
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult create_compute_system(
        const CreateComputeSystemParameters& params,
        HcsSystemHandle& out_hcs_system) const;

    // ---------------------------------------------------------

    /**
     * Start a compute system.
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult start_compute_system(
        const HcsSystemHandle& target_hcs_system) const;

    // ---------------------------------------------------------

    /**
     * Gracefully shutdown the compute system
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult shutdown_compute_system(
        const HcsSystemHandle& target_hcs_system) const;

    // ---------------------------------------------------------

    /**
     * Forcefully shutdown the compute system
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult terminate_compute_system(
        const HcsSystemHandle& target_hcs_system) const;

    // ---------------------------------------------------------

    /**
     * Pause the execution of a running compute system
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult pause_compute_system(
        const HcsSystemHandle& target_hcs_system) const;

    // ---------------------------------------------------------

    /**
     * Resume the execution of a previously paused compute system
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult resume_compute_system(
        const HcsSystemHandle& target_hcs_system) const;

    // ---------------------------------------------------------

    /**
     * Retrieve a Host Compute System's properties
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult get_compute_system_properties(
        const HcsSystemHandle& target_hcs_system) const;

    // ---------------------------------------------------------

    /**
     * Grant a compute system access to a file path.
     *
     * @param [in] compute_system_name Target compute system's name
     * @param [in] file_path File path to grant access to
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult grant_vm_access(
        const std::string& compute_system_name,
        const std::filesystem::path& file_path) const;

    // ---------------------------------------------------------

    /**
     * Revoke a compute system's access to a file path.
     *
     * @param [in] compute_system_name Target compute system's name
     * @param [in] file_path File path to revoke access to
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult revoke_vm_access(
        const std::string& compute_system_name,
        const std::filesystem::path& file_path) const;

    // ---------------------------------------------------------

    /**
     * Retrieve the current state of the compute system.
     *
     * @param [in] compute_system_name Target compute system's name
     * @param [out] state_out Variable to write the compute system's state
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult get_compute_system_state(
        const HcsSystemHandle& target_hcs_system,
        ComputeSystemState& state_out) const;

    // ---------------------------------------------------------

    /**
     * Modify a Host Compute System request.
     *
     * @param compute_system_name Target compute system name
     * @param request The request
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult modify_compute_system(
        const HcsSystemHandle& target_hcs_system,
        const HcsRequest& request) const;

    // ---------------------------------------------------------

    /**
     * Set compute system callback for a HCS.
     *
     * @param compute_system_name Target compute system name
     * @param context Context, which will be passed to the callback
     * @param callback Callback function
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
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
