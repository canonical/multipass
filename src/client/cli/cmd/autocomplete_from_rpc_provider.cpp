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
 */

#include "autocomplete_from_rpc_provider.h"

namespace multipass::cmd
{

AutoCompleteFromListRequest make_instances_request()
{
    return AutoCompleteFromListRequest{
        .call = &RpcMethod::list,
        .request = ListRequest{},
        .collect = [](std::vector<std::string>& proposals, ListReply& reply) {
            for (const auto& instance : reply.instance_list().instances())
            {
                proposals.push_back(instance.name());
            }
        }};
}

AutoCompleteFromListRequest make_snapshots_request()
{
    auto request = ListRequest{};
    request.set_snapshots(true);

    return AutoCompleteFromListRequest{
        .call = &RpcMethod::list,
        .request = std::move(request),
        .collect = [](std::vector<std::string>& proposals, ListReply& reply) {
            for (const auto& snapshot : reply.snapshot_list().snapshots())
            {
                proposals.push_back(snapshot.name() + '.' +
                                    snapshot.fundamentals().snapshot_name());
            }
        }};
}

} // namespace multipass::cmd
