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

#include "wrapped_qsettings.h"

#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/file_ops.h>
#include <multipass/logging/log.h>
#include <multipass/settings/persistent_settings_handler.h>

#include <cassert>

namespace mp = multipass;
namespace mpl = mp::logging;

namespace
{
std::unique_ptr<mp::WrappedQSettings> persistent_settings(const QString& filename)
{
    return mp::WrappedQSettingsFactory::instance().make_wrapped_qsettings(filename, QSettings::IniFormat);
}

bool exists_but_unreadable(const QString& filename)
{
    std::fstream in_stream;
    MP_FILEOPS.open(in_stream, qPrintable(filename), std::ios_base::in);
    return in_stream.fail() && errno && errno != ENOENT; /*
        Note: QFile::error() not enough for us: it would not distinguish the actual cause of failure;
        Note: errno is only set on some platforms, but those were experimentally verified to be the only ones that do
            not set a bad QSettings status on permission denied; to make this code portable, we need to account for a
            zero errno on the remaining platforms */
}

void check_status(const mp::WrappedQSettings& qsettings, const QString& attempted_operation)
{
    auto status = qsettings.status();
    if (status || exists_but_unreadable(qsettings.fileName()))
        throw mp::PersistentSettingsException{
            attempted_operation, status == QSettings::FormatError
                                     ? QStringLiteral("format error")
                                     : QStringLiteral("access error (consider running with an administrative role)")};
}

QString checked_get(mp::WrappedQSettings& qsettings, const QString& key, const mp::SettingSpec& spec, std::mutex& mutex)
{
    std::lock_guard<std::mutex> lock{mutex};

    const auto& fallback = spec.get_default();
    auto ret = qsettings.value(key, fallback).toString();

    check_status(qsettings, QStringLiteral("read"));

    try
    {
        spec.interpret(ret); // check validity of what we read from disk
    }
    catch (const mp::InvalidSettingException& e)
    {
        mpl::log(mpl::Level::warning, "settings", fmt::format("{}. Resetting '{}'.", e.what(), key));
        qsettings.remove(key);
        qsettings.sync();
        check_status(qsettings, "reset");
        ret = fallback;
    }

    return ret;
}

void checked_set(mp::WrappedQSettings& qsettings, const QString& key, const QString& val, std::mutex& mutex)
{
    std::lock_guard<std::mutex> lock{mutex};

    qsettings.setValue(key, val);

    qsettings.sync(); // flush to confirm we can write
    check_status(qsettings, QStringLiteral("read/write"));
}
} // namespace

mp::PersistentSettingsHandler::PersistentSettingsHandler(QString filename, SettingSpec::Set settings)
    : filename{std::move(filename)}, settings{convert(std::move(settings))}
{
}

// TODO try installing yaml backend
QString mp::PersistentSettingsHandler::get(const QString& key) const
{
    const auto& setting_spec = get_setting(key); // make sure the key is valid before reading from disk
    auto settings_file = persistent_settings(filename);
    return checked_get(*settings_file, key, setting_spec, mutex);
}

auto mp::PersistentSettingsHandler::get_setting(const QString& key) const -> const SettingSpec&
{
    try
    {
        assert(settings.at(key) && "can't have null setting spec"); // TODO use a `not_null` type (e.g. gsl::not_null)
        return *settings.at(key);
    }
    catch (const std::out_of_range&)
    {
        throw UnrecognizedSettingException{key};
    }
}

void mp::PersistentSettingsHandler::set(const QString& key, const QString& val)
{
    auto interpreted = get_setting(key).interpret(val); // check both key and value validity, convert as appropriate

    auto settings_file = persistent_settings(filename);
    checked_set(*settings_file, key, interpreted, mutex);
}

std::set<QString> mp::PersistentSettingsHandler::keys() const
{
    std::set<QString> ret{};
    std::transform(cbegin(settings), cend(settings), std::inserter(ret, begin(ret)),
                   [](const auto& elem) { return elem.first; }); // I wish get<0> worked here... maybe in C++20

    return ret;
}

auto mp::PersistentSettingsHandler::convert(SettingSpec::Set settings) -> SettingMap
{
    SettingMap ret;
    while (!settings.empty())
    {
        auto it = settings.begin();

        assert(*it && "can't have null setting spec"); // TODO use a `not_null` type (e.g. gsl::not_null)
        auto key = (*it)->get_key();                   // ensure setting not extracted yet
        ret.emplace(std::move(key), std::move(settings.extract(it).value())); /* need to extract to be able to move
                                                 see notes in https://en.cppreference.com/w/cpp/container/set/begin */
    }

    return ret;
}
