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

#include "src/platform/backends/shared/macos/backend_utils.h"
#include "tests/mock_process_factory.h"

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
? (192.168.64.255) at ff:ff:ff:ff:ff:ff on bridge100 ifscope [bridge]
? (224.0.0.251) at 1:0:5e:0:0:fb on en0 ifscope permanent [ethernet])"};
}

class getNeighbourIpFixture : public Test
{
public:
    getNeighbourIpFixture()
    {
        mpt::MockProcessFactory::Callback arp_output_callback = [](mpt::MockProcess* process) {
            if (process->program().contains("arp") && process->arguments().contains("-an"))
            {
                EXPECT_CALL(*process, read_all_standard_output()).WillOnce(Return(mock_arp_output_stream));
            }
        };

        mock_process_factory->register_callback(arp_output_callback);
    }

private:
    const std::unique_ptr<mpt::MockProcessFactory::Scope> mock_process_factory{mpt::MockProcessFactory::Inject()};
};

TEST_F(getNeighbourIpFixture, testGetIpArpSuccess)
{
    constexpr auto* existed_mac = "52:54:00:85:72:55";
    constexpr auto* expected_mapped_ip = "192.168.64.3";

    EXPECT_EQ(mp::backend::get_neighbour_ip(existed_mac).value().as_string(), expected_mapped_ip);
}

TEST_F(getNeighbourIpFixture, testGetIpArpFailure)
{
    constexpr auto* non_exist_mac = "11:11:11:11:11:11";
    EXPECT_FALSE(mp::backend::get_neighbour_ip(non_exist_mac).has_value());
}
