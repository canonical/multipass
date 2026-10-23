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

#include "nrpt_rule.h"
#include "wchar_conversion.h"

#include <multipass/utils.h>

#include <windows.h>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <ranges>
#include <stdexcept>

namespace multipass::hyperv::dns
{
namespace
{
// Local (non-group-policy) NRPT rules, as created by Add-DnsClientNrptRule.
constexpr auto nrpt_base =
    LR"(SYSTEM\CurrentControlSet\Services\Dnscache\Parameters\DnsPolicyConfig)";

constexpr DWORD nrpt_rule_version = 1;
constexpr DWORD nrpt_override_dns = 0x8; // ConfigOptions: query the rule's GenericDNSServers

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

bool non_empty(const std::string& value)
{
    return !value.empty();
}

// Separated by semicolons, as in the registry.
std::string join_non_empty(const std::vector<std::string>& values)
{
    return fmt::format("{}", fmt::join(values | std::views::filter(non_empty), ";"));
}

// Derived from the namespaces, so that a rule left behind (e.g. by a crash) is reused.
std::string rule_guid_for(const std::vector<std::string>& dns_namespaces)
{
    return fmt::format("{{{}}}",
                       multipass::utils::make_uuid(fmt::format("Multipass NRPT rule ({})",
                                                               join_non_empty(dns_namespaces))));
}

std::wstring rule_key_path(const std::string& guid)
{
    return std::wstring{nrpt_base} + L"\\" + widen(guid);
}
} // namespace

std::wstring make_namespace_multi_sz(const std::vector<std::string>& dns_namespaces)
{
    std::wstring multi;
    for (const auto& ns : dns_namespaces | std::views::filter(non_empty))
        multi += widen(ns) + L'\0'; // terminate this entry
    if (multi.empty())
        multi.push_back(L'\0'); // empty list still needs one terminator before the final one
    multi.push_back(L'\0');     // terminate the list
    return multi;
}

NrptRule::NrptRule(const std::string& dns_namespace, const std::vector<std::string>& name_servers)
    : NrptRule{std::vector<std::string>{dns_namespace}, name_servers}
{
}

NrptRule::NrptRule(const std::vector<std::string>& dns_namespaces,
                   const std::vector<std::string>& name_servers)
    : rule_guid{rule_guid_for(dns_namespaces)}
{
    // An empty "Name" makes the rule inert, and an empty "GenericDNSServers" makes every name in
    // the namespaces fail to resolve. Neither is ever intended.
    if (std::ranges::none_of(dns_namespaces, non_empty))
        throw std::invalid_argument{"NRPT: at least one DNS namespace is required"};
    if (std::ranges::none_of(name_servers, non_empty))
        throw std::invalid_argument{"NRPT: at least one name server is required"};

    // Opens the key if it exists, and overwrites all of its values below.
    const auto key_path = rule_key_path(rule_guid);
    HKEY key = nullptr;
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
        throw std::runtime_error{
            fmt::format("NRPT: failed to create registry key (error {})", status)};

    const auto set = [&](const wchar_t* name, DWORD type, const void* data, std::size_t size) {
        if (const auto status = ::RegSetValueExW(key,
                                                 name,
                                                 0,
                                                 type,
                                                 static_cast<const BYTE*>(data),
                                                 static_cast<DWORD>(size));
            status != ERROR_SUCCESS)
        {
            ::RegCloseKey(key);
            ::RegDeleteKeyW(HKEY_LOCAL_MACHINE, key_path.c_str());
            throw std::runtime_error{
                fmt::format("NRPT: failed to set {} (error {})", wchar_to_utf8(name), status)};
        }
    };
    const auto set_string = [&](const wchar_t* name, const std::wstring& value) {
        set(name, REG_SZ, value.c_str(), (value.size() + 1) * sizeof(wchar_t));
    };

    const auto names = make_namespace_multi_sz(dns_namespaces);
    set(L"Version", REG_DWORD, &nrpt_rule_version, sizeof(DWORD));
    set(L"Name", REG_MULTI_SZ, names.data(), names.size() * sizeof(wchar_t));
    set_string(L"GenericDNSServers", widen(join_non_empty(name_servers)));
    set(L"ConfigOptions", REG_DWORD, &nrpt_override_dns, sizeof(DWORD));
    set_string(L"DisplayName", widen(nrpt_rule_display_name));
    set_string(L"Comment", L"Created by Multipass");

    ::RegCloseKey(key);
}

NrptRule::~NrptRule()
{
    ::RegDeleteKeyW(HKEY_LOCAL_MACHINE, rule_key_path(rule_guid).c_str());
}
} // namespace multipass::hyperv::dns
