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

#ifndef MULTIPASS_INSTANCE_SETTINGS_HANDLER_H
#define MULTIPASS_INSTANCE_SETTINGS_HANDLER_H

#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/network_interface_info.h>
#include <multipass/settings/settings_handler.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_specs.h>

#include <QString>

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace multipass
{

class InstanceSettingsHandler : public SettingsHandler
{
public:
    InstanceSettingsHandler(std::unordered_map<std::string, VMSpecs>& vm_instance_specs,
                            std::unordered_map<std::string, VirtualMachine::ShPtr>& operative_instances,
                            const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances,
                            const std::unordered_set<std::string>& preparing_instances,
                            std::function<void()> instance_persister,
                            std::function<bool(const std::string&)> is_bridged,
                            std::function<void(const std::string&)> add_interface);

    std::set<QString> keys() const override;
    QString get(const QString& key) const override;
    void set(const QString& key, const QString& val) override;

private:
    VirtualMachine& modify_instance(const std::string& instance_name);
    VMSpecs& modify_spec(const std::string& instance_name);
    const VMSpecs& find_spec(const std::string& instance_name) const;

private:
    // references, careful
    std::unordered_map<std::string, VMSpecs>& vm_instance_specs;
    std::unordered_map<std::string, VirtualMachine::ShPtr>& operative_instances;
    const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances;
    const std::unordered_set<std::string>& preparing_instances;
    std::function<void()> instance_persister;
    std::function<bool(const std::string&)> is_bridged;
    std::function<void(const std::string&)> add_interface;
};

class InstanceSettingsException : public SettingsException
{
public:
    InstanceSettingsException(const std::string& reason, const std::string& instance, const std::string& detail);
};

class InstanceStateSettingsException : public InstanceSettingsException
{
public:
    using InstanceSettingsException::InstanceSettingsException;
};

class NonAuthorizedBridgeSettingsException : public InstanceSettingsException
{
public:
    NonAuthorizedBridgeSettingsException(const std::string& reason, const std::string& instance, const std::string& net)
        : InstanceSettingsException{reason, instance, fmt::format("Need user authorization to bridge {}", net)}
    {
    }
};

class BridgeFailureException : public InstanceSettingsException
{
public:
    BridgeFailureException(const std::string& reason, const std::string& instance, const std::string& net)
        : InstanceSettingsException{reason, instance, fmt::format("Failure to bridge {}", net)}
    {
    }
};

} // namespace multipass

#endif // MULTIPASS_INSTANCE_SETTINGS_HANDLER_H
