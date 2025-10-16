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

#include <CoreFoundation/CoreFoundation.h>

namespace multipass::applevz
{
struct CFError
{
    CFErrorRef ref = nullptr;

    explicit CFError(CFErrorRef r = nullptr) : ref(r)
    {
    }

    CFError(const CFError&) = delete;

    CFError(CFError&& other) noexcept : ref(other.ref)
    {
        other.ref = nullptr;
    }

    CFError& operator=(const CFError&) = delete;

    CFError& operator=(CFError&& other) noexcept
    {
        if (this != &other)
        {
            CFRelease(ref);

            ref = other.ref;
            other.ref = nullptr;
        }
        return *this;
    }

    ~CFError() noexcept
    {
        CFRelease(ref);
    }

    // Allow implicit conversion to CFErrorRef for easy passing
    operator CFErrorRef() const
    {
        return ref;
    }
};
} // namespace multipass::applevz
