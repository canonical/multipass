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

#include <multipass/basic_persistent_setting.h>

namespace mp = multipass;

mp::BasicPersistentSetting::BasicPersistentSetting(QString key, QString default_)
    : key{std::move(key)}, default_{std::move(default_)}
{
}

QString multipass::BasicPersistentSetting::get_key() const
{
    return key;
}

QString multipass::BasicPersistentSetting::get_default() const
{
    return default_;
}

QString multipass::BasicPersistentSetting::interpret(const QString& val) const
{
    return val;
}
