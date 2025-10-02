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

    EXPECT_EQ(subnet.network_address(), mp::IPAddress("192.168.0.0"));
    EXPECT_EQ(subnet.prefix_length(), 24);
}

TEST(Subnet, canInitializeFromString)
{
    mp::Subnet subnet{"192.168.0.0/24"};

    EXPECT_EQ(subnet.network_address(), mp::IPAddress("192.168.0.0"));
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
    EXPECT_EQ(subnet.network_address(), mp::IPAddress{"192.168.0.0"});
    EXPECT_EQ(subnet.min_address(), mp::IPAddress{"192.168.0.1"});
    EXPECT_EQ(subnet.max_address(), mp::IPAddress{"192.168.0.254"});
    EXPECT_EQ(subnet.usable_address_count(), 254);

    subnet = mp::Subnet{"121.212.1.152/11"};
    EXPECT_EQ(subnet.network_address(), mp::IPAddress{"121.192.0.0"});
    EXPECT_EQ(subnet.min_address(), mp::IPAddress{"121.192.0.1"});
    EXPECT_EQ(subnet.max_address(), mp::IPAddress{"121,223.255.254"});
    EXPECT_EQ(subnet.usable_address_count(), 2097150);

    subnet = mp::Subnet{"0.0.0.0/0"};
    EXPECT_EQ(subnet.network_address(), mp::IPAddress{"0.0.0.0"});
    EXPECT_EQ(subnet.min_address(), mp::IPAddress{"0.0.0.1"});
    EXPECT_EQ(subnet.max_address(), mp::IPAddress{"255,255.255.254"});
    EXPECT_EQ(subnet.usable_address_count(), 4294967294);
}

