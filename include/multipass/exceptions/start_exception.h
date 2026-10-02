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

#include <stdexcept>
#include <string>

namespace multipass
{
class StartException : public std::runtime_error
{
public:
    StartException(const std::string& instance_name, const std::string& what, bool expected = false)
        : runtime_error(what), instance_name(instance_name), expected{expected}
    {
    }

    std::string name() const
    {
        return instance_name;
    }
    bool was_intentional() const
    {
        return expected;
    }

private:
    const std::string instance_name;
    bool expected;
};
} // namespace multipass
