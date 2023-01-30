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

#ifndef MULTIPASS_INVALID_MEMORY_SIZE_EXCEPTION_H
#define MULTIPASS_INVALID_MEMORY_SIZE_EXCEPTION_H

#include <fmt/format.h>

#include <stdexcept>
#include <string>

namespace multipass
{
class InvalidMemorySizeException : public std::runtime_error
{
public:
    InvalidMemorySizeException(const std::string& val)
        : runtime_error(fmt::format("{} is not a valid memory size - need a non-negative integer (in base 10) "
                                    "or a decimal followed by K, M, or G (e.g. 1234B, 42MiB, 0.5G)",
                                    val))
    {
    }
};
} // namespace multipass
#endif // MULTIPASS_INVALID_MEMORY_SIZE_EXCEPTION_H
