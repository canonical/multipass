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

#ifndef MULTIPASS_INVALID_SETTINGS_EXCEPTION_H
#define MULTIPASS_INVALID_SETTINGS_EXCEPTION_H

#include <multipass/format.h>

#include <QString>

#include <stdexcept>

namespace multipass
{
class InvalidSettingsException : public std::runtime_error
{
public:
    InvalidSettingsException(const QString& key) : runtime_error{fmt::format("Unrecognized settings key: '{}'", key)}
    {
    }

    InvalidSettingsException(const QString& key, const QString& val, const QString& why)
        : runtime_error{fmt::format("Invalid setting '{}={}': {}", key, val, why)}
    {
    }
};
} // namespace multipass

#endif // MULTIPASS_INVALID_SETTINGS_EXCEPTION_H
