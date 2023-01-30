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

#ifndef MULTIPASS_COMMAND_H
#define MULTIPASS_COMMAND_H

#include <multipass/callable_traits.h>
#include <multipass/cli/return_codes.h>
#include <multipass/disabled_copy_move.h>
#include <multipass/format.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/terminal.h>
#include <multipass/utils.h>

#include <QLocalSocket>
#include <QString>

#include <grpc++/grpc++.h>

namespace multipass
{
class ArgParser;

namespace cmd
{
class Command : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<Command>;
    Command(Rpc::StubInterface& stub, std::ostream& cout, std::ostream& cerr) : stub{&stub}, cout{cout}, cerr{cerr}
    {
    }

    Command(Rpc::StubInterface& stub, Terminal* term) : stub{&stub}, term{term}, cout{term->cout()}, cerr{term->cerr()}
    {
    }
    virtual ~Command() = default;

    virtual ReturnCode run(ArgParser* parser) = 0;

    virtual std::string name() const = 0;
    virtual std::vector<std::string> aliases() const
    {
        return {name()};
    };
    virtual QString short_help() const = 0;
    virtual QString description() const = 0;

protected:
    template <typename RpcFunc, typename Request, typename SuccessCallable, typename FailureCallable,
              typename StreamingCallback>
    ReturnCode dispatch(RpcFunc&& rpc_func, const Request& request, SuccessCallable&& on_success,
                        FailureCallable&& on_failure, StreamingCallback&& streaming_callback)
    {
        check_return_callables(on_success, on_failure);

        using Arg0Type = typename multipass::callable_traits<SuccessCallable>::template arg<0>::type;
        using ReplyType = typename std::remove_reference<Arg0Type>::type;
        ReplyType reply;
        auto handle_failure = adapt_failure_handler(on_failure, reply);

        auto rpc_method = std::bind(rpc_func, stub, std::placeholders::_1);

        grpc::ClientContext context;
        std::unique_ptr<grpc::ClientReaderWriterInterface<Request, ReplyType>> client = rpc_method(&context);

        client->Write(request);

        while (client->Read(&reply))
        {
            streaming_callback(reply, client.get());
        }

        auto status = client->Finish();

        if (status.ok())
        {
            return on_success(reply);
        }
        else if (status.error_code() != grpc::StatusCode::UNAVAILABLE)
        {
            return handle_failure(status);
        }
        else
        {
            auto socket_address{context.peer()};
            const auto tokens = multipass::utils::split(context.peer(), ":");
            if (tokens[0] == "unix")
            {
                socket_address = tokens[1];
                QLocalSocket multipassd_socket;
                multipassd_socket.connectToServer(QString::fromStdString(socket_address));
                if (!multipassd_socket.waitForConnected() &&
                    multipassd_socket.error() == QLocalSocket::SocketAccessError)
                {
                    grpc::Status denied_status{
                        grpc::StatusCode::PERMISSION_DENIED, "multipass socket access denied",
                        fmt::format("Please check that you have read/write permissions to '{}'", socket_address)};
                    return handle_failure(denied_status);
                }
            }

            grpc::Status access_error_status{
                grpc::StatusCode::NOT_FOUND, "cannot connect to the multipass socket",
                fmt::format("Please ensure multipassd is running and '{}' is accessible", socket_address)};

            return handle_failure(access_error_status);
        }
    }

    template <typename RpcFunc, typename Request, typename SuccessCallable, typename FailureCallable>
    ReturnCode dispatch(RpcFunc&& rpc_func, const Request& request, SuccessCallable&& on_success,
                        FailureCallable&& on_failure)
    {
        using Arg0Type = typename multipass::callable_traits<SuccessCallable>::template arg<0>::type;
        using ReplyType = typename std::remove_reference<Arg0Type>::type;
        return dispatch(rpc_func, request, on_success, on_failure,
                        [this](ReplyType& reply, grpc::ClientReaderWriterInterface<Request, ReplyType>* client) {
                            if (!reply.log_line().empty())
                            {
                                cerr << reply.log_line();
                            }
                        });
    }

    Rpc::StubInterface* stub;
    Terminal* term;
    std::ostream& cout;
    std::ostream& cerr;

private:
    template <typename SuccessCallable, typename FailureCallable>
    void check_return_callables(SuccessCallable&& on_success, FailureCallable&& on_failure)
    {
        using SuccessCallableTraits = multipass::callable_traits<SuccessCallable>;
        using FailureCallableTraits = multipass::callable_traits<FailureCallable>;
        using SuccessCallableArg0Type = std::remove_reference_t<typename SuccessCallableTraits::template arg<0>::type>;
        using FailureCallableArg0Type = std::remove_reference_t<typename FailureCallableTraits::template arg<0>::type>;

        static_assert(std::is_same<typename SuccessCallableTraits::return_type, ReturnCode>::value);
        static_assert(std::is_same<typename FailureCallableTraits::return_type, ReturnCode>::value);

        static_assert(SuccessCallableTraits::num_args == 1);
        static_assert(std::is_base_of_v<google::protobuf::Message, SuccessCallableArg0Type>,
                      "`on_success` should receive a Message");

        if constexpr (FailureCallableTraits::num_args != 1)
        {
            static_assert(FailureCallableTraits::num_args == 2, "`on_failure` needs to take either 1 or 2 parameters");
            using FailureCallableArg1Type =
                std::remove_reference_t<typename FailureCallableTraits::template arg<1>::type>;
            static_assert(std::is_same_v<SuccessCallableArg0Type, FailureCallableArg1Type>,
                          "`on_success` and `on_failure` should handle the same reply types");
        }
        static_assert(std::is_same<FailureCallableArg0Type, grpc::Status>::value);
    }

    template <typename FailureCallable, typename Reply>
    auto adapt_failure_handler(FailureCallable& on_failure, Reply& reply) // lvalue refs ensure args' lifetime continues
    {
        return [&on_failure, &reply](grpc::Status status) {
            (void)reply; // suppress unhelpful warning in clang: https://bugs.llvm.org/show_bug.cgi?id=35450
            if constexpr (multipass::callable_traits<FailureCallable>::num_args == 2)
                return on_failure(status, reply);
            else
                return on_failure(status);
        };
    }
};
} // namespace cmd
} // namespace multipass
#endif // MULTIPASS_COMMAND_H
