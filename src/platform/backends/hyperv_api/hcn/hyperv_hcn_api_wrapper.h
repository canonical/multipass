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

#ifndef MULTIPASS_HYPERV_API_HCN_WRAPPER
#define MULTIPASS_HYPERV_API_HCN_WRAPPER

#include <hyperv_api/hcn/hyperv_hcn_api_table.h>
#include <hyperv_api/hcn/hyperv_hcn_wrapper_interface.h>

namespace multipass::hyperv::hcn
{

/**
 * A high-level wrapper class that defines
 * the common operations that Host Compute Network
 * API provide.
 */
struct HCNWrapper : public HCNWrapperInterface
{

    /**
     * Construct a new HCNWrapper
     *
     * @param api_table The HCN API table object (optional)
     *
     * The wrapper will use the real HCN API by default.
     */
    HCNWrapper(const HCNAPITable& api_table = {});
    HCNWrapper(const HCNWrapper&) = default;
    HCNWrapper(HCNWrapper&&) = default;
    HCNWrapper& operator=(const HCNWrapper&) = delete;
    HCNWrapper& operator=(HCNWrapper&&) = delete;

    /**
     * Create a new Host Compute Network
     *
     * @param [in] params Parameters for the new network
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult create_network(
        const CreateNetworkParameters& params) const override;

    /**
     * Delete an existing Host Compute Network
     *
     * @param [in] network_guid Target network's GUID
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult delete_network(const std::string& network_guid) const override;

    /**
     * Create a new Host Compute Network Endpoint
     *
     * @param [in] params Parameters for the new endpoint
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult create_endpoint(
        const CreateEndpointParameters& params) const override;

    /**
     * Delete an existing Host Compute Network Endpoint
     *
     * @param [in] params Target endpoint's GUID
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] OperationResult delete_endpoint(const std::string& endpoint_guid) const override;

private:
    const HCNAPITable api{};
};

} // namespace multipass::hyperv::hcn

#endif // MULTIPASS_HYPERV_API_HCN_WRAPPER
