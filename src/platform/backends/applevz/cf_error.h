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
namespace detail
{
inline std::string cfstring_to_std_string(CFStringRef s)
{
    if (!s)
        return {};

    CFIndex len = CFStringGetLength(s);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;

    auto buffer = std::make_unique<char[]>(maxSize);

    if (CFStringGetCString(s, buffer.get(), maxSize, kCFStringEncodingUTF8))
        return {buffer.get()};

    return {};
}
} // namespace detail

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
            std::swap(ref, other.ref);
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
} // namespace multipass::applevz

namespace fmt
{
template <>
struct formatter<multipass::applevz::CFError>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::applevz::CFError& e, FormatContext& ctx) const
    {
        if (!e.ref)
            return format_to(ctx.out(), "<null CFError>");

        CFIndex code = CFErrorGetCode(e.ref);
        CFStringRef domain = CFErrorGetDomain(e.ref);
        CFStringRef desc = CFErrorCopyDescription(e.ref);

        std::string domain_str = multipass::applevz::detail::cfstring_to_std_string(domain);
        std::string desc_str = multipass::applevz::detail::cfstring_to_std_string(desc);

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
