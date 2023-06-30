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

#ifndef MULTIPASS_INTERNAL_TIMEOUT_EXCEPTION_H
#define MULTIPASS_INTERNAL_TIMEOUT_EXCEPTION_H

#include <chrono>
#include <stdexcept>
#include <string>

#include <multipass/format.h>

namespace multipass
{

class InternalTimeoutException : public std::runtime_error
{
public:
    InternalTimeoutException(const std::string& action, std::chrono::milliseconds timeout)
        : std::runtime_error{fmt::format("Could not {} within {}ms", action, timeout.count())}
    {
    }
};

} // namespace multipass

#endif // MULTIPASS_INTERNAL_TIMEOUT_EXCEPTION_H
