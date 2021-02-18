/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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

#include <multipass/exceptions/unsupported_alias_exception.h>
#include <multipass/exceptions/unsupported_remote_exception.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "VMImageHost";
}

mp::CommonVMImageHost::CommonVMImageHost(std::chrono::seconds manifest_time_to_live)
  : manifest_time_to_live{manifest_time_to_live}, last_update{}
{
    // careful: the functor below relies on polymorphic behavior, which is not available in constructors
    // fine here as the call is deferred to after the constructor is done (independently of connection type)
    QObject::connect(&manifest_single_shot, &QTimer::timeout, [this]() {
        try
        {
            update_manifests();
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error, category, e.what());
        }
    });

    manifest_single_shot.setSingleShot(true);
    manifest_single_shot.start(0);
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

void mp::CommonVMImageHost::on_manifest_empty(const std::string& details)
{
    mpl::log(mpl::Level::info, category, details);
}

void mp::CommonVMImageHost::on_manifest_update_failure(const std::string& details)
{
    need_extra_update = true;
    mpl::log(mpl::Level::warning, category, fmt::format("Could not update manifest: {}", details));
}

void mp::CommonVMImageHost::check_remote_is_supported(const std::string& remote_name) const
{
    if (!MP_PLATFORM.is_remote_supported(remote_name))
        throw mp::UnsupportedRemoteException(
            fmt::format("Remote \'{}\' is not a supported remote for this platform. Please use "
                        "`multipass find` for supported remotes and images.",
                        remote_name));
}

void mp::CommonVMImageHost::check_alias_is_supported(const std::string& alias, const std::string& remote_name) const
{
    if (!MP_PLATFORM.is_alias_supported(alias, remote_name))
        throw mp::UnsupportedAliasException(fmt::format(
            "\'{}\' is not a supported alias. Please use `multipass find` for supported image aliases.", alias));
}

bool mp::CommonVMImageHost::check_all_aliases_are_supported(const QStringList& aliases,
                                                            const std::string& remote_name) const
{
    for (const auto& alias : aliases)
    {
        try
        {
            check_alias_is_supported(alias.toStdString(), remote_name);
        }
        catch (const mp::UnsupportedAliasException&)
        {
            return false;
        }
    }

    return true;
}
