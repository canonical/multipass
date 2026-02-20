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
#include "mock_file_ops.h"
#include "mock_platform.h"
#include "mock_utils.h"

#include <multipass/subnet.h>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

TEST(Subnet, canInitializeFromIpCidrPair)
{
    mp::Subnet subnet{mp::IPAddress("192.168.0.0"), 24};

    EXPECT_EQ(subnet.masked_address(), mp::IPAddress("192.168.0.0"));
    EXPECT_EQ(subnet.prefix_length(), 24);
}

TEST(Subnet, canInitializeFromString)
{
    mp::Subnet subnet{"192.168.0.0/24"};

    EXPECT_EQ(subnet.masked_address(), mp::IPAddress("192.168.0.0"));
    EXPECT_EQ(subnet.prefix_length(), 24);
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

TEST(Subnet, throwsOnLargePrefixLength)
{
    // valid but not supported
    MP_EXPECT_THROW_THAT(mp::Subnet{"192.168.0.0/31"},
                         mp::Subnet::PrefixLengthOutOfRange,
                         mpt::match_what(HasSubstr("31")));

    MP_EXPECT_THROW_THAT((mp::Subnet{mp::IPAddress{"192.168.0.0"}, 31}),
                         mp::Subnet::PrefixLengthOutOfRange,
                         mpt::match_what(HasSubstr("31")));

    // valid but not supported
    MP_EXPECT_THROW_THAT(mp::Subnet{"192.168.0.0/32"},
                         mp::Subnet::PrefixLengthOutOfRange,
                         mpt::match_what(HasSubstr("32")));

    // boundary case
    MP_EXPECT_THROW_THAT(mp::Subnet{"192.168.0.0/33"},
                         mp::Subnet::PrefixLengthOutOfRange,
                         mpt::match_what(HasSubstr("33")));

    // at 8 bit limit
    EXPECT_THROW(mp::Subnet{"192.168.0.0/255"}, mp::Subnet::PrefixLengthOutOfRange);

    // above 8 bit limit
    EXPECT_THROW(mp::Subnet{"192.168.0.0/895231337"}, mp::Subnet::PrefixLengthOutOfRange);

    // extreme case
    EXPECT_THROW(mp::Subnet{"192.168.0.0/895231337890712387952378952359871235987169601436"},
                 mp::Subnet::PrefixLengthOutOfRange);
}

TEST(Subnet, throwsOnNegativePrefixLength)
{
    EXPECT_THROW(mp::Subnet{"192.168.0.0/-24"}, mp::Subnet::PrefixLengthOutOfRange);
}

TEST(Subnet, givesCorrectRange)
{
    mp::Subnet subnet{"192.168.0.0/24"};
    EXPECT_EQ(subnet.masked_address(), mp::IPAddress{"192.168.0.0"});
    EXPECT_EQ(subnet.min_address(), mp::IPAddress{"192.168.0.1"});
    EXPECT_EQ(subnet.max_address(), mp::IPAddress{"192.168.0.254"});
    EXPECT_EQ(subnet.usable_address_count(), 254);

    subnet = mp::Subnet{"121.212.1.152/11"};
    EXPECT_EQ(subnet.masked_address(), mp::IPAddress{"121.192.0.0"});
    EXPECT_EQ(subnet.min_address(), mp::IPAddress{"121.192.0.1"});
    EXPECT_EQ(subnet.max_address(), mp::IPAddress{"121,223.255.254"});
    EXPECT_EQ(subnet.usable_address_count(), 2097150);

    subnet = mp::Subnet{"0.0.0.0/0"};
    EXPECT_EQ(subnet.masked_address(), mp::IPAddress{"0.0.0.0"});
    EXPECT_EQ(subnet.min_address(), mp::IPAddress{"0.0.0.1"});
    EXPECT_EQ(subnet.max_address(), mp::IPAddress{"255,255.255.254"});
    EXPECT_EQ(subnet.usable_address_count(), 4294967294);
}

TEST(Subnet, getsAddress)
{
    mp::Subnet subnet{"192.168.255.52/24"};
    EXPECT_EQ(subnet.address(), mp::IPAddress{"192.168.255.52"});

    subnet = mp::Subnet{"255.168.1.152/8"};
    EXPECT_EQ(subnet.address(), mp::IPAddress{"255.168.1.152"});

    subnet = mp::Subnet{"192.168.1.152/0"};
    EXPECT_EQ(subnet.address(), mp::IPAddress{"192.168.1.152"});

    subnet = mp::Subnet{"255.212.1.152/13"};
    EXPECT_EQ(subnet.address(), mp::IPAddress{"255.212.1.152"});
}

TEST(Subnet, networkAddressConvertsToMaskedIP)
{
    mp::Subnet subnet{"192.168.255.52/24"};
    EXPECT_EQ(subnet.masked_address(), mp::IPAddress{"192.168.255.0"});

    subnet = mp::Subnet{"255.168.1.152/8"};
    EXPECT_EQ(subnet.masked_address(), mp::IPAddress{"255.0.0.0"});

    subnet = mp::Subnet{"192.168.1.152/0"};
    EXPECT_EQ(subnet.masked_address(), mp::IPAddress{"0.0.0.0"});

    subnet = mp::Subnet{"255.212.1.152/13"};
    EXPECT_EQ(subnet.masked_address(), mp::IPAddress{"255.208.0.0"});
}

TEST(Subnet, getsBroadcastAddress)
{
    mp::Subnet subnet{"192.168.255.52/24"};
    EXPECT_EQ(subnet.broadcast_address(), mp::IPAddress{"192.168.255.255"});

    subnet = mp::Subnet{"255.168.1.152/8"};
    EXPECT_EQ(subnet.broadcast_address(), mp::IPAddress{"255.255.255.255"});

    subnet = mp::Subnet{"192.168.1.152/0"};
    EXPECT_EQ(subnet.broadcast_address(), mp::IPAddress{"255.255.255.255"});

    subnet = mp::Subnet{"255.212.1.152/13"};
    EXPECT_EQ(subnet.broadcast_address(), mp::IPAddress{"255.215.255.255"});
}

TEST(Subnet, getSubnetMaskReturnsSubnetMask)
{
    mp::Subnet subnet{"192.168.0.1/24"};
    EXPECT_EQ(subnet.subnet_mask(), mp::IPAddress("255.255.255.0"));

    subnet = mp::Subnet{"192.168.0.1/21"};
    EXPECT_EQ(subnet.subnet_mask(), mp::IPAddress("255.255.248.0"));

    subnet = mp::Subnet{"192.168.0.1/16"};
    EXPECT_EQ(subnet.subnet_mask(), mp::IPAddress("255.255.0.0"));

    subnet = mp::Subnet{"192.168.0.1/9"};
    EXPECT_EQ(subnet.subnet_mask(), mp::IPAddress("255.128.0.0"));

    subnet = mp::Subnet{"192.168.0.1/4"};
    EXPECT_EQ(subnet.subnet_mask(), mp::IPAddress("240.0.0.0"));

    subnet = mp::Subnet{"192.168.0.1/0"};
    EXPECT_EQ(subnet.subnet_mask(), mp::IPAddress("0.0.0.0"));
}

TEST(Subnet, canonicalConvertsToMaskedIP)
{
    mp::Subnet subnet{"192.168.255.52/24"};
    EXPECT_EQ(subnet.canonical(), mp::Subnet{"192.168.255.0/24"});

    subnet = mp::Subnet{"255.168.1.152/8"};
    EXPECT_EQ(subnet.canonical(), mp::Subnet{"255.0.0.0/8"});

    subnet = mp::Subnet{"192.168.1.152/0"};
    EXPECT_EQ(subnet.canonical(), mp::Subnet{"0.0.0.0/0"});

    subnet = mp::Subnet{"255.212.1.152/13"};
    EXPECT_EQ(subnet.canonical(), mp::Subnet{"255.208.0.0/13"});
}

TEST(Subnet, canConvertToString)
{
    mp::Subnet subnet{"192.168.0.1/24"};
    EXPECT_EQ(subnet.to_cidr(), "192.168.0.1/24");

    subnet = mp::Subnet{"255.0.255.0/8"};
    EXPECT_EQ(subnet.to_cidr(), "255.0.255.0/8");

    subnet = mp::Subnet{"255.0.255.0/0"};
    EXPECT_EQ(subnet.to_cidr(), "255.0.255.0/0");
}

TEST(Subnet, sizeGetsTheRightSize)
{
    mp::Subnet subnet{"192.168.0.1/24"};
    EXPECT_EQ(subnet.size(24), 1);
    EXPECT_EQ(subnet.size(25), 2);
    EXPECT_EQ(subnet.size(30), 64);

    subnet = mp::Subnet{"255.0.255.0/8"};
    EXPECT_EQ(subnet.size(9), 2);
}

TEST(Subnet, sizeHandlesSmallerPrefixLength)
{
    mp::Subnet subnet{"192.168.0.1/24"};
    EXPECT_EQ(subnet.size(23), 0);
    EXPECT_EQ(subnet.size(16), 0);
    EXPECT_EQ(subnet.size(0), 0);

    subnet = mp::Subnet{"255.0.255.0/8"};
    EXPECT_EQ(subnet.size(7), 0);
}

TEST(Subnet, getSpecificSubnetWorks)
{
    mp::Subnet subnet{"192.168.0.1/16"};

    auto res1 = subnet.get_specific_subnet(0, 24);
    EXPECT_EQ(res1.prefix_length(), 24);
    EXPECT_EQ(res1.masked_address(), subnet.masked_address());

    auto res2 = subnet.get_specific_subnet(129, 24);
    EXPECT_EQ(res2.prefix_length(), 24);
    EXPECT_EQ(res2.masked_address(), mp::IPAddress{"192.168.129.0"});

    auto res3 = subnet.get_specific_subnet(subnet.size(20) - 1, 20);
    EXPECT_EQ(res3.prefix_length(), 20);
    EXPECT_EQ(res3.masked_address(), mp::IPAddress{"192.168.240.0"});
}

TEST(Subnet, getSpecificSubnetFailsOnBadIndex)
{
    mp::Subnet subnet{"192.168.0.1/16"};

    EXPECT_THROW(std::ignore = subnet.get_specific_subnet(99999999, 24), std::invalid_argument);
    EXPECT_THROW(std::ignore = subnet.get_specific_subnet(256, 24), std::invalid_argument);
}

TEST(Subnet, getSpecificSubnetFailsOnBadLength)
{
    mp::Subnet subnet{"192.168.0.1/16"};

    EXPECT_THROW(std::ignore = subnet.get_specific_subnet(0, 15), std::logic_error);
    EXPECT_THROW(std::ignore = subnet.get_specific_subnet(0, 0), std::logic_error);
}

TEST(Subnet, containsWorksOnContainedSubnets)
{
    mp::Subnet container{"192.168.0.0/16"};

    // self
    EXPECT_TRUE(container.contains(container));

    // bounds
    mp::Subnet subnet{"192.168.0.0/17"};
    EXPECT_TRUE(container.contains(subnet));

    subnet = mp::Subnet{"192.168.128.0/17"};
    EXPECT_TRUE(container.contains(subnet));

    // sanity cases
    subnet = mp::Subnet{"192.168.72.0/24"};
    EXPECT_TRUE(container.contains(subnet));

    subnet = mp::Subnet{"192.168.123.220/30"};
    EXPECT_TRUE(container.contains(subnet));
}

TEST(Subnet, containsWorksOnUnContainedSubnets)
{
    mp::Subnet subnet{"172.17.0.0/16"};

    // boundary (subset)
    mp::Subnet container{"172.17.0.0/15"};
    EXPECT_FALSE(subnet.contains(container));

    // boundaries (disjoint)
    container = mp::Subnet{"172.16.0.0/16"};
    EXPECT_FALSE(subnet.contains(container));

    container = mp::Subnet{"172.18.0.0/16"};
    EXPECT_FALSE(subnet.contains(container));

    // disjoint
    container = mp::Subnet{"192.168.1.0/24"};
    EXPECT_FALSE(subnet.contains(container));

    container = mp::Subnet{"172.1.0.0/16"};
    EXPECT_FALSE(subnet.contains(container));

    // subset
    container = mp::Subnet{"0.0.0.0/0"};
    EXPECT_FALSE(subnet.contains(container));

    container = mp::Subnet{"172.0.0.0/8"};
    EXPECT_FALSE(subnet.contains(container));
}

TEST(Subnet, containsWorksOnContainedIps)
{
    mp::Subnet subnet{"10.0.0.0/8"};

    // boundaries
    mp::IPAddress ip{"10.0.0.0"};
    EXPECT_TRUE(subnet.contains(ip));

    ip = mp::IPAddress{"10.255.255.255"};
    EXPECT_TRUE(subnet.contains(ip));

    // sanity
    ip = mp::IPAddress{"10.1.2.3"};
    EXPECT_TRUE(subnet.contains(ip));

    ip = mp::IPAddress{"10.168.172.192"};
    EXPECT_TRUE(subnet.contains(ip));
}

TEST(Subnet, containsWorksOnUnContainedIps)
{
    mp::Subnet subnet{"192.168.66.0/24"};

    // boundaries
    mp::IPAddress ip{"192.168.67.0"};
    EXPECT_FALSE(subnet.contains(ip));

    ip = mp::IPAddress{"192.168.65.255"};
    EXPECT_FALSE(subnet.contains(ip));

    // sanity
    ip = mp::IPAddress{"0.0.0.0"};
    EXPECT_FALSE(subnet.contains(ip));

    ip = mp::IPAddress{"255.255.255.255"};
    EXPECT_FALSE(subnet.contains(ip));

    ip = mp::IPAddress{"192.168.1.72"};
    EXPECT_FALSE(subnet.contains(ip));
}

TEST(Subnet, relationalComparisonsWorkAsExpected)
{
    const mp::Subnet low{"0.0.0.0/0"};
    const mp::Subnet middle{"192.168.0.0/16"};
    const mp::Subnet submiddle{middle.masked_address(), 24};
    const mp::Subnet high{"255.255.255.0/24"};

    EXPECT_LT(low, middle);
    EXPECT_LT(low, submiddle);
    EXPECT_LT(low, high);
    EXPECT_LE(low, low);
    EXPECT_GE(low, low);

    EXPECT_GT(high, low);
    EXPECT_GT(high, submiddle);
    EXPECT_GT(high, middle);

    EXPECT_GT(middle, low);
    EXPECT_GT(middle, submiddle);
    EXPECT_LT(middle, high);

    EXPECT_GT(submiddle, low);
    EXPECT_LT(submiddle, middle);
    EXPECT_LT(submiddle, high);
}
