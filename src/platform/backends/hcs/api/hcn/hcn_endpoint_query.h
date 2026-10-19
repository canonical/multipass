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

#include <string>

namespace multipass::hyperv::hcn
{

/**
 * Parameters for creating a network endpoint.
 */
struct EndpointQuery
{
    /**
     * The GUID of the VM that endpoint belongs to. Note that it's not the VM's name -- the name
     * needs to be resolved to a GUID via `get_compute_system_guid`.
     */
    std::string vm_guid{};
};

} // namespace multipass::hyperv::hcn

/**
 * Formatter type specialization for EndpointQuery
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcn::EndpointQuery, Char>
    : formatter<basic_string_view<Char>, Char>
{

    template <typename FormatContext>
    auto format(const multipass::hyperv::hcn::EndpointQuery& params, FormatContext& ctx) const
        -> FormatContext::iterator;
};
