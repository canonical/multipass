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

#include <hyperv_api/dns/nrpt_rule.h>

#include <string>
#include <vector>

namespace dns = multipass::hyperv::dns;

namespace multipass::test
{

struct HyperVNrptRule_UnitTests : public ::testing::Test
{
};

// Split a REG_MULTI_SZ buffer into entries; verifies the trailing double-NUL list terminator.
std::vector<std::wstring> parse_multi_sz(const std::wstring& multi)
{
    std::vector<std::wstring> entries;
    // A well-formed REG_MULTI_SZ ends in two NULs.
    EXPECT_GE(multi.size(), 2u);
    EXPECT_EQ(multi[multi.size() - 1], L'\0');
    EXPECT_EQ(multi[multi.size() - 2], L'\0');

    std::wstring current;
    for (size_t i = 0; i + 1 < multi.size(); ++i)
    {
        if (multi[i] == L'\0')
        {
            entries.push_back(current);
            current.clear();
        }
        else
            current.push_back(multi[i]);
    }
    return entries;
}

// ---------------------------------------------------------

// Multiple suffixes encode as multiple REG_MULTI_SZ entries -- the core of multi-suffix support.
TEST_F(HyperVNrptRule_UnitTests, multiple_suffixes_encode_as_multiple_entries)
{
    const auto multi = dns::make_namespace_multi_sz({".multipass.zone1", ".multipass.zone2"});
    const auto entries = parse_multi_sz(multi);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0], L".multipass.zone1");
    EXPECT_EQ(entries[1], L".multipass.zone2");
}

// ---------------------------------------------------------

TEST_F(HyperVNrptRule_UnitTests, single_suffix_encodes_as_one_entry)
{
    const auto multi = dns::make_namespace_multi_sz({".multipass"});
    const auto entries = parse_multi_sz(multi);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0], L".multipass");
}

// ---------------------------------------------------------

TEST_F(HyperVNrptRule_UnitTests, empty_entries_are_skipped)
{
    const auto multi = dns::make_namespace_multi_sz({".multipass.zone1", "", ".multipass.zone2"});
    const auto entries = parse_multi_sz(multi);
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0], L".multipass.zone1");
    EXPECT_EQ(entries[1], L".multipass.zone2");
}

// ---------------------------------------------------------

TEST_F(HyperVNrptRule_UnitTests, empty_list_is_double_nul_terminated)
{
    const auto multi = dns::make_namespace_multi_sz({});
    ASSERT_EQ(multi.size(), 2u);
    EXPECT_EQ(multi[0], L'\0');
    EXPECT_EQ(multi[1], L'\0');
}

} // namespace multipass::test
