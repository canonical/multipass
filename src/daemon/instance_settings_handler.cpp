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

#include "instance_settings_handler.h"

#include <multipass/cli/prompters.h>
#include <multipass/constants.h>
#include <multipass/exceptions/invalid_memory_size_exception.h>
#include <multipass/settings/bool_setting_spec.h>

#include <QRegularExpression>
#include <QStringList>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto cpus_suffix = "cpus";
constexpr auto mem_suffix = "memory";
constexpr auto disk_suffix = "disk";
constexpr auto bridged_suffix = "bridged";

enum class Operation
{
    Obtain,
    Modify
};

std::string operation_msg(Operation op)
{
    return op == Operation::Obtain ? "Cannot obtain instance settings" : "Cannot update instance settings";
}

QRegularExpression make_key_regex()
{
    const auto instance_pattern = QStringLiteral("(?<instance>.+)");
    const auto prop_template = QStringLiteral("(?<property>%1)");
    const auto either_prop = QStringList{cpus_suffix, mem_suffix, disk_suffix, bridged_suffix}.join("|");
    const auto prop_pattern = prop_template.arg(either_prop);

    const auto key_template = QStringLiteral(R"(%1\.%2\.%3)");
    const auto key_pattern = key_template.arg(mp::daemon_settings_root, instance_pattern, prop_pattern);

    return QRegularExpression{QRegularExpression::anchoredPattern(key_pattern)};
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
pick_instance(InstanceMap& instances, const std::string& instance_name, Operation operation,
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
        assert(!is_deleted || operation == Operation::Modify); // obtaining info from deleted instances is fine

        throw mp::InstanceSettingsException{operation_msg(operation), instance_name, reason};
    }
}

void check_state_for_update(mp::VirtualMachine& instance)
{
    auto st = instance.current_state();
    if (st != mp::VirtualMachine::State::stopped && st != mp::VirtualMachine::State::off)
        throw mp::InstanceStateSettingsException{operation_msg(Operation::Modify),
                                                 instance.vm_name,
                                                 "Instance must be stopped for modification"};
}

mp::MemorySize get_memory_size(const QString& key, const QString& val)
{
    try
    {
        auto ret = mp::MemorySize{val.toStdString()};
        if (ret.in_bytes() == 0)
            throw mp::InvalidSettingException{key, val, "Need a positive size."};

        return ret;
    }
    catch (const mp::InvalidMemorySizeException& e)
    {
        throw mp::InvalidSettingException{key, val, e.what()};
    }
}

void update_cpus(const QString& key, const QString& val, mp::VirtualMachine& instance, mp::VMSpecs& spec)
{
    bool converted_ok = false;
    if (auto cpus = val.toInt(&converted_ok); !converted_ok || cpus < std::stoi(mp::min_cpu_cores))
        throw mp::InvalidSettingException{
            key, val, QString("Need a positive integer (in decimal format) of minimum %1").arg(mp::min_cpu_cores)};
    else if (cpus != spec.num_cores) // NOOP if equal
    {
        instance.update_cpus(cpus);
        spec.num_cores = cpus;
    }
}

void update_mem(const QString& key, const QString& val, mp::VirtualMachine& instance, mp::VMSpecs& spec,
                const mp::MemorySize& size)
{
    if (size < mp::MemorySize{mp::min_memory_size})
        throw mp::InvalidSettingException{key, val,
                                          QString("Memory less than %1 minimum not allowed").arg(mp::min_memory_size)};
    else if (size != spec.mem_size) // NOOP if equal
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

void update_bridged(const QString& key,
                    const QString& val,
                    const std::string& instance_name,
                    std::function<bool(const std::string&)> is_bridged,
                    std::function<void(const std::string&)> add_interface)
{
    auto want_bridged = mp::BoolSettingSpec{key, "false"}.interpret(val) == "true";
    if (!want_bridged)
    {
        if (is_bridged(instance_name)) // inspects host networks once
            throw mp::InvalidSettingException{key, val, "Removing the bridged network is currently not supported"};
    }
    else
        add_interface(instance_name); // if already bridged, this merely warns
}
} // namespace

mp::InstanceSettingsException::InstanceSettingsException(const std::string& reason, const std::string& instance,
                                                         const std::string& detail)
    : SettingsException{fmt::format("{}; instance: {}; reason: {}", reason, instance, detail)}
{
}

mp::InstanceSettingsHandler::InstanceSettingsHandler(
    std::unordered_map<std::string, VMSpecs>& vm_instance_specs,
    std::unordered_map<std::string, VirtualMachine::ShPtr>& operative_instances,
    const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances,
    const std::unordered_set<std::string>& preparing_instances,
    std::function<void()> instance_persister,
    std::function<bool(const std::string&)> is_bridged,
    std::function<void(const std::string&)> add_interface)
    : vm_instance_specs{vm_instance_specs},
      operative_instances{operative_instances},
      deleted_instances{deleted_instances},
      preparing_instances{preparing_instances},
      instance_persister{std::move(instance_persister)},
      is_bridged{is_bridged},
      add_interface{add_interface}
{
}

std::set<QString> mp::InstanceSettingsHandler::keys() const
{
    static const auto key_template = QStringLiteral("%1.%2.%3").arg(daemon_settings_root);

    std::set<QString> ret;
    for (const auto& item : vm_instance_specs)
        for (const auto& suffix : {cpus_suffix, mem_suffix, disk_suffix, bridged_suffix})
            ret.insert(key_template.arg(item.first.c_str()).arg(suffix));

    return ret;
}

QString mp::InstanceSettingsHandler::get(const QString& key) const
{
    auto [instance_name, property] = parse_key(key);
    const auto& spec = find_spec(instance_name);

    if (property == bridged_suffix)
    {
        return is_bridged(instance_name) ? "true" : "false";
    }
    if (property == cpus_suffix)
        return QString::number(spec.num_cores);
    if (property == mem_suffix)
        return QString::fromStdString(spec.mem_size.human_readable()); /* TODO return in bytes when --raw
                                                                          (need unmarshall capability, w/ flag) */

    assert(property == disk_suffix);
    return QString::fromStdString(spec.disk_space.human_readable()); // TODO idem
}

void mp::InstanceSettingsHandler::set(const QString& key, const QString& val)
{
    auto [instance_name, property] = parse_key(key);

    if (preparing_instances.find(instance_name) != preparing_instances.end())
        throw InstanceSettingsException{operation_msg(Operation::Modify), instance_name, "instance is being prepared"};

    auto& instance = modify_instance(instance_name); // we need this first, to refuse updating deleted instances
    auto& spec = modify_spec(instance_name);
    check_state_for_update(instance);

    if (property == cpus_suffix)
        update_cpus(key, val, instance, spec);
    else if (property == bridged_suffix)
    {
        update_bridged(key, val, instance_name, is_bridged, add_interface);
    }
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

    instance_persister();
}

auto mp::InstanceSettingsHandler::modify_instance(const std::string& instance_name) -> VirtualMachine&
{
    auto ret = pick_instance(operative_instances, instance_name, Operation::Modify, deleted_instances);

    assert(ret && "can't have null instance");
    return *ret;
}

auto mp::InstanceSettingsHandler::modify_spec(const std::string& instance_name) -> VMSpecs&
{
    return pick_instance(vm_instance_specs, instance_name, Operation::Modify);
}

auto mp::InstanceSettingsHandler::find_spec(const std::string& instance_name) const -> const VMSpecs&
{
    return pick_instance(vm_instance_specs, instance_name, Operation::Obtain);
}
