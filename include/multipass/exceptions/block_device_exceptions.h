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

#ifndef MULTIPASS_BLOCK_DEVICE_EXCEPTIONS_H
#define MULTIPASS_BLOCK_DEVICE_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace multipass
{
class ValidationError : public std::runtime_error
{
public:
    explicit ValidationError(const std::string& msg) : runtime_error{msg}
    {
    }
};

class NotFoundError : public std::runtime_error
{
public:
    explicit NotFoundError(const std::string& msg) : runtime_error{msg}
    {
    }
};

} // namespace multipass

#endif // MULTIPASS_BLOCK_DEVICE_EXCEPTIONS_H