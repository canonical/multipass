
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

#include <string>

#include <multipass/format.h>

namespace multipass
{
class RustException : public std::exception
{
    // This alternative implementation is so that both rust::Error and RustExceptions can be
    // captured by the same catch statement.
public:
    RustException(const std::string& error)
        : exception{fmt::format("FFI boundary error: {}", error)}
    {
    }
    const char* what() const noexcept override
    {
        return exception.c_str();
    }
    const std::string& what_std() const noexcept
    {
        return exception;
    }

private:
    const std::string exception;
};

class FaultyFFIArgument : public RustException
{
public:
    FaultyFFIArgument(const std::string& arg_name)
        : RustException(fmt::format("Faulty FFI boundary argument: {}", arg_name))
    {
    }
};
class UnknownRustError : public RustException
{
public:
    UnknownRustError(const std::string& error)
        : RustException(fmt::format("Unknown Rust error: {}", error))
    {
    }
};
} // namespace multipass
