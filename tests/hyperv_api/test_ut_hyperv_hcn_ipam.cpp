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

#include <hyperv_api/hcn/hyperv_hcn_ipam.h>

namespace hcn = multipass::hyperv::hcn;

namespace multipass::test
{

using uut_t = hcn::HcnIpam;

struct HyperVHCNIpam_UnitTests : public ::testing::Test
{
};

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNIpam_UnitTests, format_narrow)
{
    uut_t uut;
    uut.type = hyperv::hcn::HcnIpamType::Static();
    uut.subnets.emplace_back(
        hyperv::hcn::HcnSubnet{"192.168.1.0/24", {hcn::HcnRoute{"192.168.1.1", "0.0.0.0/0", 123}}});
    const auto result = fmt::format("{}", uut);
    constexpr auto expected_result = R"json(
        {
            "Type": "static",
            "Subnets": [
                {
                    "Policies": [],
                    "Routes" : [
                        {
                            "NextHop": "192.168.1.1",
                            "DestinationPrefix": "0.0.0.0/0",
                            "Metric": 123
                        }
                    ],
                    "IpAddressPrefix" : "192.168.1.0/24",
                    "IpSubnets": null
                }
            ]
        })json";

    const auto result_nws = trim_whitespace(result.c_str());
    const auto expected_nws = trim_whitespace(expected_result);

    EXPECT_STREQ(result_nws.c_str(), expected_nws.c_str());
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNIpam_UnitTests, format_wide)
{
    uut_t uut;
    uut.type = hyperv::hcn::HcnIpamType::Dhcp();
    uut.subnets.emplace_back(
        hyperv::hcn::HcnSubnet{"192.168.1.0/24", {hcn::HcnRoute{"192.168.1.1", "0.0.0.0/0", 123}}});
    const auto result = fmt::format("{}", uut);
    constexpr auto expected_result = R"json(
        {
            "Type": "DHCP",
            "Subnets": [
                {
                    "Policies": [],
                    "Routes" : [
                        {
                            "NextHop": "192.168.1.1",
                            "DestinationPrefix": "0.0.0.0/0",
                            "Metric": 123
                        }
                    ],
                    "IpAddressPrefix" : "192.168.1.0/24",
                    "IpSubnets": null
                }
            ]
        })json";

    const auto result_nws = trim_whitespace(result.c_str());
    const auto expected_nws = trim_whitespace(expected_result);

    EXPECT_STREQ(result_nws.c_str(), expected_nws.c_str());
}

} // namespace multipass::test
