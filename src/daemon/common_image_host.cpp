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

#include "common_image_host.h"

#include "multipass/constants.h" // TODO@no-merge drop

#include <multipass/logging/log.h>

#include <multipass/format.h>
#include <multipass/platform.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "VMImageHost";
}

void mp::CommonVMImageHost::for_each_entry_do(const Action& action)
{
    for_each_entry_do_impl(action);
}

auto mp::CommonVMImageHost::info_for_full_hash(const std::string& full_hash) -> VMImageInfo
{
    return info_for_full_hash_impl(full_hash);
}

void mp::CommonVMImageHost::update_manifests(const bool is_force_update_from_network)
{
    clear();
    fetch_manifests(is_force_update_from_network);
}

void mp::CommonVMImageHost::on_manifest_empty(const std::string& details)
{
    mpl::log(mpl::Level::info, category, details);
}

void mp::CommonVMImageHost::on_manifest_update_failure(const std::string& details)
{
    mpl::log(mpl::Level::warning, category, fmt::format("Could not update manifest: {}", details));
}

bool mp::CommonVMImageHost::alias_verifies_image_is_supported(const QStringList& aliases,
                                                              const std::string& remote_name) const
{
    // TODO@no-merge find a better place for this stuff
    static const std::unordered_set<std::string> supported_snapcraft_aliases{
        "core18",
        "18.04",
        "core20",
        "20.04",
        "core22",
        "22.04",
        "core24",
        "24.04",
        "devel",
    };

    return aliases.empty() || remote_name != snapcraft_remote ||
           std::any_of(aliases.cbegin(), aliases.cend(), [](const auto& alias) {
               return supported_snapcraft_aliases.find(alias.toStdString()) != supported_snapcraft_aliases.end();
           });
}
