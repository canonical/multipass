/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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
#include <multipass/platform.h>
#include <multipass/settings.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h> // TODO move out

#include <QDir>
#include <QSettings>

#include <algorithm>
#include <array>
#include <cassert>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace mp = multipass;

namespace
{
const auto file_extension = QStringLiteral("conf");
const auto daemon_root = QStringLiteral("local");
const auto client_root = QStringLiteral("client");
const auto petenv_name = QStringLiteral("primary");
const auto autostart_default = QStringLiteral("true");
const auto winterm_default = QStringLiteral("none");

std::map<QString, QString> make_defaults()
{ // clang-format off
    return {{mp::petenv_key, petenv_name},
            {mp::driver_key, mp::platform::default_driver()},
            {mp::autostart_key, autostart_default},
            {mp::winterm_key, winterm_default}};
} // clang-format on

/*
 * We make up our own file names to:
 *   a) avoid unknown org/domain in path;
 *   b) write daemon config to a central location (rather than user-dependent)
 * Examples:
 *   - ${HOME}/.config/multipass/multipass.conf
 *   - /root/.config/multipass/multipassd.conf
 */
QString file_for(const QString& key) // the key should have passed checks at this point
{
    // static consts ensure these stay fixed
    static const auto file_pattern = QStringLiteral("%2.%1").arg(file_extension); // note the order
    static const auto user_config_path =
        QDir{mp::StandardPaths::instance().writableLocation(mp::StandardPaths::GenericConfigLocation)};
    static const auto cli_client_dir_path = QDir{user_config_path.absoluteFilePath(mp::client_name)};
    static const auto daemon_dir_path = QDir{mp::platform::daemon_config_home()}; // temporary, replace w/ AppConfigLoc
    static const auto client_file_path = cli_client_dir_path.absoluteFilePath(file_pattern.arg(mp::client_name));
    static const auto daemon_file_path = daemon_dir_path.absoluteFilePath(file_pattern.arg(mp::daemon_name));

    assert(key.startsWith(daemon_root) || key.startsWith(client_root));
    return key.startsWith(daemon_root) ? daemon_file_path : client_file_path;
}

QSettings persistent_settings(const QString& key)
{
    return {file_for(key), QSettings::IniFormat};
}

bool exists_but_unreadable(const QString& filename)
{
    std::ifstream stream;
    stream.open(filename.toStdString(), std::ios_base::in);
    return stream.fail() && errno && errno != ENOENT; /*
        Note: QFile::error() not enough for us: it would not distinguish the actual cause of failure;
        Note: errno is only set on some platforms, but those were experimentally verified to be the only ones that do
            not set a bad QSettings status on permission denied; to make this code portable, we need to account for a
            zero errno on the remaining platforms */
}

void check_status(const QSettings& settings, const QString& attempted_operation)
{
    auto status = settings.status();
    if (status || exists_but_unreadable(settings.fileName()))
        throw mp::PersistentSettingsException{
            attempted_operation, status == QSettings::FormatError
                                     ? QStringLiteral("format error")
                                     : QStringLiteral("access error (consider running with an administrative role)")};
}

QString checked_get(const QSettings& settings, const QString& key, const QString& fallback, std::mutex& mutex)
{
    std::lock_guard<std::mutex> lock{mutex};

    auto ret = settings.value(key, fallback).toString();

    check_status(settings, QStringLiteral("read"));
    return ret;
}

void checked_set(QSettings& settings, const QString& key, const QString& val, std::mutex& mutex)
{
    std::lock_guard<std::mutex> lock{mutex};

    settings.setValue(key, val);

    settings.sync(); // flush to confirm we can write
    check_status(settings, QStringLiteral("read/write"));
}

QString interpret_bool(QString val)
{ // constrain accepted values to avoid QVariant::toBool interpreting non-empty strings (such as "nope") as true
    static constexpr auto convert_to_true = {"on", "yes", "1"};
    static constexpr auto convert_to_false = {"off", "no", "0"};
    val = val.toLower();

    if (std::find(cbegin(convert_to_true), cend(convert_to_true), val) != cend(convert_to_true))
        return QStringLiteral("true");
    else if (std::find(cbegin(convert_to_false), cend(convert_to_false), val) != cend(convert_to_false))
        return QStringLiteral("false");
    else
        return val;
}

} // namespace

mp::Settings::Settings(const Singleton<Settings>::PrivatePass& pass)
    : Singleton<Settings>::Singleton{pass}, defaults{make_defaults()}
{
}

std::set<QString> multipass::Settings::keys() const
{
    std::set<QString> ret{};
    std::transform(cbegin(defaults), cend(defaults), std::inserter(ret, begin(ret)),
                   [](const auto& elem) { return elem.first; }); // I wish get<0> worked here... maybe in C++20

    return ret;
}

// TODO try installing yaml backend
QString mp::Settings::get(const QString& key) const
{
    const auto& default_ret = get_default(key); // make sure the key is valid before reading from disk
    auto settings = persistent_settings(key);
    return checked_get(settings, key, default_ret, mutex);
}

void mp::Settings::set(const QString& key, const QString& val)
{
    get_default(key); // make sure the key is valid before setting
    set_aux(key, val);
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

QString mp::Settings::get_daemon_settings_file_path() // temporary
{
    return file_for(daemon_root);
}

QString mp::Settings::get_client_settings_file_path() // idem
{
    return file_for(client_root);
}

void multipass::Settings::set_aux(const QString& key, QString val) // work with a copy of val
{
    // TODO we should have handler callbacks instead
    if (key == petenv_key && !mp::utils::valid_hostname(val.toStdString()))
        throw InvalidSettingsException{key, val, "Invalid hostname"};
    else if (key == driver_key && !mp::platform::is_backend_supported(val))
        throw InvalidSettingsException(key, val, "Invalid driver");
    else if (key == autostart_key && (val = interpret_bool(val)) != "true" && val != "false")
        throw InvalidSettingsException(key, val, "Invalid flag, try \"true\" or \"false\"");
    else if (key == winterm_key)
        val = mp::platform::interpret_winterm_integration(val);

    auto settings = persistent_settings(key);
    checked_set(settings, key, val, mutex);
}
