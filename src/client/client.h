/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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
#include <multipass/cli/command.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/rpc_connection_type.h>

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
    std::ostream& cout;
    std::ostream& cerr;
};

class Client
{
public:
    explicit Client(ClientConfig& context);
    void run(const QStringList& arguments);

private:
    template <typename T>
    void add_command();
    const std::unique_ptr<CertProvider> cert_provider;
    std::shared_ptr<grpc::Channel> rpc_channel;
    std::unique_ptr<multipass::Rpc::Stub> stub;

    std::vector<cmd::Command::UPtr> commands;

    std::ostream& cout;
    std::ostream& cerr;
};
} // namespace multipass
#endif // MULTIPASS_CLIENT_H
