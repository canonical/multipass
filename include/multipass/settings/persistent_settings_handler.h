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

#ifndef MULTIPASS_PERSISTENT_SETTINGS_HANDLER_H
#define MULTIPASS_PERSISTENT_SETTINGS_HANDLER_H

#include "setting_spec.h"
#include "settings_handler.h"

#include <map>
#include <mutex>

namespace multipass
{
class PersistentSettingsHandler : public SettingsHandler
{
public:
    PersistentSettingsHandler(QString filename, SettingSpec::Set settings); // no nulls please
    QString get(const QString& key) const override;
    void set(const QString& key, const QString& val) override;
    std::set<QString> keys() const override;

private:
    const SettingSpec& get_setting(const QString& key) const; // throws on unknown key

private:
    using SettingMap = std::map<QString, SettingSpec::UPtr>;
    static SettingMap convert(SettingSpec::Set);

private:
    QString filename;
    SettingMap settings;
    mutable std::mutex mutex;
};
} // namespace multipass

#endif // MULTIPASS_PERSISTENT_SETTINGS_HANDLER_H
