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
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/platform.h>
#include <multipass/settings.h>
#include <multipass/utils.h> // TODO move out

#include <QSettings>
#include <QStandardPaths>

#include <cassert>
#include <memory>
#include <stdexcept>

namespace mp = multipass;

namespace
{
const auto file_extension = QStringLiteral("conf");
const auto daemon_root = QStringLiteral("local");
const auto petenv_name = QStringLiteral("primary");
std::map<QString, QString> make_defaults()
{ // clang-format off
    return {{mp::petenv_key, petenv_name},
            {mp::driver_key, mp::platform::default_driver()}};
} // clang-format on

QString file_for(const QString& key) // the key should have passed checks at this point
{
    // we make up our own file names to avoid unknown org/domain in path
    // Example: ${HOME}/.config/multipass/multipassd.conf
    static const auto file_path_base =
        QStringLiteral("%1/%2/%4.%3")                                              // note the order
            .arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)) // top config dir
            .arg(mp::client_name)                                                  // subdir
            .arg(file_extension);                                                  // %4 not replaced yet at this point
    static const auto client_file_path = file_path_base.arg(mp::client_name);
    static const auto daemon_file_path = file_path_base.arg(mp::daemon_name); // static consts ensure these stay fixed

    assert(key.startsWith(daemon_root) || key.startsWith("client"));
    return key.startsWith(daemon_root) ? daemon_file_path : client_file_path;
}

std::unique_ptr<QSettings> persistent_settings(const QString& key)
{
    static const auto format = QSettings::defaultFormat(); // static const to make sure these stay fixed

    return std::make_unique<QSettings>(file_for(key), format); /* unique_ptr to circumvent absent copy-ctor and no RVO
                                                                  guarantee (until C++17) */
}

void check_status(const QSettings& settings, const QString& attempted_operation)
{
    auto status = settings.status();
    if (status)
        throw mp::PersistentSettingsException{attempted_operation, status == QSettings::AccessError
                                                                       ? QStringLiteral("access")
                                                                       : QStringLiteral("format")};
}

QString checked_get(QSettings& settings, const QString& key, const QString& fallback)
{
    auto ret = settings.value(key, fallback).toString();

    check_status(settings, QStringLiteral("read"));
    return ret;
}

void checked_set(QSettings& settings, const QString& key, const QString& val)
{
    settings.setValue(key, val);

    settings.sync(); // flush to confirm we can write
    check_status(settings, QStringLiteral("write"));
}

} // namespace

mp::Settings::Settings(const Singleton<Settings>::PrivatePass& pass)
    : Singleton<Settings>::Singleton{pass}, defaults{make_defaults()}
{
}

// TODO try installing yaml backend
QString mp::Settings::get(const QString& key) const
{
    const auto& default_ret = get_default(key); // make sure the key is valid before reading from disk
    return checked_get(*persistent_settings(key), key, default_ret);
}

void mp::Settings::set(const QString& key, const QString& val)
{
    get_default(key); // make sure the key is valid before setting
    if (key == petenv_key && !mp::utils::valid_hostname(val.toStdString()))
        throw InvalidSettingsException{key, val, "Invalid hostname"}; // TODO move checking logic out
    checked_set(*persistent_settings(key), key, val);
}

const QString& mp::Settings::get_default(const QString& key) const
{
    try
    {
        return defaults.at(key);
    }
    catch (const std::out_of_range&)
    {
        throw InvalidSettingsException{key};
    }
}
