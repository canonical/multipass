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

#pragma once

#include <multipass/cli/dispatch_rpc.h>
#include <multipass/disabled_copy_move.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/terminal.h>

namespace multipass
{
class ArgParser;

namespace cmd
{
class Command : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<Command>;
    Command(Rpc::StubInterface& stub, std::ostream& cout, std::ostream& cerr)
        : stub{&stub}, cout{cout}, cerr{cerr}
    {
    }

    Command(Rpc::StubInterface& stub, Terminal* term)
        : stub{&stub}, term{term}, cout{term->cout()}, cerr{term->cerr()}
    {
    }
    virtual ~Command() = default;

    virtual ReturnCodeVariant run(ArgParser* parser) = 0;

    virtual std::string name() const = 0;
    virtual std::vector<std::string> aliases() const
    {
        return {name()};
    };
    virtual QString short_help() const = 0;
    virtual QString description() const = 0;

protected:
    template <typename RpcFunc,
              typename Request,
              typename SuccessCallable,
              typename FailureCallable,
              typename StreamingCallback>
    ReturnCodeVariant dispatch(RpcFunc&& rpc_func,
                               const Request& request,
                               SuccessCallable&& on_success,
                               FailureCallable&& on_failure,
                               StreamingCallback&& streaming_callback)
    {
        return dispatch_rpc_stream(stub,
                                   std::forward<RpcFunc>(rpc_func),
                                   request,
                                   std::forward<SuccessCallable>(on_success),
                                   std::forward<FailureCallable>(on_failure),
                                   std::forward<StreamingCallback>(streaming_callback));
    }

    template <typename RpcFunc,
              typename Request,
              typename SuccessCallable,
              typename FailureCallable>
    ReturnCodeVariant dispatch(RpcFunc&& rpc_func,
                               const Request& request,
                               SuccessCallable&& on_success,
                               FailureCallable&& on_failure)
    {
        return dispatch_rpc(stub,
                            std::forward<RpcFunc>(rpc_func),
                            request,
                            std::forward<SuccessCallable>(on_success),
                            std::forward<FailureCallable>(on_failure),
                            cerr);
    }

    Rpc::StubInterface* stub;
    Terminal* term;
    std::ostream& cout;
    std::ostream& cerr;
};
} // namespace cmd
} // namespace multipass
