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

#include "tests/unit/mock_availability_zone_manager.h"
#include "tests/unit/mock_process_factory.h"
#include "tests/unit/mock_utils.h"

#include <src/platform/backends/shared/macos/backend_utils.h>

#include <multipass/subnet.h>

namespace mp = multipass;
namespace mpt = multipass::test;

namespace
{
const QByteArray mock_arp_output_stream = QByteArray{R"(
? (192.168.1.1) at 3c:37:86:8a:e6:84 on en0 ifscope [ethernet]
? (192.168.1.255) at ff:ff:ff:ff:ff:ff on en0 ifscope [ethernet]
? (192.168.64.2) at 52:54:0:2a:12:b6 on bridge100 ifscope [bridge]
? (192.168.64.3) at 52:54:0:85:72:55 on bridge100 ifscope [bridge]
? (192.168.64.4) at 52:54:0:e1:cd:ab on bridge100 ifscope [bridge]
? (192.168.64.5) at 50:eb:f6:7f:39:a7 on bridge100 ifscope [bridge]
? (192.168.64.6) at 50:eb:f6:7f:39:a7 on bridge100 ifscope [bridge]
? (192.168.64.255) at ff:ff:ff:ff:ff:ff on bridge100 ifscope [bridge]
? (192.168.2.1) at 18:58:80:a:4a:1c on en0 ifscope [ethernet]
? (192.168.2.1) at be:d0:74:27:1c:64 on bridge100 ifscope permanent [bridge]
? (192.168.2.2) at (incomplete) on en0 ifscope [ethernet]
? (192.168.2.2) at 52:54:0:55:1a:c8 on bridge100 ifscope [bridge]
? (192.168.2.43) at c8:99:b2:77:72:f0 on en0 ifscope [ethernet]
? (192.168.2.100) at 40:6c:8f:20:be:f2 on en0 ifscope [ethernet]
? (192.168.2.105) at 90:48:9a:16:df:14 on en0 ifscope [ethernet]
? (192.168.2.130) at be:c7:5:e7:50:38 on en0 ifscope [ethernet]
? (192.168.2.143) at f6:ac:38:42:84:bd on en0 ifscope [ethernet]
? (192.168.2.154) at 7e:1:62:5a:3f:ab on en0 ifscope [ethernet]
? (192.168.2.182) at e:a2:3d:ee:2a:2e on en0 ifscope permanent [ethernet]
? (192.168.2.185) at da:bd:76:7e:4c:98 on en0 ifscope [ethernet]
? (192.168.2.212) at c8:99:b2:75:25:b0 on en0 ifscope [ethernet]
? (192.168.2.255) at ff:ff:ff:ff:ff:ff on en0 ifscope [ethernet]
? (224.0.0.251) at 1:0:5e:0:0:fb on en0 ifscope permanent [ethernet])"};
}

class GetNeighbourIpFixture : public Test
{
public:
    GetNeighbourIpFixture()
    {
        mpt::MockProcessFactory::Callback arp_output_callback = [](mpt::MockProcess* process) {
            if (process->program().contains("arp") && process->arguments().contains("-an"))
            {
                EXPECT_CALL(*process, read_all_standard_output())
                    .WillOnce(Return(mock_arp_output_stream));
            }
        };

        mock_process_factory->register_callback(arp_output_callback);
    }

private:
    const std::unique_ptr<mpt::MockProcessFactory::Scope> mock_process_factory{
        mpt::MockProcessFactory::Inject()};
};

struct GetNeighbourIPValidInputsTests
    : public GetNeighbourIpFixture,
      public WithParamInterface<std::pair<std::string, std::string>>
{
};

TEST_P(GetNeighbourIPValidInputsTests, validInputCases)
{
    const auto& [existed_mac, expected_mapped_ip] = GetParam();
    auto [mock_utils, utils_guard] = mpt::MockUtils::inject();

    EXPECT_CALL(*mock_utils, run_cmd_for_status(QString("ping"), _, _))
        .WillRepeatedly([](const QString&, const QStringList& args, auto&&) {
            return !args.contains("192.168.64.5");
        });

    EXPECT_EQ(mp::backend::get_neighbour_ip(existed_mac).value().as_string(), expected_mapped_ip);
}

INSTANTIATE_TEST_SUITE_P(GetNeighbourIPTestsInstantiation,
                         GetNeighbourIPValidInputsTests,
                         Values(std::make_pair("52:54:00:2a:12:b6", "192.168.64.2"),
                                std::make_pair("52:54:00:85:72:55", "192.168.64.3"),
                                std::make_pair("52:54:00:e1:cd:ab", "192.168.64.4"),
                                std::make_pair("50:eb:f6:7f:39:a7", "192.168.64.6"),
                                std::make_pair("52:54:00:55:1a:c8", "192.168.2.2"),
                                std::make_pair("01:00:5e:00:00:fb", "224.0.0.251")));

