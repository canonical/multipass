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

#include "common.h"

#include <multipass/dns_provider.h>

namespace multipass::test
{
class MockDnsProvider : public DnsProvider
{
public:
    MOCK_METHOD(void, start, (const std::string&), (override));
    MOCK_METHOD(void, teardown, (), (noexcept, override));
    MOCK_METHOD(void, set_host, (const std::string&, const std::vector<IPAddress>&), (override));
    MOCK_METHOD(void, remove_host, (const std::string&), (override));
};
} // namespace multipass::test
