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

#ifndef MULTIPASS_MOCK_FIREWALL_CONFIG
#define MULTIPASS_MOCK_FIREWALL_CONFIG

#include "tests/common.h"
#include "tests/mock_singleton_helpers.h"

#include <src/platform/backends/qemu/linux/firewall_config.h>

namespace multipass
{
namespace test
{
struct MockFirewallConfig : public FirewallConfig
{
    using FirewallConfig::FirewallConfig;

    MOCK_METHOD(void, verify_firewall_rules, (), (override));
};

struct MockFirewallConfigFactory : public FirewallConfigFactory
{
    using FirewallConfigFactory::FirewallConfigFactory;

    MOCK_METHOD(FirewallConfig::UPtr, make_firewall_config, (const QString&, const std::string&), (const, override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockFirewallConfigFactory, FirewallConfigFactory);
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_MOCK_FIREWALL_CONFIG
