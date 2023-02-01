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

#ifndef MULTIPASS_BLUEPRINT_EXCEPTIONS_H
#define MULTIPASS_BLUEPRINT_EXCEPTIONS_H

#include <multipass/format.h>

#include <stdexcept>
#include <string>

namespace multipass
{
class BlueprintMinimumException : public std::runtime_error
{
public:
    BlueprintMinimumException(const std::string& type, const std::string& min_val)
        : runtime_error(fmt::format("Requested {} is less than Blueprint minimum of {}", type, min_val))
    {
    }
};

class InvalidBlueprintException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class IncompatibleBlueprintException : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};
} // namespace multipass
#endif // MULTIPASS_BLUEPRINT_EXCEPTIONS_H
