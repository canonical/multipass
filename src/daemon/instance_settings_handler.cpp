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
    auto instance_pattern = QStringLiteral("(?<instance>.*)");

    const auto property_template = QStringLiteral("(?<property>%1)");
    auto either_property = QStringList{cpus_suffix, mem_suffix, disk_suffix}.join("|");
    auto property_pattern = property_template.arg(std::move(either_property));

    const auto key_template = QStringLiteral(R"(%1\.%2\.%3)").arg(mp::daemon_settings_root);
    auto inner_key_pattern = key_template.arg(std::move(instance_pattern)).arg(std::move(property_pattern));

    return QRegularExpression{QRegularExpression::anchoredPattern(std::move(inner_key_pattern))};
}

std::pair<QString, QString> parse_key(const QString& key)
{
    static const auto key_regex = make_key_regex();

    auto match = key_regex.match(key);
    if (match.hasMatch())
    {
        auto instance = match.captured("instance");
        auto property = match.captured("property");

        assert(!instance.isEmpty() && !property.isEmpty());
        return {std::move(instance), std::move(property)};
    }

    throw mp::UnrecognizedSettingException{key};
}

void check_state_for_update(mp::VirtualMachine& instance)
{
    auto st = instance.current_state();
    if (st != mp::VirtualMachine::State::stopped && st != mp::VirtualMachine::State::off)
        throw mp::InstanceSettingsException{mp::InstanceSettingsHandler::Operation::Update, instance.vm_name,
                                            "Instance must be stopped for modification"};
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
    const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances)
    : vm_instance_specs{vm_instance_specs}, vm_instances{vm_instances}, deleted_instances{deleted_instances}
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

    auto& instance = find_instance(instance_name, Operation::Update);
    check_state_for_update(instance);
    // TODO@ricab
}

auto mp::InstanceSettingsHandler::find_instance(const QString& name, Operation operation) const -> VirtualMachine&
{
    auto instance_name = name.toStdString();
    try
    {
        auto& ret_ptr = vm_instances.at(instance_name);

        assert(ret_ptr && "can't have null instance");
        return *ret_ptr;
    }
    catch (std::out_of_range&)
    {
        const auto is_deleted = deleted_instances.find(instance_name) != deleted_instances.end();
        const auto reason = is_deleted ? "Instance is deleted" : "No such instance";

        throw InstanceSettingsException{operation, instance_name, reason};
    }
}
