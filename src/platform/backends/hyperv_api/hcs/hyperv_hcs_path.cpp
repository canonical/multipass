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

#include <hyperv_api/hcs/hyperv_hcs_path.h>

using multipass::hyperv::hcs::HcsPath;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcsPath, Char>::format(const HcsPath& path, FormatContext& ctx) const ->
    typename FormatContext::iterator
{
    if constexpr (std::is_same_v<char, Char>)
    {
        return format_to(ctx.out(), "{}", path.get().generic_string());
    }
    else if constexpr (std::is_same_v<wchar_t, Char>)
    {
        return format_to(ctx.out(), L"{}", path.get().generic_wstring());
    }
}

template auto fmt::formatter<HcsPath, char>::format<fmt::format_context>(const HcsPath&, fmt::format_context&) const
    -> fmt::format_context::iterator;

template auto fmt::formatter<HcsPath, wchar_t>::format<fmt::wformat_context>(const HcsPath&,
                                                                             fmt::wformat_context&) const
    -> fmt::wformat_context::iterator;
