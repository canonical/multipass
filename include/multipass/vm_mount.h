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

#include <multipass/id_mappings.h>

#include <boost/json.hpp>

#include <string>

namespace multipass
{
class VMMount
{
public:
    enum class MountType : int
    {
        Classic = 0,
        Native = 1
    };

    VMMount() = default;
    VMMount(const std::string& sourcePath,
            id_mappings gidMappings,
            id_mappings uidMappings,
            MountType mountType);

    const std::string& get_source_path() const noexcept;
    const id_mappings& get_gid_mappings() const noexcept;
    const id_mappings& get_uid_mappings() const noexcept;
    MountType get_mount_type() const noexcept;

    friend inline bool operator==(const VMMount& a, const VMMount& b) noexcept = default;

private:
    friend void tag_invoke(const boost::json::value_from_tag&,
                           boost::json::value& json,
                           const VMMount& mount);
    friend VMMount tag_invoke(const boost::json::value_to_tag<VMMount>&,
                              const boost::json::value& json);

    std::string source_path;
    id_mappings gid_mappings;
    id_mappings uid_mappings;
    MountType mount_type;
};

inline const std::string& VMMount::get_source_path() const noexcept
{
    return source_path;
}

inline const multipass::id_mappings& VMMount::get_gid_mappings() const noexcept
{
    return gid_mappings;
}

inline const multipass::id_mappings& VMMount::get_uid_mappings() const noexcept
{
    return uid_mappings;
}

inline VMMount::MountType VMMount::get_mount_type() const noexcept
{
    return mount_type;
}

void tag_invoke(const boost::json::value_from_tag&, boost::json::value& json, const VMMount& mount);
VMMount tag_invoke(const boost::json::value_to_tag<VMMount>&, const boost::json::value& json);

} // namespace multipass

namespace fmt
{
template <>
struct formatter<multipass::VMMount::MountType> : formatter<int>
{
    template <typename FormatContext>
    auto format(const multipass::VMMount::MountType& mount_type, FormatContext& ctx) const
    {
        return formatter<int>::format(static_cast<int>(mount_type), ctx);
    }
};
} // namespace fmt
