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

#include "snapshot_settings_handler.h"

#include <multipass/constants.h>

#include <QString>

namespace mp = multipass;

namespace
{
constexpr auto name_suffix = "name";
constexpr auto comment_suffix = "comment";

std::tuple<std::string, QString, std::string> parse_key(const QString& key)
{
    return {"fake_instance", "fake_snapshot", "fake_property"}; // TODO@no-merge
}
} // namespace

mp::SnapshotSettingsHandler::SnapshotSettingsHandler(
    std::unordered_map<std::string, VirtualMachine::ShPtr>& operative_instances,
    const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances,
    const std::unordered_set<std::string>& preparing_instances)
    : operative_instances{operative_instances},
      deleted_instances{deleted_instances},
      preparing_instances{preparing_instances}
{
}

std::set<QString> mp::SnapshotSettingsHandler::keys() const
{
    static const auto key_template = QStringLiteral("%1.%2.%3.%4").arg(daemon_settings_root);
    std::set<QString> ret;

    const auto& const_operative_instances = operative_instances;
    for (const auto* instance_map : {&const_operative_instances, &deleted_instances})
        for (const auto& [vm_name, vm] : *instance_map)
            for (const auto& snapshot : vm->view_snapshots())
                for (const auto& suffix : {name_suffix, comment_suffix})
                    ret.insert(key_template.arg(vm_name.c_str(), snapshot->get_name().c_str(), suffix));

    return ret;
}

QString mp::SnapshotSettingsHandler::get(const QString& key) const
{
    auto [instance_name, snapshot_name, property] = parse_key(key);

    auto snapshot = find_snapshot(instance_name, snapshot_name.toStdString());

    if (property == name_suffix)
        return snapshot_name; // not very useful, but for completeness

    assert(property == comment_suffix);
    return QString::fromStdString(snapshot->get_comment());
}

void mp::SnapshotSettingsHandler::set(const QString& key, const QString& val)
{
}

auto mp::SnapshotSettingsHandler::find_snapshot(const std::string& instance_name,
                                                const std::string& snapshot_name) const
    -> const std::shared_ptr<const Snapshot>
{
    return nullptr;
}
