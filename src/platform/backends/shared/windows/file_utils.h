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
#include <system_error>

namespace multipass
{
/**
 * Clear the sparse attribute (FILE_ATTRIBUTE_SPARSE_FILE) of a file, if set.
 * Files that aren't sparse are left untouched.
 *
 * @param [in] path Path to the file
 * @return An empty error code on success, the Win32 error otherwise
 */
[[nodiscard]] std::error_code clear_sparse_flag(const std::filesystem::path& path);
} // namespace multipass
