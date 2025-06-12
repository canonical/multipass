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

#ifndef MULTIPASS_HYPERV_API_HCS_MODIFY_MEMORY_SETTINGS_H
#define MULTIPASS_HYPERV_API_HCS_MODIFY_MEMORY_SETTINGS_H

#include <fmt/format.h>

#include <cstdint>

namespace multipass::hyperv::hcs
{

struct HcsModifyMemorySettings
{
    std::uint32_t size_in_mb{0};
};

} // namespace multipass::hyperv::hcs

#endif