struct GetNeighbourIPInvalidInputsTests : public GetNeighbourIpFixture,
                                          public WithParamInterface<std::string>
{
};

TEST_P(GetNeighbourIPInvalidInputsTests, inValidInputCases)
{
    const auto& non_exist_mac = GetParam();
    EXPECT_FALSE(mp::backend::get_neighbour_ip(non_exist_mac).has_value());
}

INSTANTIATE_TEST_SUITE_P(GetNeighbourIPTestsInstantiation,
                         GetNeighbourIPInvalidInputsTests,
                         Values("11:11:11:11:11:11", "ee:ee:ee:ee:ee:ee"));

TEST(EnableCrossZoneRouting, singleZoneDoesNotTouchForwardingOrPf)
{
    const auto mock_process_factory = mpt::MockProcessFactory::Inject();
    auto [mock_utils, utils_guard] = mpt::MockUtils::inject();

    const mp::Subnet subnet{"192.168.64.0/24"};
    NiceMock<mpt::MockAvailabilityZone> zone;
    ON_CALL(zone, get_subnet()).WillByDefault(ReturnRef(subnet));

    NiceMock<mpt::MockAvailabilityZoneManager> mock_manager;
    const std::vector<std::reference_wrapper<const mp::AvailabilityZone>> zones{zone};
    EXPECT_CALL(mock_manager, get_zones()).WillRepeatedly(Return(zones));

    // Neither IP forwarding nor pf rules should be touched with fewer than two zones.
    EXPECT_CALL(*mock_utils, run_cmd_for_status(_, _, _)).Times(0);

    mp::backend::enable_cross_zone_routing(mock_manager);

    EXPECT_TRUE(mock_process_factory->process_list().empty());
}

TEST(EnableCrossZoneRouting, multipleZonesEnablesForwardingAndInstallsPfRules)
{
    const auto mock_process_factory = mpt::MockProcessFactory::Inject();
    auto [mock_utils, utils_guard] = mpt::MockUtils::inject();

    const mp::Subnet subnet_a{"192.168.64.0/24"};
    const mp::Subnet subnet_b{"192.168.65.0/24"};
    const mp::Subnet subnet_c{"192.168.66.0/24"};

    NiceMock<mpt::MockAvailabilityZone> zone_a, zone_b, zone_c;
    ON_CALL(zone_a, get_subnet()).WillByDefault(ReturnRef(subnet_a));
    ON_CALL(zone_b, get_subnet()).WillByDefault(ReturnRef(subnet_b));
    ON_CALL(zone_c, get_subnet()).WillByDefault(ReturnRef(subnet_c));

    NiceMock<mpt::MockAvailabilityZoneManager> mock_manager;
    const std::vector<std::reference_wrapper<const mp::AvailabilityZone>> zones{zone_a,
                                                                                zone_b,
                                                                                zone_c};
    EXPECT_CALL(mock_manager, get_zones()).WillRepeatedly(Return(zones));

    EXPECT_CALL(
        *mock_utils,
        run_cmd_for_status(QString("sysctl"), Contains(QString("net.inet.ip.forwarding=1")), _))
        .WillOnce(Return(true));

    std::string captured_rules;
    mock_process_factory->register_callback([&captured_rules](mpt::MockProcess* process) {
        if (process->program() == "pfctl")
        {
            EXPECT_THAT(process->arguments(), Contains(QString("-f")));
            EXPECT_CALL(*process, write(_)).WillOnce([&captured_rules](const QByteArray& data) {
                captured_rules = data.toStdString();
                return data.size();
            });
            EXPECT_CALL(*process, wait_for_finished(_)).WillRepeatedly(Return(true));
        }
    });

    mp::backend::enable_cross_zone_routing(mock_manager);

    // N zones produce a "pass" rule for each of the N * (N - 1) ordered, distinct subnet pairs.
    EXPECT_THAT(QString::fromStdString(captured_rules).split('\n', Qt::SkipEmptyParts),
                SizeIs(zones.size() * (zones.size() - 1)));
    for (const auto& src : mock_manager.get_zones())
    {
        for (const auto& dst : mock_manager.get_zones())
        {
            if (&src.get() != &dst.get())
            {
                EXPECT_THAT(captured_rules,
                            HasSubstr(fmt::format("pass quick inet from {} to {} flags any keep "
                                                  "state",
                                                  src.get().get_subnet().canonical().to_cidr(),
                                                  dst.get().get_subnet().canonical().to_cidr())));
            }
            else
            {
                EXPECT_THAT(
                    captured_rules,
                    Not(HasSubstr(fmt::format("from {} to {}",
                                              src.get().get_subnet().canonical().to_cidr(),
                                              dst.get().get_subnet().canonical().to_cidr()))));
            }
        }
    }
}
