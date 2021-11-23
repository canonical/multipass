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

#include "instance_settings_handler.h"

#include <multipass/constants.h>
#include <multipass/exceptions/invalid_memory_size_exception.h>
#include <multipass/format.h>

#include <QRegularExpression>
#include <QStringList>

namespace mp = multipass;

namespace
{
constexpr auto cpus_suffix = "cpus";
constexpr auto mem_suffix = "memory";
constexpr auto disk_suffix = "disk";

std::string operation_msg(mp::InstanceSettingsHandler::Operation op)
{
    return op == mp::InstanceSettingsHandler::Operation::Obtain ? "Cannot obtain instance settings"
                                                                : "Cannot update instance settings";
}

QRegularExpression make_key_regex()
{
    auto instance_pattern = QStringLiteral("(?<instance>.+)");

    const auto property_template = QStringLiteral("(?<property>%1)");
    auto either_property = QStringList{cpus_suffix, mem_suffix, disk_suffix}.join("|");
    auto property_pattern = property_template.arg(std::move(either_property));

    const auto key_template = QStringLiteral(R"(%1\.%2\.%3)").arg(mp::daemon_settings_root);
    auto inner_key_pattern = key_template.arg(std::move(instance_pattern)).arg(std::move(property_pattern));

    return QRegularExpression{QRegularExpression::anchoredPattern(std::move(inner_key_pattern))};
}

std::pair<std::string, std::string> parse_key(const QString& key)
{
    static const auto key_regex = make_key_regex();

    auto match = key_regex.match(key);
    if (match.hasMatch())
    {
        auto instance = match.captured("instance");
        auto property = match.captured("property");

        assert(!instance.isEmpty() && !property.isEmpty());
        return {instance.toStdString(), property.toStdString()};
    }

    throw mp::UnrecognizedSettingException{key};
}

template <typename InstanceMap>
typename InstanceMap::mapped_type&
pick_instance(InstanceMap& instances, const std::string& instance_name,
              mp::InstanceSettingsHandler::Operation operation,
              const std::unordered_map<std::string, mp::VirtualMachine::ShPtr>& deleted = {})
{
    try
    {
        return instances.at(instance_name);
    }
    catch (std::out_of_range&)
    {
        const auto is_deleted = deleted.find(instance_name) != deleted.end();
        const auto reason = is_deleted ? "Instance is deleted" : "No such instance";

        throw mp::InstanceSettingsException{operation, instance_name, reason};
    }
}

void check_state_for_update(mp::VirtualMachine& instance)
{
    auto st = instance.current_state();
    if (st != mp::VirtualMachine::State::stopped && st != mp::VirtualMachine::State::off)
        throw mp::InstanceSettingsException{mp::InstanceSettingsHandler::Operation::Update, instance.vm_name,
                                            "Instance must be stopped for modification"};
}

mp::MemorySize get_memory_size(const QString& key, const QString& val)
{
    try
    {
        return mp::MemorySize{val.toStdString()};
    }
    catch (const mp::InvalidMemorySizeException& e)
    {
        throw mp::InvalidSettingException{key, val, e.what()};
    }
}

void update_cpus(const QString& key, const QString& val, mp::VirtualMachine& instance, mp::VMSpecs& spec)
{
    bool converted_ok = false;
    if (auto cpus = val.toInt(&converted_ok); !converted_ok || cpus < 1)
        throw mp::InvalidSettingException{key, val, "Need a positive decimal integer"};
    else if (cpus < spec.num_cores)
        throw mp::InvalidSettingException{key, val, "The number of cores can only be increased"};
    else if (cpus > spec.num_cores) // NOOP if equal
    {
        instance.update_num_cores(cpus);
        spec.num_cores = cpus;
    }
}

void update_mem(const QString& key, const QString& val, mp::VirtualMachine& instance, mp::VMSpecs& spec,
                const mp::MemorySize& size)
{
    if (size < spec.mem_size)
        throw mp::InvalidSettingException{key, val, "Memory can only be expanded"};
    else if (size > spec.mem_size) // NOOP if equal
    {
        instance.resize_memory(size);
        spec.mem_size = size;
    }
}

void update_disk(const QString& key, const QString& val, mp::VirtualMachine& instance, mp::VMSpecs& spec,
                 const mp::MemorySize& size)
{
    if (size < spec.disk_space)
        throw mp::InvalidSettingException{key, val, "Disk can only be expanded"};
    else if (size > spec.disk_space) // NOOP if equal
    {
        instance.resize_disk(size);
        spec.disk_space = size;
    }
}

} // namespace

mp::InstanceSettingsException::InstanceSettingsException(mp::InstanceSettingsHandler::Operation op,
                                                         std::string instance, std::string detail)
    : SettingsException{
          fmt::format("{}; instance: {}; reason: {}", operation_msg(op), std::move(instance), std::move(detail))}
{
}

mp::InstanceSettingsHandler::InstanceSettingsHandler(
    std::unordered_map<std::string, VMSpecs>& vm_instance_specs,
    std::unordered_map<std::string, VirtualMachine::ShPtr>& vm_instances,
    const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances,
    const std::unordered_set<std::string>& preparing_instances)
    : vm_instance_specs{vm_instance_specs},
      vm_instances{vm_instances},
      deleted_instances{deleted_instances},
      preparing_instances{preparing_instances}
{
}

std::set<QString> mp::InstanceSettingsHandler::keys() const
{
    static constexpr auto instance_placeholder = "<instance-name>"; // actual instances would bloat help text
    static const auto ret = [] {
        std::set<QString> ret;
        const auto key_template = QStringLiteral("%1.%2.%3").arg(daemon_settings_root);
        for (const auto& suffix : {cpus_suffix, mem_suffix, disk_suffix})
            ret.insert(key_template.arg(instance_placeholder).arg(suffix));

        return ret;
    }();

    return ret;
}

QString mp::InstanceSettingsHandler::get(const QString& key) const
{
    return QString(); // TODO@ricab
}

void mp::InstanceSettingsHandler::set(const QString& key, const QString& val)
{
    auto [instance_name, property] = parse_key(key);
    assert(property == cpus_suffix || property == mem_suffix || property == disk_suffix);

    if (preparing_instances.find(instance_name) != preparing_instances.end())
        throw InstanceSettingsException{Operation::Update, instance_name, "Instance is being prepared"};

    auto& instance = find_instance(instance_name, Operation::Update);
    auto& spec = find_spec(instance_name, Operation::Update);
    check_state_for_update(instance);

    if (property == cpus_suffix)
        update_cpus(key, val, instance, spec);
    else
    {
        auto size = get_memory_size(key, val);
        if (property == mem_suffix)
            update_mem(key, val, instance, spec, size);
        else
        {
            assert(property == disk_suffix);
            update_disk(key, val, instance, spec, size);
        }
    }
}

auto mp::InstanceSettingsHandler::find_instance(const std::string& instance_name, Operation operation) const
    -> VirtualMachine&
{
    auto ret = pick_instance(vm_instances, instance_name, operation, deleted_instances);

    assert(ret && "can't have null instance");
    return *ret;
}

auto mp::InstanceSettingsHandler::find_spec(const std::string& instance_name, Operation operation) const -> VMSpecs&
{
    return pick_instance(vm_instance_specs, instance_name, operation);
}
