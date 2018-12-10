/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#ifndef MULTIPASS_EXITLESS_SSHPROCESS_EXCEPTION_H
#define MULTIPASS_EXITLESS_SSHPROCESS_EXCEPTION_H

#include <fmt/format.h>

#include <stdexcept>
#include <string>

namespace multipass
{
class ExitlessSSHProcessException : public std::runtime_error
{
public:
    ExitlessSSHProcessException(const std::string& command, const std::string& cause)
        : runtime_error(fmt::format("failed to obtain exit status for remote process '{}': {}", command, cause))
    {
    }

};
} // namespace multipass
#endif // MULTIPASS_EXITLESS_SSHPROCESS_EXCEPTION_H
