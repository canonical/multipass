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

#pragma once

#include <filesystem>
#include <optional>

namespace multipass::hyperv
{
struct HCSOwnership
{
    std::filesystem::path active_disk;
    std::filesystem::path state_file_stem;

    static std::optional<HCSOwnership> load(const std::filesystem::path& instance_dir);

    void persist(const std::filesystem::path& instance_dir) const;
};
} // namespace multipass::hyperv
