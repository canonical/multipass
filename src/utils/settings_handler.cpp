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

#include "wrapped_qsettings.h"

#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/file_ops.h>
#include <multipass/settings_handler.h>

namespace mp = multipass;

namespace
{
std::unique_ptr<mp::WrappedQSettings> persistent_settings(const QString& filename)
{
    auto ret = mp::WrappedQSettingsFactory::instance().make_wrapped_qsettings(filename, QSettings::IniFormat);
    ret->setIniCodec("UTF-8");

    return ret;
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

void check_status(const mp::WrappedQSettings& settings, const QString& attempted_operation)
{
    auto status = settings.status();
    if (status || exists_but_unreadable(settings.fileName()))
        throw mp::PersistentSettingsException{
            attempted_operation, status == QSettings::FormatError
                                     ? QStringLiteral("format error")
                                     : QStringLiteral("access error (consider running with an administrative role)")};
}

QString checked_get(const mp::WrappedQSettings& settings, const QString& key, const QString& fallback,
                    std::mutex& mutex)
{
    std::lock_guard<std::mutex> lock{mutex};

    auto ret = settings.value(key, fallback).toString();

    check_status(settings, QStringLiteral("read"));
    return ret;
}
} // namespace

mp::StandardSettingsHandler::StandardSettingsHandler(QString filename, std::map<QString, QString> defaults)
    : filename{std::move(filename)}, defaults{std::move(defaults)}
{
}

// TODO try installing yaml backend
QString mp::StandardSettingsHandler::get(const QString& key) const
{
    const auto& default_ret = get_default(key); // make sure the key is valid before reading from disk
    auto settings = persistent_settings(key);
    return checked_get(*settings, key, default_ret, mutex);
}

const QString& mp::StandardSettingsHandler::get_default(const QString& key) const
{
    try
    {
        return defaults.at(key);
    }
    catch (const std::out_of_range&)
    {
        throw InvalidSettingsException{key}; // TODO@ricab split into unrecognized-key-exc
    }
}
