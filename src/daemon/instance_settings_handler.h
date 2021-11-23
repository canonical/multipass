/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include "vm_specs.h"

#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/settings/settings_handler.h>
#include <multipass/virtual_machine.h>

#include <QString>

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
                            std::unordered_map<std::string, VirtualMachine::ShPtr>& vm_instances,
                            const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances,
                            const std::unordered_set<std::string>& preparing_instances);

    std::set<QString> keys() const override;
    QString get(const QString& key) const override;
    void set(const QString& key, const QString& val) override;

public:
    enum class Operation
    {
        Obtain,
        Update
    };

private:
    VirtualMachine& find_instance(const std::string& instance_name, Operation operation) const;
    VMSpecs& find_spec(const std::string& instance_name, Operation operation) const;

private:
    // references, careful
    std::unordered_map<std::string, VMSpecs>& vm_instance_specs;
    std::unordered_map<std::string, VirtualMachine::ShPtr>& vm_instances;
    const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances;
    const std::unordered_set<std::string>& preparing_instances;
};

class InstanceSettingsException : public SettingsException
{
public:
    InstanceSettingsException(InstanceSettingsHandler::Operation op, std::string instance, std::string detail);
};

} // namespace multipass

#endif // MULTIPASS_INSTANCE_SETTINGS_HANDLER_H
