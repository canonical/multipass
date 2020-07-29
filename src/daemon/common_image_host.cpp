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

#include <multipass/format.h>
#include <multipass/logging/log.h>

#include <QtConcurrent/QtConcurrent>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "VMImageHost";
}

mp::CommonVMImageHost::CommonVMImageHost(std::chrono::seconds manifest_time_to_live)
  : manifest_time_to_live{manifest_time_to_live}, last_update{}
{
    update_manifests();
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
        if (manifest_update_future.isRunning())
        {
            mpl::log(mpl::Level::trace, category, "Manifest update already running. Waiting to finish…");
            manifest_update_future.waitForFinished();
        }
        else
        {
            manifest_update_future = QtConcurrent::run([this, now] {
                try
                {
                    mpl::log(mpl::Level::trace, category, "Updating manifests…");
                    need_extra_update = false;

                    clear();
                    fetch_manifests();

                    last_update = now;
                    mpl::log(mpl::Level::trace, category, "Finished manifest updates.");
                }
                catch (const std::exception& e)
                {
                    mpl::log(mpl::Level::error, category, e.what());
                }
            });
        }
    }
}

void mp::CommonVMImageHost::on_manifest_empty(const std::string& details)
{
    mpl::log(mpl::Level::info, category, details);
}

void mp::CommonVMImageHost::on_manifest_update_failure(const std::string& details)
{
    need_extra_update = true;
    mpl::log(mpl::Level::warning, category, fmt::format("Could not update manifest: {}", details));
}
