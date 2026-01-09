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

struct _GUID;
using GUID = _GUID;

/**
 * Formatter for GUID type
 */
template <typename Char>
struct fmt::formatter<GUID, Char> : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const GUID& guid, FormatContext& ctx) const -> FormatContext::iterator;
};
