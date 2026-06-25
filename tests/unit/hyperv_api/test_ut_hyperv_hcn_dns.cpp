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

#include "tests/unit/common.h"
#include "tests/unit/hyperv_api/hyperv_test_utils.h"

#include <hyperv_api/hcn/hyperv_hcn_dns.h>

namespace hcn = multipass::hyperv::hcn;

namespace multipass::test
{

using uut_t = hcn::HcnDns;

struct HyperVHCNDns_UnitTests : public ::testing::Test
{
};

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNDns_UnitTests, format_narrow)
{
    uut_t uut;
    uut.domain = "multipass.test";
    uut.search = {"multipass.test", "example.test"};
    uut.server_list = {"192.168.1.1", "192.168.1.2"};
    uut.options = {"ndots:1"};
    const auto result = fmt::to_string(uut);
    constexpr auto expected_result = R"json(
        {
            "Domain": "multipass.test",
            "Search": ["multipass.test","example.test"],
            "ServerList": ["192.168.1.1","192.168.1.2"],
            "Options": ["ndots:1"]
        })json";

    const auto result_nws = trim_whitespace(result.c_str());
    const auto expected_nws = trim_whitespace(expected_result);

    EXPECT_STREQ(result_nws.c_str(), expected_nws.c_str());
}

// ---------------------------------------------------------

/**
 * Success scenario: Everything goes as expected.
 */
TEST_F(HyperVHCNDns_UnitTests, format_wide)
{
    uut_t uut;
    uut.domain = "multipass.test";
    uut.search = {"multipass.test", "example.test"};
    uut.server_list = {"192.168.1.1", "192.168.1.2"};
    uut.options = {"ndots:1"};
    const auto result = fmt::to_wstring(uut);
    constexpr auto expected_result = LR"json(
        {
            "Domain": "multipass.test",
            "Search": ["multipass.test","example.test"],
            "ServerList": ["192.168.1.1","192.168.1.2"],
            "Options": ["ndots:1"]
        })json";

    const auto result_nws = trim_whitespace(result.c_str());
    const auto expected_nws = trim_whitespace(expected_result);

    EXPECT_STREQ(result_nws.c_str(), expected_nws.c_str());
}

// ---------------------------------------------------------

/**
 * Success scenario: empty lists render as empty JSON arrays.
 */
TEST_F(HyperVHCNDns_UnitTests, format_empty_lists)
{
    uut_t uut;
    uut.domain = "multipass.test";
    const auto result = fmt::to_string(uut);
    constexpr auto expected_result = R"json(
        {
            "Domain": "multipass.test",
            "Search": [],
            "ServerList": [],
            "Options": []
        })json";

    const auto result_nws = trim_whitespace(result.c_str());
    const auto expected_nws = trim_whitespace(expected_result);

    EXPECT_STREQ(result_nws.c_str(), expected_nws.c_str());
}

} // namespace multipass::test
