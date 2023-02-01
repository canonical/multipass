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

#ifndef MULTIPASS_CSTRING_H
#define MULTIPASS_CSTRING_H

#include <string>

namespace multipass
{
namespace logging
{
class CString
{
public:
    constexpr CString(const char* data) : data{data}
    {
    }

    // Use this carefully, it depends on the lifetime of s
    CString(const std::string& s) : data{s.c_str()}
    {
    }

    const char* c_str() const
    {
        return data;
    }

private:
    const char* data;
};
} // namespace logging
} // namespace multipass

#endif // MULTIPASS_CSTRING_H
