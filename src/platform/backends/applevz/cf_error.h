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

#include <fmt/format.h>

#include <CoreFoundation/CoreFoundation.h>

namespace multipass::applevz
{

namespace
{

inline std::string cfstring_to_std_string(CFStringRef s)
{
    if (!s)
        return {};

    CFIndex len = CFStringGetLength(s);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;

    auto buffer = std::make_unique<char[]>(maxSize);
    if (!buffer)
    {
        return std::string();
    }

    if (CFStringGetCString(s, buffer.get(), maxSize, kCFStringEncodingUTF8))
    {
        std::string result(buffer.get());
        return result;
    }

    return std::string();
}

} // namespace

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

namespace fmt
{
template <>
struct formatter<CFErrorRef>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(CFErrorRef e, FormatContext& ctx)
    {
        if (!e)
            return format_to(ctx.out(), "<null CFErrorRef>");

        CFIndex code = CFErrorGetCode(e);
        CFStringRef domain = CFErrorGetDomain(e);
        CFStringRef desc = CFErrorCopyDescription(e);
        std::string sdomain = multipass::applevz::cfstring_to_std_string(domain);
        std::string sdesc = multipass::applevz::cfstring_to_std_string(desc);

        CFRelease(desc);

        return format_to(ctx.out(),
                         "{} ({}): {}",
                         sdomain.empty() ? "CFError" : sdomain,
                         code,
                         sdesc);
    }
};

} // namespace fmt
