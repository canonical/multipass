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

#include <src/platform/backends/hyperv_api/dns/nrpt_rule.h>

// clang-format off
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// clang-format on

#include <string>
#include <vector>

namespace dns = multipass::hyperv::dns;

namespace multipass::test
{

namespace
{
// NRPT rules live under one local key per rule, named by GUID.
constexpr auto nrpt_base =
    LR"(SYSTEM\CurrentControlSet\Services\Dnscache\Parameters\DnsPolicyConfig)";

std::wstring rule_key_path(const std::string& guid)
{
    return std::wstring{nrpt_base} + L"\\" + std::wstring{guid.begin(), guid.end()};
}

bool key_exists(const std::string& guid)
{
    HKEY key{};
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, rule_key_path(guid).c_str(), 0, KEY_READ, &key) !=
        ERROR_SUCCESS)
        return false;
    ::RegCloseKey(key);
    return true;
}

// Reads a registry string value and returns its raw character payload (REG_MULTI_SZ entries are
// separated by embedded NULs).
std::wstring read_value(const std::string& guid, const wchar_t* name)
{
    DWORD bytes{};
    if (::RegGetValueW(HKEY_LOCAL_MACHINE,
                       rule_key_path(guid).c_str(),
                       name,
                       RRF_RT_ANY,
                       nullptr,
                       nullptr,
                       &bytes) != ERROR_SUCCESS)
        return {};
    std::wstring buffer(bytes / sizeof(wchar_t), L'\0');
    if (::RegGetValueW(HKEY_LOCAL_MACHINE,
                       rule_key_path(guid).c_str(),
                       name,
                       RRF_RT_ANY,
                       nullptr,
                       buffer.data(),
                       &bytes) != ERROR_SUCCESS)
        return {};
    return buffer;
}

bool contains(const std::wstring& haystack, const wchar_t* needle)
{
    return haystack.find(needle) != std::wstring::npos;
}
} // namespace

struct HyperVNrptRule_IntegrationTests : public ::testing::Test
{
};

// Writes a real two-suffix NRPT rule to HKLM and verifies the live registry holds both
// namespaces routed to both servers, then that the rule is removed on destruction.
TEST_F(HyperVNrptRule_IntegrationTests, multi_suffix_rule_written_and_removed)
{
    const std::vector<std::string> suffixes{".multipass.zone1", ".multipass.zone2"};
    const std::vector<std::string> servers{"172.50.224.1", "172.50.225.1"};
    std::string guid;

    {
        dns::NrptRule rule{suffixes, servers};
        guid = rule.guid();
        ASSERT_FALSE(guid.empty());

        const auto names = read_value(guid, L"Name");
        EXPECT_TRUE(contains(names, L".multipass.zone1"));
        EXPECT_TRUE(contains(names, L".multipass.zone2"));

        const auto resolvers = read_value(guid, L"GenericDNSServers");
        EXPECT_TRUE(contains(resolvers, L"172.50.224.1"));
        EXPECT_TRUE(contains(resolvers, L"172.50.225.1"));
    }

    EXPECT_FALSE(key_exists(guid)) << "NRPT rule not removed on destruction.";
}

} // namespace multipass::test
