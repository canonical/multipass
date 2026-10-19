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

#include <multipass/dns_provider.h>

namespace multipass
{
// For platforms that cannot make instances resolvable by name: does nothing.
class NullDnsProvider final : public DnsProvider
{
public:
    void start(const std::string&) override
    {
    }

    void teardown() noexcept override
    {
    }

    void set_host(const std::string&, const std::vector<IPAddress>&) override
    {
    }

    void remove_host(const std::string&) override
    {
    }
};
} // namespace multipass
