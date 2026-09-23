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

#include "hyperv_migration_utils.h"

#include <multipass/file_ops.h>

#include <fmt/format.h>
#include <fmt/std.h>

#include <algorithm>
#include <stdexcept>
#include <system_error>

namespace fs = std::filesystem;

bool multipass::hyperv::migration::same_path(const fs::path& lhs, const fs::path& rhs)
{
    return MP_FILEOPS.weakly_canonical(lhs) == MP_FILEOPS.weakly_canonical(rhs);
}

bool multipass::hyperv::migration::path_is_within(const fs::path& path, const fs::path& root)
{
    const auto canonical_path = MP_FILEOPS.weakly_canonical(path);
    const auto canonical_root = MP_FILEOPS.weakly_canonical(root);
    return std::ranges::mismatch(canonical_root, canonical_path).in1 == canonical_root.end();
}

std::uintmax_t multipass::hyperv::migration::logical_file_length(const fs::path& path)
{
    std::error_code error;
    const auto size = MP_FILEOPS.file_size(path, error);
    if (error)
        throw std::runtime_error{
            fmt::format("Could not read the length of '{}': {}", path, error.message())};
    return size;
}
