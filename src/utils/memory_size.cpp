/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include <multipass/exceptions/invalid_memory_size_exception.h>
#include <multipass/memory_size.h>
#include <multipass/utils.h>

#include <fmt/format.h>

namespace mp = multipass;

mp::MemorySize::MemorySize(const std::string& val) : bytes{0LL} // TODO @ricab
{
}

long long mp::MemorySize::in_bytes() const noexcept
{
    return 0LL; // TODO @ricab
}

long long mp::MemorySize::in_kilobytes() const noexcept
{
    return 0LL; // TODO @ricab
}

long long mp::MemorySize::in_megabytes() const noexcept
{
    return 0LL; // TODO @ricab
}

long long mp::MemorySize::in_gigabytes() const noexcept
{
    return 0LL; // TODO @ricab
}