TEST(Subnet, convertsToMaskedIP)
{
    mp::Subnet subnet{"192.168.255.52/24"};
    EXPECT_EQ(subnet.network_address(), mp::IPAddress{"192.168.255.0"});

    subnet = mp::Subnet{"255.168.1.152/8"};
    EXPECT_EQ(subnet.network_address(), mp::IPAddress{"255.0.0.0"});

    subnet = mp::Subnet{"192.168.1.152/0"};
    EXPECT_EQ(subnet.network_address(), mp::IPAddress{"0.0.0.0"});

    subnet = mp::Subnet{"255.212.1.152/13"};
    EXPECT_EQ(subnet.network_address(), mp::IPAddress{"255.208.0.0"});
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

TEST(Subnet, canConvertToString)
{
    mp::Subnet subnet{"192.168.0.1/24"};
    EXPECT_EQ(subnet.to_cidr(), "192.168.0.0/24");

    subnet = mp::Subnet{"255.0.255.0/8"};
    EXPECT_EQ(subnet.to_cidr(), "255.0.0.0/8");

    subnet = mp::Subnet{"255.0.255.0/0"};
    EXPECT_EQ(subnet.to_cidr(), "0.0.0.0/0");
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

/* TODO C++20 uncomment
TEST(Subnet, relationalComparisonsWorkAsExpected)
{
    const mp::Subnet low{"0.0.0.0/0"};
    const mp::Subnet middle{"192.168.0.0/16"};
    const mp::Subnet submiddle{middle.network_address(), 24};
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
*/

struct SubnetUtils : public Test
{
    SubnetUtils()
    {
        ON_CALL(*mock_platform, subnet_used_locally).WillByDefault(Return(false));
        ON_CALL(*mock_platform, can_reach_gateway).WillByDefault(Return(false));
    }

    mpt::MockUtils::GuardedMock mock_utils_injection{mpt::MockUtils::inject<StrictMock>()};
    mpt::MockUtils* mock_utils = mock_utils_injection.first;

    mpt::MockPlatform::GuardedMock mock_platform_injection = mpt::MockPlatform::inject<NiceMock>();
    mpt::MockPlatform* mock_platform = mock_platform_injection.first;
};

TEST_F(SubnetUtils, generateRandomSubnetTriviallyWorks)
{
    const mp::Subnet range{"10.1.2.0/24"};

    EXPECT_CALL(*mock_utils, random_int(_, _)).WillOnce(Invoke([](auto a, auto b) {
        EXPECT_EQ(a, b);
        return a;
    }));

    mp::Subnet subnet = MP_SUBNET_UTILS.generate_random_subnet(24, range);

    EXPECT_EQ(subnet.network_address(), range.network_address());
    EXPECT_EQ(subnet.prefix_length(), 24);
}

TEST_F(SubnetUtils, generateRandomSubnetFailsOnSmallRange)
{
    mp::Subnet range{"192.168.1.0/16"};

    EXPECT_THROW(std::ignore = MP_SUBNET_UTILS.generate_random_subnet(15, range), std::logic_error);
    EXPECT_THROW(std::ignore = MP_SUBNET_UTILS.generate_random_subnet(0, range), std::logic_error);
}

TEST_F(SubnetUtils, generateRandomSubnetFailsOnBadPrefixLength)
{
    mp::Subnet range{"0.0.0.0/0"};

    EXPECT_THROW(std::ignore = MP_SUBNET_UTILS.generate_random_subnet(31, range),
                 mp::Subnet::PrefixLengthOutOfRange);
    EXPECT_THROW(std::ignore = MP_SUBNET_UTILS.generate_random_subnet(32, range),
                 mp::Subnet::PrefixLengthOutOfRange);
    EXPECT_THROW(std::ignore = MP_SUBNET_UTILS.generate_random_subnet(33, range),
                 mp::Subnet::PrefixLengthOutOfRange);
    EXPECT_THROW(std::ignore = MP_SUBNET_UTILS.generate_random_subnet(255, range),
                 mp::Subnet::PrefixLengthOutOfRange);
}

TEST_F(SubnetUtils, generateRandomSubnetRespectsRange)
{
    mp::Subnet range("192.168.0.0/16");

    auto [mock_utils, guard] = mpt::MockUtils::inject();

    EXPECT_CALL(*mock_utils, random_int(_, _)).WillOnce(ReturnArg<0>()).WillOnce(ReturnArg<1>());

    auto subnetLow = MP_SUBNET_UTILS.generate_random_subnet(24, range);
    auto subnetHigh = MP_SUBNET_UTILS.generate_random_subnet(24, range);

    EXPECT_EQ(subnetLow.network_address(), mp::IPAddress{"192.168.0.0"});
    EXPECT_EQ(subnetLow.prefix_length(), 24);

    EXPECT_EQ(subnetHigh.network_address(), mp::IPAddress{"192.168.255.0"});
    EXPECT_EQ(subnetHigh.prefix_length(), 24);
}

TEST_F(SubnetUtils, generateRandomSubnetWorksAtUpperExtreme)
{
    mp::Subnet range("0.0.0.0/0");

    EXPECT_CALL(*mock_utils, random_int(_, _)).WillOnce(ReturnArg<0>()).WillOnce(ReturnArg<1>());

    auto subnetLow = MP_SUBNET_UTILS.generate_random_subnet(30, range);
    auto subnetHigh = MP_SUBNET_UTILS.generate_random_subnet(30, range);

    EXPECT_EQ(subnetLow.network_address(), mp::IPAddress{"0.0.0.0"});
    EXPECT_EQ(subnetLow.prefix_length(), 30);

    EXPECT_EQ(subnetHigh.network_address(), mp::IPAddress{"255.255.255.252"});
    EXPECT_EQ(subnetHigh.prefix_length(), 30);
}

TEST_F(SubnetUtils, generateRandomSubnetGivesUpUsedLocally)
{
    mp::Subnet range("0.0.0.0/0");

    EXPECT_CALL(*mock_utils, random_int(_, _)).WillRepeatedly(ReturnArg<0>());

    EXPECT_CALL(*mock_platform, subnet_used_locally).WillRepeatedly(Return(true));

    MP_EXPECT_THROW_THAT(std::ignore = MP_SUBNET_UTILS.generate_random_subnet(24, range),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("subnet")));
}

TEST_F(SubnetUtils, generateRandomSubnetGivesUpGatewayReached)
{
    mp::Subnet range("0.0.0.0/0");

    EXPECT_CALL(*mock_utils, random_int(_, _)).WillRepeatedly(ReturnArg<0>());

    EXPECT_CALL(*mock_platform, subnet_used_locally).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_platform, can_reach_gateway).WillRepeatedly(Return(true));

    MP_EXPECT_THROW_THAT(std::ignore = MP_SUBNET_UTILS.generate_random_subnet(24, range),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("subnet")));
}
