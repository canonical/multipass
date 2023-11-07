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

#ifndef MULTIPASS_SNAPSHOT_SETTINGS_HANDLER_H
#define MULTIPASS_SNAPSHOT_SETTINGS_HANDLER_H

#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/settings/settings_handler.h>
#include <multipass/snapshot.h>
#include <multipass/virtual_machine.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace multipass
{

class SnapshotSettingsHandler : public SettingsHandler
{
public:
    SnapshotSettingsHandler(std::unordered_map<std::string, VirtualMachine::ShPtr>& operative_instances,
                            const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances,
                            const std::unordered_set<std::string>& preparing_instances) noexcept;

    std::set<QString> keys() const override;
    QString get(const QString& key) const override;
    void set(const QString& key, const QString& val) override;

private:
    std::shared_ptr<const Snapshot> find_snapshot(const std::string& instance_name,
                                                  const std::string& snapshot_name,
                                                  bool deleted_ok = true) const;
    std::shared_ptr<const VirtualMachine> find_instance(const std::string& instance_name, bool deleted_ok = true) const;

    std::shared_ptr<Snapshot> modify_snapshot(const std::string& instance_name, const std::string& snapshot_name);
    std::shared_ptr<VirtualMachine> modify_instance(const std::string& instance_name);

private:
    // references, careful
    std::unordered_map<std::string, VirtualMachine::ShPtr>& operative_instances;
    const std::unordered_map<std::string, VirtualMachine::ShPtr>& deleted_instances;
    const std::unordered_set<std::string>& preparing_instances;
};

class SnapshotSettingsException : public SettingsException
{
public:
    SnapshotSettingsException(const std::string& missing_instance, const std::string& detail);
    explicit SnapshotSettingsException(const std::string& detail);
};

} // namespace multipass

#endif // MULTIPASS_SNAPSHOT_SETTINGS_HANDLER_H
