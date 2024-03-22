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

#ifndef MULTIPASS_FILE_OPEN_FAILED_EXCEPTION_H
#define MULTIPASS_FILE_OPEN_FAILED_EXCEPTION_H

#include <multipass/format.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace multipass
{
class FileOpenFailedException : public std::runtime_error
{
public:
    explicit FileOpenFailedException(const std::string& name)
        : std::runtime_error(fmt::format("failed to open file '{}': {}({})", name, strerror(errno), errno))
    {
    }
};
} // namespace multipass

#endif // MULTIPASS_FILE_OPEN_FAILED_EXCEPTION_H
