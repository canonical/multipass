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

#include "tests/common.h"
#include "tests/hyperv_api/hyperv_test_utils.h"

#include <hyperv_api/hcn/hyperv_hcn_network_policy.h>

namespace hcn = multipass::hyperv::hcn;

namespace multipass::test
{

using uut_t = hcn::HcnNetworkPolicy;

struct HyperVHCNNetworkPolicy_UnitTests : public ::testing::Test
{
};

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNNetworkPolicy_UnitTests, format_narrow)
{
    uut_t uut{hyperv::hcn::HcnNetworkPolicyType::NetAdapterName()};
    uut.settings = hyperv::hcn::HcnNetworkPolicyNetAdapterName{"client eastwood"};

    const auto result = fmt::to_string(uut);
    constexpr auto expected_result = R"json(
        {
            "Type": "NetAdapterName",
            "Settings": {
                "NetworkAdapterName": "client eastwood"
            }
        })json";

    const auto result_nws = trim_whitespace(result.c_str());
    const auto expected_nws = trim_whitespace(expected_result);

    EXPECT_STREQ(result_nws.c_str(), expected_nws.c_str());
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNNetworkPolicy_UnitTests, format_wide)
{
    uut_t uut{hyperv::hcn::HcnNetworkPolicyType::NetAdapterName()};
    uut.settings = hyperv::hcn::HcnNetworkPolicyNetAdapterName{"client eastwood"};

    const auto result = fmt::to_wstring(uut);
    constexpr auto expected_result = LR"json(
        {
            "Type": "NetAdapterName",
            "Settings": {
                "NetworkAdapterName": "client eastwood"
            }
        })json";

    const auto result_nws = trim_whitespace(result.c_str());
    const auto expected_nws = trim_whitespace(expected_result);

    EXPECT_STREQ(result_nws.c_str(), expected_nws.c_str());
}

} // namespace multipass::test
