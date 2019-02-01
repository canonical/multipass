/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include <fmt/format.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "VMImageHost";
}

mp::CommonVMImageHost::CommonVMImageHost(std::chrono::seconds manifest_time_to_live)
  : manifest_time_to_live{manifest_time_to_live}, last_update{}
{
}

void mp::CommonVMImageHost::for_each_entry_do(const Action& action)
{
    update_manifests();

    for_each_entry_do_impl(action);
}

auto mp::CommonVMImageHost::info_for_full_hash(const std::string& full_hash) -> VMImageInfo
{
    update_manifests();

    return info_for_full_hash_impl(full_hash);
}

void mp::CommonVMImageHost::update_manifests()
{
    const auto now = std::chrono::steady_clock::now();
    if ((now - last_update) > manifest_time_to_live || need_extra_update)
    {
        need_extra_update = false;

        clear();
        fetch_manifests();

        last_update = now;
    }
}

void mp::CommonVMImageHost::on_manifest_update_failure(const std::string& details)
{
    need_extra_update = true;
    mpl::log(mpl::Level::warning, category, fmt::format("Could not update manifest: {}", details));
}
