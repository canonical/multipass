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
    mpl::log_message(mpl::Level::info, category, details);
}

void mp::CommonVMImageHost::on_manifest_update_failure(const std::string& details)
{
    mpl::log(mpl::Level::warning, category, fmt::format("Could not update manifest: {}", details));
}
