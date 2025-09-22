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

#include <multipass/subnet.h>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

TEST(Subnet, canInitializeFromIpCidrPair)
{
    mp::Subnet subnet{mp::IPAddress("192.168.0.0"), 24};

    EXPECT_EQ(subnet.get_identifier(), mp::IPAddress("192.168.0.0"));
    EXPECT_EQ(subnet.get_CIDR(), 24);
}

TEST(Subnet, canInitializeFromString)
{
    mp::Subnet subnet{"192.168.0.0/24"};

    EXPECT_EQ(subnet.get_identifier(), mp::IPAddress("192.168.0.0"));
    EXPECT_EQ(subnet.get_CIDR(), 24);
}

TEST(Subet, throwsOnInvalidIP)
{
    EXPECT_THROW(mp::Subnet{""}, std::invalid_argument);
    EXPECT_THROW(mp::Subnet{"thisisnotanipithinkbuticouldbewrong"}, std::invalid_argument);

    EXPECT_THROW(mp::Subnet{"192.168/16"}, std::invalid_argument);
    EXPECT_THROW(mp::Subnet{"/24"}, std::invalid_argument);
    EXPECT_THROW(mp::Subnet{"/"}, std::invalid_argument);

    MP_EXPECT_THROW_THAT(mp::Subnet{"192.168.XXX.XXX/16"},
                         std::invalid_argument,
                         mpt::match_what(HasSubstr("invalid IP octet")));
}

TEST(Subnet, throwsOnLargeCIDR)
{
    static constexpr auto what = "CIDR value must be non-negative and less than 31";

    // valid but not supported
    MP_EXPECT_THROW_THAT(mp::Subnet{"192.168.0.0/31"},
                         std::invalid_argument,
                         mpt::match_what(HasSubstr(what)));

    // valid but not supported
    MP_EXPECT_THROW_THAT(mp::Subnet{"192.168.0.0/32"},
                         std::invalid_argument,
                         mpt::match_what(HasSubstr(what)));

    // boundary case
    MP_EXPECT_THROW_THAT(mp::Subnet{"192.168.0.0/33"},
                         std::invalid_argument,
                         mpt::match_what(HasSubstr(what)));

    // at 8 bit limit
    MP_EXPECT_THROW_THAT(mp::Subnet{"192.168.0.0/255"},
                         std::invalid_argument,
                         mpt::match_what(HasSubstr(what)));

    // above 8 bit limit
    MP_EXPECT_THROW_THAT(mp::Subnet{"192.168.0.0/895231337"},
                         std::invalid_argument,
                         mpt::match_what(HasSubstr(what)));

    // extreme case
    MP_EXPECT_THROW_THAT(mp::Subnet{"192.168.0.0/895231337890712387952378952359871235987169601436"},
                         std::invalid_argument,
                         mpt::match_what(HasSubstr(what)));
}

TEST(Subnet, throwsOnNegativeCIDR)
{
    static constexpr auto what = "CIDR value must be non-negative and less than 31";

    MP_EXPECT_THROW_THAT(mp::Subnet{"192.168.0.0/-24"},
                         std::invalid_argument,
                         mpt::match_what(HasSubstr(what)));
}

TEST(Subnet, givesCorrectRange)
{
    mp::Subnet subnet{"192.168.0.0/24"};
    EXPECT_EQ(subnet.get_identifier(), mp::IPAddress{"192.168.0.0"});
    EXPECT_EQ(subnet.get_min_address(), mp::IPAddress{"192.168.0.1"});
    EXPECT_EQ(subnet.get_max_address(), mp::IPAddress{"192.168.0.254"});
    EXPECT_EQ(subnet.get_address_count(), 254);

    subnet = mp::Subnet{"121.212.1.152/11"};
    EXPECT_EQ(subnet.get_identifier(), mp::IPAddress{"121.192.0.0"});
    EXPECT_EQ(subnet.get_min_address(), mp::IPAddress{"121.192.0.1"});
    EXPECT_EQ(subnet.get_max_address(), mp::IPAddress{"121,223.255.254"});
    EXPECT_EQ(subnet.get_address_count(), 2097150);

    subnet = mp::Subnet{"0.0.0.0/0"};
    EXPECT_EQ(subnet.get_identifier(), mp::IPAddress{"0.0.0.0"});
    EXPECT_EQ(subnet.get_min_address(), mp::IPAddress{"0.0.0.1"});
    EXPECT_EQ(subnet.get_max_address(), mp::IPAddress{"255,255.255.254"});
    EXPECT_EQ(subnet.get_address_count(), 4294967294);
}

TEST(Subnet, convertsToMaskedIP)
{
    mp::Subnet subnet{"192.168.255.52/24"};
    EXPECT_EQ(subnet.get_identifier(), mp::IPAddress{"192.168.255.0"});

    subnet = mp::Subnet{"255.168.1.152/8"};
    EXPECT_EQ(subnet.get_identifier(), mp::IPAddress{"255.0.0.0"});

    subnet = mp::Subnet{"192.168.1.152/0"};
    EXPECT_EQ(subnet.get_identifier(), mp::IPAddress{"0.0.0.0"});

    subnet = mp::Subnet{"255.212.1.152/13"};
    EXPECT_EQ(subnet.get_identifier(), mp::IPAddress{"255.208.0.0"});
}

TEST(Subnet, getSubnetMaskReturnsSubnetMask)
{
    mp::Subnet subnet{"192.168.0.1/24"};
    EXPECT_EQ(subnet.get_subnet_mask(), mp::IPAddress("255.255.255.0"));

    subnet = mp::Subnet{"192.168.0.1/21"};
    EXPECT_EQ(subnet.get_subnet_mask(), mp::IPAddress("255.255.248.0"));

    subnet = mp::Subnet{"192.168.0.1/16"};
    EXPECT_EQ(subnet.get_subnet_mask(), mp::IPAddress("255.255.0.0"));

    subnet = mp::Subnet{"192.168.0.1/9"};
    EXPECT_EQ(subnet.get_subnet_mask(), mp::IPAddress("255.128.0.0"));

    subnet = mp::Subnet{"192.168.0.1/4"};
    EXPECT_EQ(subnet.get_subnet_mask(), mp::IPAddress("240.0.0.0"));

    subnet = mp::Subnet{"192.168.0.1/0"};
    EXPECT_EQ(subnet.get_subnet_mask(), mp::IPAddress("0.0.0.0"));
}

TEST(Subnet, canConvertToString)
{
    mp::Subnet subnet{"192.168.0.1/24"};
    EXPECT_EQ(subnet.as_string(), "192.168.0.0/24");

    subnet = mp::Subnet{"255.0.255.0/8"};
    EXPECT_EQ(subnet.as_string(), "255.0.0.0/8");

    subnet = mp::Subnet{"255.0.255.0/0"};
    EXPECT_EQ(subnet.as_string(), "0.0.0.0/0");
}
