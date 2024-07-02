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

#ifndef MULTIPASS_VIRTUAL_MACHINE_STATE_EXCEPTIONS_H
#define MULTIPASS_VIRTUAL_MACHINE_STATE_EXCEPTIONS_H

#include <stdexcept>
#include <string>

namespace multipass
{
class VMStateIdempotentException : public std::runtime_error
{
public:
    explicit VMStateIdempotentException(const std::string& msg) : runtime_error{msg}
    {
    }
};

class VMStateInvalidException : public std::runtime_error
{
public:
    explicit VMStateInvalidException(const std::string& msg) : runtime_error{msg}
    {
    }
};

} // namespace multipass

#endif // MULTIPASS_VIRTUAL_MACHINE_STATE_EXCEPTIONS_H
