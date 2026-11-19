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

#include "formatted_exception_base.h"

#include <stdexcept>
#include <string>

namespace multipass
{
class MountTargetInUseError : public FormattedExceptionBase<>
{
public:
    MountTargetInUseError(const std::string& target, const std::string& vm_name)
        : FormattedExceptionBase{"\"{}\" is already mounted in '{}'", target, vm_name}
    {
    }
};

class MountNotFoundError : public FormattedExceptionBase<>
{
public:
    MountNotFoundError(const std::string& target, const std::string& vm_name)
        : FormattedExceptionBase{"path \"{}\" is not mounted in '{}'", target, vm_name}
    {
    }
};

// Wraps a failure to activate a newly created mount, keeping the original message
class MountActivationError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};
} // namespace multipass
