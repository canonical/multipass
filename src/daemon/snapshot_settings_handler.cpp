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

namespace
{
constexpr auto name_suffix = "name";
constexpr auto comment_suffix = "comment";
} // namespace

multipass::SnapshotSettingsHandler::SnapshotSettingsHandler(
    std::unordered_map<std::string, VirtualMachine::ShPtr>& operative_instances,
    const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances,
    const std::unordered_set<std::string>& preparing_instances)
    : operative_instances{operative_instances},
      deleted_instances{deleted_instances},
      preparing_instances{preparing_instances}
{
}

std::set<QString> multipass::SnapshotSettingsHandler::keys() const
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

QString multipass::SnapshotSettingsHandler::get(const QString& key) const
{
    return QString{};
}

void multipass::SnapshotSettingsHandler::set(const QString& key, const QString& val)
{
}
