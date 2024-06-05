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

#ifndef MULTIPASS_VM_MOUNT_H
#define MULTIPASS_VM_MOUNT_H

#include <multipass/id_mappings.h>

#include <QJsonObject>

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
    explicit VMMount(const QJsonObject& json);
    VMMount(const std::string& sourcePath, id_mappings gidMappings, id_mappings uidMappings, MountType mountType);

    QJsonObject serialize() const;

    const std::string& get_source_path() const noexcept;
    const id_mappings& get_gid_mappings() const noexcept;
    const id_mappings& get_uid_mappings() const noexcept;
    MountType get_mount_type() const noexcept;

    friend bool operator==(const VMMount& a, const VMMount& b) noexcept;
    friend bool operator!=(const VMMount& a, const VMMount& b) noexcept;

private:
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

inline bool operator==(const VMMount& a, const VMMount& b) noexcept
{
    return std::tie(a.source_path, a.gid_mappings, a.uid_mappings, a.mount_type) ==
           std::tie(b.source_path, b.gid_mappings, b.uid_mappings, b.mount_type);
}

inline bool operator!=(const VMMount& a, const VMMount& b) noexcept // TODO drop in C++20
{
    return !(a == b);
}

} // namespace multipass

namespace fmt
{
template <>
struct formatter<multipass::VMMount::MountType>
{
    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx)
    {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(const multipass::VMMount::MountType& mount_type, FormatContext& ctx) const
    {
        return format_to(ctx.out(), "{}", static_cast<int>(mount_type));
    }
};
} // namespace fmt

#endif // MULTIPASS_VM_MOUNT_H
