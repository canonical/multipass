/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <variant>

namespace multipass
{
enum class ParseCode
{
    Ok,
    CommandLineError,
    CommandFail,
    HelpRequested
};

enum ReturnCode
{
    Ok = 0,
    CommandLineError = 1,
    CommandFail = 2,
    DaemonFail = 3,
    Retry = 4,
    ShellExecFail = 255,
};

// The purpose of this enum is to have a compile-time tagged int for VM-internal return codes
enum VMReturnCode
{
};
// Only implicitly int-convertible types should be used here. If at least 2 of the same type are
// used, index-based get<> and holds_alternative<> are needed.
using ReturnCodeVariant = std::variant<ReturnCode, VMReturnCode>;

inline bool are_return_codes_equal(ReturnCodeVariant rc1, ReturnCode rc2)
{
    // A VMReturnCode can only be obtained if everything in multipass works properly and we manage
    // to obtain that code. In that case, the mp::ReturnCode can only be ReturnCode::Ok.

    return (std::holds_alternative<VMReturnCode>(rc1) && rc2 == ReturnCode::Ok) ||
           (std::holds_alternative<ReturnCode>(rc1) && std::get<ReturnCode>(rc1) == rc2);
}
} // namespace multipass
