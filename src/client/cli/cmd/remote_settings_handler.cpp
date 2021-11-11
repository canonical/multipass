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

#include "remote_settings_handler.h"

#include <multipass/exceptions/settings_exceptions.h>

namespace mp = multipass;

mp::RemoteSettingsHandler::RemoteSettingsHandler(QString key_prefix) : key_prefix{key_prefix}
{
}

QString mp::RemoteSettingsHandler::get(const QString& key) const
{
    if (key.startsWith(key_prefix))
    {
        return QString{"stub"}; // TODO@ricab
    }

    throw mp::UnrecognizedSettingException{key};
}

void mp::RemoteSettingsHandler::set(const QString& key, const QString& val) const
{
    if (key.startsWith(key_prefix))
    {
        ; // TODO@ricab
    }

    throw mp::UnrecognizedSettingException{key};
}

std::set<QString> mp::RemoteSettingsHandler::keys() const
{
    return std::set<QString>(); // TODO@ricab
}
