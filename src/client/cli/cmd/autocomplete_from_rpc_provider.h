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

#pragma once

#include "common_cli.h"

#include <multipass/cli/dispatch_rpc.h>
#include <multipass/cli/return_codes.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <functional>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace multipass::cmd
{

template <typename TCall, typename TRequest, typename TReply>
struct AutoCompleteFromRpcRequest
{
    using Reply = TReply;

    TCall call;
    TRequest request;
    std::function<void(std::vector<std::string>&, TReply&)> collect;
};

using AutoCompleteFromListRequest =
    AutoCompleteFromRpcRequest<decltype(&RpcMethod::list), ListRequest, ListReply>;

AutoCompleteFromListRequest make_instances_request();
AutoCompleteFromListRequest make_snapshots_request();

/**
 * This class retrieves autocompletion suggestions from an internal RPC call.
 */
class AutoCompleteFromRpcProvider
{
public:
    explicit AutoCompleteFromRpcProvider(Rpc::StubInterface* stub) : _stub{stub}
    {
    }

    template <typename AutoCompletionRequest>
    void collect(std::vector<std::string>& proposals, const AutoCompletionRequest& request) const
    {
        using Reply = typename AutoCompletionRequest::Reply;

        auto success_cb = [&proposals, &request](Reply& reply) -> ReturnCodeVariant {
            request.collect(proposals, reply);
            return ReturnCode::Ok;
        };

        // Nothing to do on error, simply add no proposal.
        auto failure_cb = [](grpc::Status&) -> ReturnCodeVariant { return ReturnCode::Ok; };

        auto trash_stream = std::ostringstream{};
        dispatch_rpc(_stub, request.call, request.request, success_cb, failure_cb, trash_stream);
    }

    template <typename... Requests>
    std::vector<std::string> provide(const Requests&... request) const
    {
        auto proposals = std::vector<std::string>{};
        (collect(proposals, request), ...);
        return proposals;
    }

private:
    Rpc::StubInterface* _stub = nullptr;
};

} // namespace multipass::cmd
