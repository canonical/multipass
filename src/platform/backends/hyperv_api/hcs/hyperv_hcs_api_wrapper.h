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

#ifndef MULTIPASS_HYPERV_API_HCS_WRAPPER
#define MULTIPASS_HYPERV_API_HCS_WRAPPER

#include <hyperv_api/hcs/hyperv_hcs_api_table.h>
#include <hyperv_api/hcs/hyperv_hcs_create_compute_system_params.h>
#include <hyperv_api/hcs/hyperv_hcs_wrapper_interface.h>

namespace multipass::hyperv::hcs
{

/**
 * A high-level wrapper class that defines
 * the common operations that Host Compute System
 * API provide.
 */
struct HCSWrapper : public HCSWrapperInterface
{

    /**
     * Construct a new HCNWrapper
     *
     * @param api_table The HCN API table object (optional)
     *
     * The wrapper will use the real HCN API by default.
     */
    HCSWrapper(const HCSAPITable& api_table = {});
    HCSWrapper(const HCSWrapper&) = default;
    HCSWrapper(HCSWrapper&&) = default;
    HCSWrapper& operator=(const HCSWrapper&) = delete;
    HCSWrapper& operator=(HCSWrapper&&) = delete;

    // ---------------------------------------------------------

    /**
     * Create a new Host Compute System
     *
     * @param [in] params Parameters for the new compute system
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult create_compute_system(const CreateComputeSystemParameters& params) const override;

    // ---------------------------------------------------------

    /**
     * Start a compute system.
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult start_compute_system(const std::string& compute_system_name) const override;

    // ---------------------------------------------------------

    /**
     * Gracefully shutdown the compute system
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult shutdown_compute_system(const std::string& compute_system_name) const override;

    // ---------------------------------------------------------

    /**
     * Forcefully shutdown the compute system
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult terminate_compute_system(const std::string& compute_system_name) const override;

    // ---------------------------------------------------------

    /**
     * Pause the execution of a running compute system
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult pause_compute_system(const std::string& compute_system_name) const override;

    // ---------------------------------------------------------

    /**
     * Resume the execution of a previously paused compute system
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult resume_compute_system(const std::string& compute_system_name) const override;

    // ---------------------------------------------------------

    /**
     * Retrieve a Host Compute System's properties
     *
     * @param [in] compute_system_name Target compute system's name
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult get_compute_system_properties(const std::string& compute_system_name) const override;

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
    [[nodiscard]] OperationResult grant_vm_access(const std::string& compute_system_name,
                                                  const std::filesystem::path& file_path) const override;

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
    [[nodiscard]] OperationResult revoke_vm_access(const std::string& compute_system_name,
                                                   const std::filesystem::path& file_path) const override;

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
    [[nodiscard]] OperationResult get_compute_system_state(const std::string& compute_system_name,
                                                           ComputeSystemState& state_out) const override;

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
    [[nodiscard]] OperationResult modify_compute_system(const std::string& compute_system_name,
                                                        const HcsRequest& request) const override;

private:
    const HCSAPITable api{};
};

} // namespace multipass::hyperv::hcs

#endif // MULTIPASS_HYPERV_API_HCS_WRAPPER
