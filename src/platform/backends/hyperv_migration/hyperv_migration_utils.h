/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// TODO hyperv migration, remove (whole file)

#pragma once

#include <cstdint>
#include <filesystem>

namespace multipass::hyperv::migration
{
[[nodiscard]] bool same_path(const std::filesystem::path& lhs, const std::filesystem::path& rhs);

[[nodiscard]] bool path_is_within(const std::filesystem::path& path,
                                  const std::filesystem::path& root);

// Throws if the length cannot be read.
[[nodiscard]] std::uintmax_t logical_file_length(const std::filesystem::path& path);
} // namespace multipass::hyperv::migration
