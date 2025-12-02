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

namespace multipass::apple
{

namespace
{

inline std::string CFStringToStdString(CFStringRef s)
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
            if (ref)
                CFRelease(ref);

            ref = other.ref;
            other.ref = nullptr;
        }
        return *this;
    }

    ~CFError() noexcept
    {
        if (ref)
            CFRelease(ref);
    }

    // Check if error is present
    explicit operator bool() const noexcept
    {
        return ref != nullptr;
    }
};

} // namespace multipass::apple

namespace fmt
{
template <>
struct formatter<multipass::apple::CFError>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::apple::CFError& e, FormatContext& ctx) const
    {
        if (!e.ref)
            return format_to(ctx.out(), "<null CFError>");

        CFIndex code = CFErrorGetCode(e.ref);
        CFStringRef domain = CFErrorGetDomain(e.ref);
        CFStringRef desc = CFErrorCopyDescription(e.ref);

        std::string domain_str = multipass::apple::CFStringToStdString(domain);
        std::string desc_str = multipass::apple::CFStringToStdString(desc);

        auto result = format_to(ctx.out(),
                                "{} ({}): {}",
                                domain_str.empty() ? "CFError" : domain_str,
                                code,
                                desc_str.empty() ? "<unknown error>" : desc_str);

        if (desc)
            CFRelease(desc);

        return result;
    }
};

} // namespace fmt
