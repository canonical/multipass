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

#ifndef MULTIPASS_CLIENT_GUI_H
#define MULTIPASS_CLIENT_GUI_H

#include "gui_cmd.h"

#include <multipass/cert_provider.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/rpc_connection_type.h>

#include <memory>
#include <sstream>
#include <unordered_map>

namespace multipass
{
struct ClientConfig
{
    const std::string server_address;
    const RpcConnectionType conn_type;
    std::unique_ptr<CertProvider> cert_provider;
};

class ClientGui
{
public:
    explicit ClientGui(ClientConfig& context);
    int run(const QStringList& arguments);

private:
    std::shared_ptr<grpc::Channel> rpc_channel;
    std::unique_ptr<multipass::Rpc::Stub> stub;
    std::stringstream null_stream;
    std::unique_ptr<cmd::GuiCmd> gui_cmd;
};
} // namespace multipass
#endif // MULTIPASS_CLIENT_GUI_H
