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

#include <hyperv_api/hcn/hyperv_hcn_api_table.h>
#include <hyperv_api/hcn/hyperv_hcn_create_endpoint_params.h>
#include <hyperv_api/hcn/hyperv_hcn_create_network_params.h>
#include <hyperv_api/hyperv_api_operation_result.h>

#include <multipass/singleton.h>

#include <string>

namespace multipass::hyperv::hcn
{

/**
 * A high-level wrapper class that defines
 * the common operations that Host Compute Network
 * API provide.
 */
struct HCNWrapper : public Singleton<HCNWrapper>
{

    /**
     * Construct a new HCNWrapper
     *
     */
    HCNWrapper(const Singleton<HCNWrapper>::PrivatePass&) noexcept;

    /**
     * Create a new Host Compute Network
     *
     * @param [in] params Parameters for the new network
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult create_network(
        const CreateNetworkParameters& params) const;

    /**
     * Delete an existing Host Compute Network
     *
     * @param [in] network_guid Target network's GUID
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult delete_network(const std::string& network_guid) const;

    /**
     * Create a new Host Compute Network Endpoint
     *
     * @param [in] params Parameters for the new endpoint
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult create_endpoint(
        const CreateEndpointParameters& params) const;

    /**
     * Delete an existing Host Compute Network Endpoint
     *
     * @param [in] endpoint_guid Target endpoint's GUID
     *
     * @return An object that evaluates to true on success, false otherwise.
     * message() may contain details of failure when result is false.
     */
    [[nodiscard]] virtual OperationResult delete_endpoint(const std::string& endpoint_guid) const;
};

inline const HCNWrapper& HCN()
{
    return HCNWrapper::instance();
}

} // namespace multipass::hyperv::hcn
