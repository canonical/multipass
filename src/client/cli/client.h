/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#ifndef MULTIPASS_CLIENT_H
#define MULTIPASS_CLIENT_H

#include <multipass/cert_provider.h>
#include <multipass/cli/alias_dict.h>
#include <multipass/cli/command.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/rpc_connection_type.h>
#include <multipass/terminal.h>

#include <map>
#include <memory>
#include <unordered_map>

namespace multipass
{
struct ClientConfig
{
    const std::string server_address;
    const RpcConnectionType conn_type;
    std::unique_ptr<CertProvider> cert_provider;
    Terminal *term;
};

class Client
{
public:
    explicit Client(ClientConfig& context);
    virtual ~Client() = default;
    int run(const QStringList& arguments);

protected:
    template <typename T, typename... Ts>
    void add_command(Ts&&... params);
    void sort_commands();

private:
    std::shared_ptr<grpc::Channel> rpc_channel;
    std::unique_ptr<multipass::Rpc::Stub> stub;

    std::vector<cmd::Command::UPtr> commands;

    Terminal* term;
    AliasDict aliases;
};
} // namespace multipass

template <typename T, typename... Ts>
void multipass::Client::add_command(Ts&&... params)
{
    auto cmd = std::make_unique<T>(*rpc_channel, *stub, term, std::forward<Ts>(params)...);
    commands.push_back(std::move(cmd));
}

#endif // MULTIPASS_CLIENT_H
