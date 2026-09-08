/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tests/unit/common.h"

#include <src/platform/backends/hyperv/hyperv_migration_service.h>

#include <string>
#include <utility>
#include <vector>

namespace mhv = multipass::hyperv;

namespace
{
multipass::NetworkInterfaceInfo make_switch(std::string id, std::vector<std::string> links)
{
    return {.id = std::move(id),
            .type = "switch",
            .description = "vSwitch",
            .links = std::move(links)};
}
} // namespace

TEST(HyperVNetworkTranslation, resolvesSwitchToItsSinglePhysicalAdapterPreservingOrderAndMac)
{
    const std::vector<multipass::NetworkInterface> source{
        {.id = "ExtSwitch (nic-b)", .mac_address = "52:54:00:00:00:0b", .auto_mode = true},
        {.id = "ExtSwitch (nic-a)", .mac_address = "52:54:00:00:00:0a", .auto_mode = false}};

    const std::vector<multipass::NetworkInterfaceInfo> networks{
        make_switch("ExtSwitch (nic-a)", {"Ethernet 1"}),
        make_switch("ExtSwitch (nic-b)", {"Ethernet 2"}),
        {.id = "Ethernet 1", .type = "Ethernet", .description = "adapter"}};

    const auto translated = mhv::translate_extra_interfaces(source, networks);

    ASSERT_EQ(translated.size(), 2u);
    // Order preserved (source order, not networks order).
    EXPECT_EQ(translated[0],
              (multipass::NetworkInterface{.id = "Ethernet 2",
                                           .mac_address = "52:54:00:00:00:0b",
                                           .auto_mode = true}));
    EXPECT_EQ(translated[1],
              (multipass::NetworkInterface{.id = "Ethernet 1",
                                           .mac_address = "52:54:00:00:00:0a",
                                           .auto_mode = false}));
}

TEST(HyperVNetworkTranslation, emptySourceYieldsEmptyResult)
{
    EXPECT_TRUE(mhv::translate_extra_interfaces({}, {make_switch("s", {"eth0"})}).empty());
}

TEST(HyperVNetworkTranslation, failsWhenSwitchIsUnknown)
{
    const std::vector<multipass::NetworkInterface> source{
        {.id = "missing", .mac_address = "52:54:00:00:00:01", .auto_mode = true}};

    EXPECT_THROW((void)mhv::translate_extra_interfaces(source, {make_switch("other", {"eth0"})}),
                 mhv::InstanceMigrationError);
}

TEST(HyperVNetworkTranslation, failsWhenSwitchBridgesNoPhysicalAdapter)
{
    const std::vector<multipass::NetworkInterface> source{
        {.id = "internal", .mac_address = "52:54:00:00:00:01", .auto_mode = true}};

    EXPECT_THROW((void)mhv::translate_extra_interfaces(source, {make_switch("internal", {})}),
                 mhv::InstanceMigrationError);
}

TEST(HyperVNetworkTranslation, failsWhenSwitchBridgesMultiplePhysicalAdapters)
{
    const std::vector<multipass::NetworkInterface> source{
        {.id = "team", .mac_address = "52:54:00:00:00:01", .auto_mode = true}};

    EXPECT_THROW(
        (void)mhv::translate_extra_interfaces(source,
                                              {make_switch("team", {"Ethernet 1", "Ethernet 2"})}),
        mhv::InstanceMigrationError);
}
