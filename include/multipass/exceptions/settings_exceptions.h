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

#pragma once

#include <multipass/format.h>

#include <QString>

#include <stdexcept>
#include <string>

namespace multipass
{
class SettingsException : public std::runtime_error
{
public:
    explicit SettingsException(const std::string& msg) : runtime_error{msg}
    {
    }
};

class PersistentSettingsException : public SettingsException
{
public:
    PersistentSettingsException(const QString& attempted_operation, const QString& detail)
        : SettingsException{fmt::format("Unable to {} settings: {}", attempted_operation, detail)}
    {
    }
};

class UnrecognizedSettingException : public SettingsException
{
public:
    explicit UnrecognizedSettingException(const QString& key)
        : SettingsException{fmt::format("Unrecognized settings key: '{}'", key)}
    {
    }
};

class InvalidSettingException : public SettingsException
{
public:
    InvalidSettingException(const QString& key, const QString& val, const QString& why)
        : SettingsException{fmt::format("Invalid setting '{}={}': {}", key, val, why)}
    {
    }
};

template <typename T>
class UnsupportedSettingValueType : public SettingsException
{
public:
    explicit UnsupportedSettingValueType(const QString& key)
        : SettingsException{
              fmt::format("Invalid value type for key {}. Type hint: {}", key, typeid(T).name())}
    {
    }
};

} // namespace multipass
