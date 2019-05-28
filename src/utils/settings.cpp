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

#include <multipass/constants.h>
#include <multipass/exceptions/invalid_settings_exception.h> // TODO move out
#include <multipass/settings.h>
#include <multipass/utils.h> // TODO move out

#include <QSettings>

namespace mp = multipass;

namespace
{
const auto petenv_name = QStringLiteral("primary");
std::map<QString, QString> make_defaults()
{
    return {{mp::petenv_key, petenv_name}};
}
} // namespace

mp::Settings::Settings(const Singleton<Settings>::PrivatePass& pass)
    : Singleton<Settings>::Singleton{pass}, defaults{make_defaults()}
{
}

// TODO @ricab try yaml...
QString mp::Settings::get(const QString& key) const
{
    const auto& default_ret = get_default(key); // make sure the key is valid before reading from disk
    return QSettings{}.value(key, default_ret).toString();
}

void mp::Settings::set(const QString& key, const QString& val)
{
    get_default(key); // make sure the key is valid before setting
    if (key == petenv_key && !mp::utils::valid_hostname(val.toStdString()))
        throw InvalidSettingsException{key, val, "Invalid hostname"}; // TODO move checking logic out
    QSettings{}.setValue(key, val);
}

const QString& mp::Settings::get_default(const QString& key) const
{
    return defaults.at(key); // throws if not there
}
