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

#ifndef MULTIPASS_SNAP_ENVIRONMENT_EXCEPTION_H
#define MULTIPASS_SNAP_ENVIRONMENT_EXCEPTION_H

#include <stdexcept>

#include <multipass/format.h>

namespace multipass
{
class SnapEnvironmentException : public std::runtime_error
{
public:
    SnapEnvironmentException(const std::string& env_var)
        : runtime_error(fmt::format("The \'{}\' environment variable is not set.", env_var))
    {
    }

    SnapEnvironmentException(const std::string& env_var, const std::string& expected_value)
        : runtime_error(fmt::format("The \'{}\' environment variable is not set to \'{}\'.", env_var, expected_value))
    {
    }
};
} // namespace multipass
#endif // MULTIPASS_SNAP_ENVIRONMENT_EXCEPTION_H
