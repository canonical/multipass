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

#include "common.h"

#include <multipass/ip_address.h>

namespace mp = multipass;
using namespace testing;

TEST(IPAddress, can_initialize_from_string)
{
    mp::IPAddress ip{"192.168.1.3"};

    EXPECT_THAT(ip.octets[0], Eq(192));
    EXPECT_THAT(ip.octets[1], Eq(168));
    EXPECT_THAT(ip.octets[2], Eq(1));
    EXPECT_THAT(ip.octets[3], Eq(3));
}

TEST(IPAddress, can_convert_to_string)
{
    mp::IPAddress ip{std::array<uint8_t, 4>{{192, 168, 1, 3}}};

    EXPECT_THAT(ip.as_string(), StrEq("192.168.1.3"));
}

TEST(IPAddress, throws_on_invalid_ip_string)
{
    EXPECT_THROW(mp::IPAddress ip{"100111.3434.3"}, std::invalid_argument);
    EXPECT_THROW(mp::IPAddress ip{"256.256.256.256"}, std::invalid_argument);
    EXPECT_THROW(mp::IPAddress ip{"-2.-3.-5.-6"}, std::invalid_argument);
    EXPECT_THROW(mp::IPAddress ip{"a.b.c.d"}, std::invalid_argument);
}

TEST(IPAddress, can_be_converted_to_integer)
{
    mp::IPAddress ip{std::array<uint8_t, 4>{{0xC0, 0xA8, 0x1, 0x3}}};

    EXPECT_THAT(ip.as_uint32(), Eq(0xC0A80103));
}

TEST(IPAddress, can_use_comparison_operators)
{
    mp::IPAddress low{"10.120.0.0"};
    mp::IPAddress high{"10.120.2.255"};

    EXPECT_TRUE(low != high);
    EXPECT_TRUE(low == low);
    EXPECT_TRUE(low < high);
    EXPECT_TRUE(low <= low);
    EXPECT_TRUE(high > low);
    EXPECT_TRUE(high >= high);
}

TEST(IPAddress, supports_addition_operator)
{
    mp::IPAddress an_ip{"10.120.0.255"};
    mp::IPAddress expected_ip{"10.120.1.3"};

    auto result_ip = an_ip + 4;
    EXPECT_THAT(result_ip, Eq(expected_ip));
}
