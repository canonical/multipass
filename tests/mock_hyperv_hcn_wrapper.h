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

#include "common.h"
#include <hyperv_api/hcn/hyperv_hcn_wrapper_interface.h>

namespace multipass::test
{

/**
 * Mock Host Compute Networking API wrapper for testing.
 */
struct MockHCNWrapper : public hyperv::hcn::HCNWrapperInterface
{
    MOCK_METHOD(hyperv::OperationResult,
                create_network,
                (const struct hyperv::hcn::CreateNetworkParameters& params),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                delete_network,
                (const std::string& network_guid),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                create_endpoint,
                (const struct hyperv::hcn::CreateEndpointParameters& params),
                (const, override));

    MOCK_METHOD(hyperv::OperationResult,
                delete_endpoint,
                (const std::string& endpoint_guid),
                (const, override));
};
} // namespace multipass::test
