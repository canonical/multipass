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

#include "image_host_remote_count.h"

#include <multipass/vm_image_host.h>
#include <multipass/vm_image_info.h>

#include <set>
#include <string>

namespace mp = multipass;
namespace mpt = multipass::test;

size_t mpt::count_remotes(mp::VMImageHost& host)
{
    std::set<std::string> remotes;
    auto counting_action = [&remotes](const std::string& remote, const VMImageInfo&) { remotes.insert(remote); };
    host.update_manifests(false);
    host.for_each_entry_do(counting_action);

    return remotes.size();
}
