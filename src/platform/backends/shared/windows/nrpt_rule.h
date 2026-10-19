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

#include <multipass/disabled_copy_move.h>

#include <string>
#include <vector>

namespace multipass::hyperv::dns
{
// The DisplayName of the NRPT rules Multipass creates, to tell them apart from others (e.g. to
// clean up after a crash).
inline constexpr auto nrpt_rule_display_name = "Multipass";

// Encodes namespaces as a REG_MULTI_SZ payload, skipping empty ones. Exposed for testing.
std::wstring make_namespace_multi_sz(const std::vector<std::string>& dns_namespaces);

/**
 * A local Windows Name Resolution Policy Table (NRPT) rule: while it lives, the host sends DNS
 * queries for its namespaces (e.g. ".multipass.local") to its name servers.
 *
 * It is written to HKLM\SYSTEM\CurrentControlSet\Services\Dnscache\Parameters\DnsPolicyConfig,
 * like `Add-DnsClientNrptRule` does, which requires administrative privileges.
 */
class NrptRule : private DisabledCopyMove
{
public:
    /**
     * @throws std::invalid_argument without any non-empty namespace or name server.
     * @throws std::runtime_error if the rule cannot be written to the registry.
     */
    NrptRule(const std::vector<std::string>& dns_namespaces,
             const std::vector<std::string>& name_servers);
    NrptRule(const std::string& dns_namespace, const std::vector<std::string>& name_servers);
    ~NrptRule();

    // The rule's registry key, "{GUID}".
    const std::string& guid() const noexcept
    {
        return rule_guid;
    }

private:
    const std::string rule_guid;
};
} // namespace multipass::hyperv::dns
