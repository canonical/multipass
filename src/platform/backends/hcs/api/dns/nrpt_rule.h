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

#include <string>
#include <vector>

namespace multipass::hyperv::dns
{

/**
 * RAII wrapper around a single local Windows Name Resolution Policy Table (NRPT) rule.
 *
 * An NRPT rule routes DNS queries for a namespace (e.g. ".multipass.test") to a
 * specific set of DNS servers, bypassing the default resolver path. This is the
 * host-side glue that points a custom suffix at the Multipass ICS DNS shim.
 *
 * The rule is written directly to the registry under
 *   HKLM\SYSTEM\CurrentControlSet\Services\Dnscache\Parameters\DnsPolicyConfig\{GUID}
 * which is exactly what `Add-DnsClientNrptRule` does, so no PowerShell dependency is
 * required. The rule is removed on destruction; it is safe to call @ref remove early.
 *
 * Creating a rule requires administrative privileges (write access to HKLM).
 */
class NrptRule
{
public:
    /**
     * Create and apply an NRPT rule.
     *
     * @param dns_namespace The namespace to match, including the leading dot
     *                      (e.g. ".multipass.test").
     * @param name_servers  The DNS server IPs to route the namespace to.
     * @throws std::runtime_error if the rule could not be written to the registry.
     */
    NrptRule(const std::string& dns_namespace, const std::vector<std::string>& name_servers);

    ~NrptRule();

    NrptRule(NrptRule&& other) noexcept;
    NrptRule& operator=(NrptRule&& other) noexcept;

    NrptRule(const NrptRule&) = delete;
    NrptRule& operator=(const NrptRule&) = delete;

    /**
     * Remove the rule from the registry. Idempotent; safe to call more than once.
     */
    void remove() noexcept;

    /**
     * The registry rule identifier, formatted as "{GUID}".
     */
    const std::string& guid() const noexcept
    {
        return rule_guid;
    }

private:
    std::string rule_guid{};
};

} // namespace multipass::hyperv::dns
