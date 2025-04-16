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

#include <hyperv_api/hcn/hyperv_hcn_route.h>

namespace hcn = multipass::hyperv::hcn;

namespace multipass::test
{

using uut_t = hcn::HcnRoute;

struct HyperVHCNRoute_UnitTests : public ::testing::Test
{
};

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNRoute_UnitTests, format_narrow)
{
    uut_t uut;
    uut.destination_prefix = "0.0.0.0/0";
    uut.metric = 123;
    uut.next_hop = "192.168.1.1";
    const auto result = fmt::format("{}", uut);
    constexpr auto expected_result = R"json(
        {
            "NextHop": "192.168.1.1",
            "DestinationPrefix": "0.0.0.0/0",
            "Metric": 123
        })json";
    EXPECT_STREQ(result.c_str(), expected_result);
}

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNRoute_UnitTests, format_wide)
{
    uut_t uut;
    uut.destination_prefix = "0.0.0.0/0";
    uut.metric = 123;
    uut.next_hop = "192.168.1.1";
    const auto result = fmt::format(L"{}", uut);
    constexpr auto expected_result = LR"json(
        {
            "NextHop": "192.168.1.1",
            "DestinationPrefix": "0.0.0.0/0",
            "Metric": 123
        })json";
    EXPECT_STREQ(result.c_str(), expected_result);
}

} // namespace multipass::test
