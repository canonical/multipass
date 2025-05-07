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
     * Add a network adapter to the host compute system.
     *
     * A new network interface card will be automatically created for
     * the endpoint. The network interface card's name will be the
     * endpoint's GUID for convenience.
     *
     * @param [in] compute_system_name Target compute system's name
     * @param [in] params Network adapter parameters
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult add_network_adapter(const std::string& compute_system_name,
                                                      const HcsNetworkAdapter& params) const override;

    // ---------------------------------------------------------

    /**
     * Remove a network adapter from the host compute system.
     *
     * @param [in] name Target compute system's name
     * @param [in] endpoint_guid GUID of the endpoint to remove
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult remove_network_adapter(const std::string& compute_system_name,
                                                         const std::string& endpoint_guid) const override;

    // ---------------------------------------------------------

    /**
     * Resize the amount of memory the compute system has.
     *
     * @param compute_system_name Target compute system name
     * @param new_size_mib New memory size, in megabytes
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult resize_memory(const std::string& compute_system_name,
                                                std::uint32_t new_size_mib) const override;

    // ---------------------------------------------------------

    /**
     * Change the amount of available vCPUs in the compute system
     *
     * @param compute_system_name Target compute system name
     * @param new_size_mib New memory size, in megabytes
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult update_cpu_count(const std::string& compute_system_name,
                                                   std::uint32_t new_vcpu_count) const override;

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
     * Add a Plan9 share to a running system
     *
     * @param [in] compute_system_name Target compute system's name
     * @param [in] params Plan9 share details
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult add_plan9_share(const std::string& compute_system_name,
                                                  const Plan9ShareParameters& params) const override;

    // ---------------------------------------------------------

    /**
     * Remove a Plan9 share from a running system
     *
     * @param [in] compute_system_name Target compute system's name
     * @param [in] params Plan9 share to remove. It's sufficient to fill the
     * name, access_name and port. The rest are redundant for remove.
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult remove_plan9_share(const std::string& compute_system_name,
                                                     const Plan9ShareParameters& params) const override;

private:
    const HCSAPITable api{};
};

} // namespace multipass::hyperv::hcs

#endif // MULTIPASS_HYPERV_API_HCS_WRAPPER
