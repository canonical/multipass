/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */
#include "client.h"
#include "cmd/copy_files.h"
#include "cmd/delete.h"
#include "cmd/exec.h"
#include "cmd/find.h"
#include "cmd/help.h"
#include "cmd/info.h"
#include "cmd/launch.h"
#include "cmd/list.h"
#include "cmd/mount.h"
#include "cmd/purge.h"
#include "cmd/recover.h"
#include "cmd/restart.h"
#include "cmd/shell.h"
#include "cmd/start.h"
#include "cmd/stop.h"
#include "cmd/suspend.h"
#include "cmd/umount.h"
#include "cmd/version.h"

#include <grpcpp/grpcpp.h>

#include <algorithm>

#include <multipass/cert_provider.h>
#include <multipass/cli/argparser.h>
#include <multipass/logging/log.h>
#include <multipass/logging/standard_logger.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
auto make_channel(const std::string& server_address, mp::RpcConnectionType conn_type, mp::CertProvider& cert_provider)
{
    std::shared_ptr<grpc::ChannelCredentials> creds;
    if (conn_type == mp::RpcConnectionType::ssl)
    {
        auto opts = grpc::SslCredentialsOptions();
        opts.server_certificate_request = GRPC_SSL_REQUEST_SERVER_CERTIFICATE_BUT_DONT_VERIFY;
        opts.pem_cert_chain = cert_provider.PEM_certificate();
        opts.pem_private_key = cert_provider.PEM_signing_key();
        creds = grpc::SslCredentials(opts);
    }
    else if (conn_type == mp::RpcConnectionType::insecure)
    {
        creds = grpc::InsecureChannelCredentials();
    }
    else
    {
        throw std::runtime_error("Unknown connection type");
    }
    return grpc::CreateChannel(server_address, creds);
}
} // namespace

mp::Client::Client(ClientConfig& config)
    : cert_provider{std::move(config.cert_provider)},
      rpc_channel{make_channel(config.server_address, config.conn_type, *cert_provider)},
      stub{mp::Rpc::NewStub(rpc_channel)},
      cout{config.cout},
      cerr{config.cerr}
{
    add_command<cmd::CopyFiles>();
    add_command<cmd::Launch>();
    add_command<cmd::Purge>();
    add_command<cmd::Exec>();
    add_command<cmd::Find>();
    add_command<cmd::Help>();
    add_command<cmd::Info>();
    add_command<cmd::List>();
    add_command<cmd::Mount>();
    add_command<cmd::Recover>();
    add_command<cmd::Shell>();
    add_command<cmd::Start>();
    add_command<cmd::Stop>();
    add_command<cmd::Suspend>();
    add_command<cmd::Restart>();
    add_command<cmd::Delete>();
    add_command<cmd::Umount>();
    add_command<cmd::Version>();

    auto name_sort = [](cmd::Command::UPtr& a, cmd::Command::UPtr& b) { return a->name() < b->name(); };
    std::sort(commands.begin(), commands.end(), name_sort);
}

template <typename T>
void mp::Client::add_command()
{
    auto cmd = std::make_unique<T>(*rpc_channel, *stub, cout, cerr);
    commands.push_back(std::move(cmd));
}

int mp::Client::run(const QStringList& arguments)
{
    QString description("Create, control and connect to Ubuntu instances.\n\n"
                        "This is a command line utility for multipass, a\n"
                        "service that manages Ubuntu instances.");

    ArgParser parser(arguments, commands, cout, cerr);
    parser.setApplicationDescription(description);

    ParseCode parse_status = parser.parse();
    if (parse_status != ParseCode::Ok)
    {
        return parser.returnCodeFrom(parse_status);
    }

    auto logger = std::make_shared<mpl::StandardLogger>(mpl::level_from(parser.verbosityLevel()));
    mpl::set_logger(logger);

    return parser.chosenCommand()->run(&parser);
}
