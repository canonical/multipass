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

#ifndef MULTIPASS_FIREWALL_CONFIG_H
#define MULTIPASS_FIREWALL_CONFIG_H

#include <multipass/singleton.h>

#include <string>

#include <QString>

namespace multipass
{
class FirewallConfig
{
public:
    using UPtr = std::unique_ptr<FirewallConfig>;

    FirewallConfig(const QString& bridge_name, const std::string& subnet);
    virtual ~FirewallConfig();

    virtual void verify_firewall_rules();

protected:
    FirewallConfig() = default; // for testing

private:
    void clear_all_firewall_rules();

    const QString firewall;
    const QString bridge_name;
    const QString cidr;
    const QString comment;

    bool firewall_error{false};
    std::string error_string;
};

#define MP_FIREWALL_CONFIG_FACTORY multipass::FirewallConfigFactory::instance()

class FirewallConfigFactory : public Singleton<FirewallConfigFactory>
{
public:
    FirewallConfigFactory(const Singleton<FirewallConfigFactory>::PrivatePass& pass) noexcept
        : Singleton<FirewallConfigFactory>::Singleton{pass} {};

    virtual FirewallConfig::UPtr make_firewall_config(const QString& bridge_name, const std::string& subnet) const;
};
} // namespace multipass
#endif // MULTIPASS_FIREWALL_CONFIG_H
