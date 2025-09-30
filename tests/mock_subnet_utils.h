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

#include "mock_singleton_helpers.h"

#include <multipass/subnet.h>

namespace multipass::test
{
struct MockSubnetUtils : public SubnetUtils
{
    using SubnetUtils::SubnetUtils;

    MOCK_METHOD(Subnet, generate_random_subnet, (uint8_t cidr, Subnet range), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockSubnetUtils, SubnetUtils);
};
} // namespace multipass::test
