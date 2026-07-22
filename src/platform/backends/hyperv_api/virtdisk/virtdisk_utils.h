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

#include <filesystem>
#include <string_view>

namespace multipass::hyperv::virtdisk
{

void try_rename(std::string_view log_category,
                const std::filesystem::path& from,
                const std::filesystem::path& to) noexcept;

bool is_direct_child_of(const std::filesystem::path& disk,
                        const std::filesystem::path& parent_disk);

} // namespace multipass::hyperv::virtdisk
