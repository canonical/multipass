/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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

#ifndef MULTIPASS_MEMORY_SIZE_H
#define MULTIPASS_MEMORY_SIZE_H

#include <string>

namespace multipass
{
class MemorySize
{
public:
    friend bool operator==(const MemorySize& a, const MemorySize& b);
    friend bool operator!=(const MemorySize& a, const MemorySize& b);
    friend bool operator<(const MemorySize& a, const MemorySize& b);
    friend bool operator>(const MemorySize& a, const MemorySize& b);
    friend bool operator<=(const MemorySize& a, const MemorySize& b);
    friend bool operator>=(const MemorySize& a, const MemorySize& b);

    MemorySize();
    explicit MemorySize(const std::string& val);
    long long in_bytes() const noexcept;
    long long in_kilobytes() const noexcept;
    long long in_megabytes() const noexcept;
    long long in_gigabytes() const noexcept;

    std::string human_readable() const;

private:
    long long bytes;
};

bool operator==(const MemorySize& a, const MemorySize& b);
bool operator!=(const MemorySize& a, const MemorySize& b);
bool operator<(const MemorySize& a, const MemorySize& b);
bool operator>(const MemorySize& a, const MemorySize& b);
bool operator<=(const MemorySize& a, const MemorySize& b);
bool operator>=(const MemorySize& a, const MemorySize& b);

} // namespace multipass

#endif // MULTIPASS_MEMORY_SIZE_H
