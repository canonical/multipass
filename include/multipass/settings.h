/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_SETTINGS_H
#define MULTIPASS_SETTINGS_H

#include "exceptions/settings_exceptions.h"
#include "singleton.h"

#include <QString>
#include <QVariant>

#include <map>

namespace multipass
{
class Settings : public Singleton<Settings>
{
public:
    Settings(const Singleton<Settings>::PrivatePass&);

    virtual QString get(const QString& key) const;            // throws on unknown key
    virtual void set(const QString& key, const QString& val); // throws on unknown key or bad settings

    template <typename T>
    T get_as(const QString& key) const;

    static QString get_daemon_settings_file_path(); // temporary

protected:
    const QString& get_default(const QString& key) const; // throws on unknown key

private:
    std::map<QString, QString> defaults;
};
} // namespace multipass

template <typename T>
T multipass::Settings::get_as(const QString& key) const
{
    auto var = QVariant{get(key)};
    if (var.canConvert<T>())
        return var.value<T>();
    throw UnsupportedSettingValueType<T>(key);
}

#endif // MULTIPASS_SETTINGS_H
