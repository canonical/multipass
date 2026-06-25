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

#include <hyperv_api/dns/nrpt_rule.h>

// clang-format off
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <rpc.h>
// clang-format on

#include <fmt/format.h>

#include <stdexcept>

namespace multipass::hyperv::dns
{

namespace
{
// Local (non-group-policy) NRPT registry location, as used by Add-DnsClientNrptRule.
constexpr auto nrpt_base = LR"(SYSTEM\CurrentControlSet\Services\Dnscache\Parameters\DnsPolicyConfig)";

// ConfigOptions bitmask value meaning "use the provided override DNS resolvers".
constexpr DWORD nrpt_override_dns = 0x8;

std::wstring widen(const std::string& s)
{
    if (s.empty())
        return {};
    const int size =
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<std::size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), size);
    return out;
}

std::string generate_rule_guid()
{
    UUID uuid{};
    if (::UuidCreate(&uuid) != RPC_S_OK)
        throw std::runtime_error{"NRPT: failed to generate rule GUID"};

    RPC_CSTR str = nullptr;
    if (::UuidToStringA(&uuid, &str) != RPC_S_OK)
        throw std::runtime_error{"NRPT: failed to format rule GUID"};

    std::string result = fmt::format("{{{}}}", reinterpret_cast<char*>(str));
    ::RpcStringFreeA(&str);
    return result;
}

std::wstring rule_key_path(const std::string& guid)
{
    return std::wstring{nrpt_base} + L"\\" + widen(guid);
}
} // namespace

NrptRule::NrptRule(const std::string& dns_namespace, const std::vector<std::string>& name_servers)
    : rule_guid{generate_rule_guid()}
{
    HKEY key = nullptr;
    const auto key_path = rule_key_path(rule_guid);
    if (const auto status = ::RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                                              key_path.c_str(),
                                              0,
                                              nullptr,
                                              REG_OPTION_NON_VOLATILE,
                                              KEY_SET_VALUE,
                                              nullptr,
                                              &key,
                                              nullptr);
        status != ERROR_SUCCESS)
    {
        rule_guid.clear();
        throw std::runtime_error{
            fmt::format("NRPT: failed to create registry key (error {})", status)};
    }

    const auto fail = [&](const char* what, LONG status) {
        ::RegCloseKey(key);
        ::RegDeleteKeyW(HKEY_LOCAL_MACHINE, key_path.c_str());
        rule_guid.clear();
        throw std::runtime_error{fmt::format("NRPT: failed to set {} (error {})", what, status)};
    };

    // Version (DWORD): NRPT rule schema version.
    const DWORD version = 1;
    if (const auto s = ::RegSetValueExW(key,
                                        L"Version",
                                        0,
                                        REG_DWORD,
                                        reinterpret_cast<const BYTE*>(&version),
                                        sizeof(version));
        s != ERROR_SUCCESS)
        fail("Version", s);

    // Name (REG_MULTI_SZ): the matched namespace(s), each with a leading dot.
    {
        std::wstring multi = widen(dns_namespace);
        multi.push_back(L'\0'); // terminate the single entry
        multi.push_back(L'\0'); // terminate the list
        if (const auto s = ::RegSetValueExW(
                key,
                L"Name",
                0,
                REG_MULTI_SZ,
                reinterpret_cast<const BYTE*>(multi.data()),
                static_cast<DWORD>(multi.size() * sizeof(wchar_t)));
            s != ERROR_SUCCESS)
            fail("Name", s);
    }

    // GenericDNSServers (REG_SZ): the override resolvers, ';'-separated.
    {
        std::string joined;
        for (std::size_t i = 0; i < name_servers.size(); ++i)
        {
            if (i != 0)
                joined += ';';
            joined += name_servers[i];
        }
        const std::wstring servers = widen(joined);
        if (const auto s = ::RegSetValueExW(
                key,
                L"GenericDNSServers",
                0,
                REG_SZ,
                reinterpret_cast<const BYTE*>(servers.c_str()),
                static_cast<DWORD>((servers.size() + 1) * sizeof(wchar_t)));
            s != ERROR_SUCCESS)
            fail("GenericDNSServers", s);
    }

    // ConfigOptions (DWORD): direct matching queries to the override resolvers.
    if (const auto s = ::RegSetValueExW(key,
                                        L"ConfigOptions",
                                        0,
                                        REG_DWORD,
                                        reinterpret_cast<const BYTE*>(&nrpt_override_dns),
                                        sizeof(nrpt_override_dns));
        s != ERROR_SUCCESS)
        fail("ConfigOptions", s);

    ::RegCloseKey(key);
}

NrptRule::~NrptRule()
{
    remove();
}

NrptRule::NrptRule(NrptRule&& other) noexcept : rule_guid{std::move(other.rule_guid)}
{
    other.rule_guid.clear();
}

NrptRule& NrptRule::operator=(NrptRule&& other) noexcept
{
    if (this != &other)
    {
        remove();
        rule_guid = std::move(other.rule_guid);
        other.rule_guid.clear();
    }
    return *this;
}

void NrptRule::remove() noexcept
{
    if (rule_guid.empty())
        return;
    ::RegDeleteKeyW(HKEY_LOCAL_MACHINE, rule_key_path(rule_guid).c_str());
    rule_guid.clear();
}

} // namespace multipass::hyperv::dns
