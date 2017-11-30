/*
 * Copyright (C) 2017 Canonical, Ltd.
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

#include <multipass/ip_address.h>
#include <multipass/ip_address_pool.h>

#include <gmock/gmock.h>

#include <QTemporaryDir>

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

namespace
{
struct IPAddressPool : public testing::Test
{
    QTemporaryDir data_dir;
};
}

TEST_F(IPAddressPool, allocates_ip)
{
    mp::IPAddress start{"10.120.0.0"};
    mp::IPAddress end{"10.120.0.5"};
    mp::IPAddressPool pool{data_dir.path(), start, end};

    auto ip = pool.obtain_ip_for("test");
    auto ip2 = pool.obtain_ip_for("test");
    auto ip3 = pool.obtain_ip_for("foo");

    EXPECT_THAT(ip, Eq(start));
    EXPECT_THAT(ip, Eq(ip2));
    EXPECT_THAT(ip, Ne(ip3));
}

TEST_F(IPAddressPool, can_remove_ip)
{
    mp::IPAddress start{"10.120.0.0"};
    mp::IPAddress end{"10.120.0.1"};
    mp::IPAddressPool pool{data_dir.path(), start, end};

    // Filling up the pool
    auto ip = pool.obtain_ip_for("a");
    pool.obtain_ip_for("b");

    // should not allow any more ips
    EXPECT_THROW(pool.obtain_ip_for("c"), std::runtime_error);

    // until we remove one
    pool.remove_ip_for("b");

    auto ip3 = pool.obtain_ip_for("d");
    EXPECT_THAT(ip3, Ne(ip));
}

TEST_F(IPAddressPool, can_allocate_all_ips_in_pool)
{
    const int expected_ips = 256;
    mp::IPAddress start{"10.120.0.0"};
    mp::IPAddress end{"10.120.0." + std::to_string(expected_ips - 1)};
    mp::IPAddressPool pool{data_dir.path(), start, end};

    for (int i = 0; i < expected_ips; i++)
    {
        pool.obtain_ip_for(std::to_string(i));
    }

    EXPECT_THROW(pool.obtain_ip_for("one_more"), std::runtime_error);

    pool.remove_ip_for(std::to_string(120));

    EXPECT_NO_THROW(pool.obtain_ip_for("now_it_will_work"));
}

TEST_F(IPAddressPool, persists_records)
{
    mp::IPAddress start{"10.120.0.0"};
    mp::IPAddress end{"10.120.0.1"};

    uint32_t ip_a;
    uint32_t ip_b;
    {
        mp::IPAddressPool pool{data_dir.path(), start, end};
        ip_a = pool.obtain_ip_for("a").as_uint32();
        ip_b = pool.obtain_ip_for("b").as_uint32();
    }

    mp::IPAddressPool pool{data_dir.path(), start, end};

    // If the IPs were persisted then the pool is full and asking for another
    // should fail
    EXPECT_THROW(pool.obtain_ip_for("c"), std::runtime_error);

    // But asking for existing ones should be ok
    auto persisted_ip_a = pool.obtain_ip_for("a");
    auto persisted_ip_b = pool.obtain_ip_for("b");

    EXPECT_THAT(persisted_ip_a.as_uint32(), Eq(ip_a));
    EXPECT_THAT(persisted_ip_b.as_uint32(), Eq(ip_b));
}
