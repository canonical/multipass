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

#include <hyperv_api/hcs_ownership.h>

#include <multipass/file_ops.h>
#include <multipass/json_utils.h>

#include <fmt/format.h>

namespace
{
constexpr auto ownership_filename = "hcs-ownership.json";
constexpr auto ownership_version = 1;
constexpr auto hcs_backend = "hcs";

std::filesystem::path ownership_path(const std::filesystem::path& instance_dir)
{
    return instance_dir / ownership_filename;
}

bool path_is_within(const std::filesystem::path& path, const std::filesystem::path& root)
{
    const auto canonical_path = MP_FILEOPS.weakly_canonical(path);
    const auto canonical_root = MP_FILEOPS.weakly_canonical(root);
    auto path_it = canonical_path.begin();
    for (auto root_it = canonical_root.begin(); root_it != canonical_root.end();
         ++root_it, ++path_it)
    {
        if (path_it == canonical_path.end() || *path_it != *root_it)
            return false;
    }
    return true;
}
} // namespace

std::optional<multipass::hyperv::HCSOwnership> multipass::hyperv::HCSOwnership::load(
    const std::filesystem::path& instance_dir)
{
    const auto path = ownership_path(instance_dir);
    const auto contents = MP_FILEOPS.try_read_file(path);
    if (!contents)
        return std::nullopt;

    const auto json = boost::json::parse(*contents);
    const auto& object = json.as_object();
    const auto version = boost::json::value_to<int>(object.at("version"));
    if (version != ownership_version)
        throw std::runtime_error{fmt::format("Unsupported HCS ownership version {}", version)};

    const auto backend = boost::json::value_to<std::string>(object.at("backend"));
    if (backend != hcs_backend)
        throw std::runtime_error{fmt::format("Unknown Hyper-V backend '{}'", backend)};

    HCSOwnership ownership{
        .active_disk = boost::json::value_to<std::string>(object.at("active_disk")),
        .state_file_stem = boost::json::value_to<std::string>(object.at("state_file_stem")),
    };
    if (!path_is_within(ownership.active_disk, instance_dir) ||
        !path_is_within(ownership.state_file_stem, instance_dir))
        throw std::runtime_error{"HCS ownership contains a path outside the instance directory"};

    return ownership;
}

void multipass::hyperv::HCSOwnership::persist(const std::filesystem::path& instance_dir) const
{
    const boost::json::object json{
        {"version", ownership_version},
        {"backend", hcs_backend},
        {"active_disk", active_disk.string()},
        {"state_file_stem", state_file_stem.string()},
    };

    MP_FILEOPS.write_transactionally(ownership_path(instance_dir), multipass::pretty_print(json));
}
