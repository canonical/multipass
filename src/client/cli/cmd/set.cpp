/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "set.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/platform.h> // temporary
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/settings.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
bool allow_only_stopped_instances(const QString& key, const QString& val) // temporary
{
    return key == "local.driver" && mp::platform::is_backend_supported(val) && (val == "qemu" || val == "libvirt") &&
           val != mp::Settings::instance().get(key); /* if we are switching between qemu and libvirt drivers (on linux),
                                                        we can only have stopped instances */
}
} // namespace

mp::ReturnCode cmd::Set::run(mp::ArgParser* parser)
{
    auto parse_code = parse_args(parser);
    auto ret = parser->returnCodeFrom(parse_code);
    if (parse_code == ParseCode::Ok)
    {
        try
        {
            if (allow_only_stopped_instances(key, val))
            {
                auto on_success = [this](ListReply& reply) {
                    for (const auto& instance : reply.instances())
                    {
                        if (instance.instance_status().status() != mp::InstanceStatus::STOPPED)
                        {
                            cerr << "All instances need to be stopped.\n";
                            cerr << "Please save any data and stop the instances before proceeding:\n\n";
                            cerr << "multipass stop --all\n";
                            return ReturnCode::CommandFail;
                        }
                    }

                    return ReturnCode::Ok;
                };

                auto on_failure = [this](grpc::Status& status) {
                    return standard_failure_handler_for(name(), cerr, status);
                };

                ListRequest request;
                request.set_verbosity_level(parser->verbosityLevel());
                ret = dispatch(&RpcMethod::list, request, on_success, on_failure);
            }

            if (ret == ReturnCode::Ok)
                Settings::instance().set(key, val);
        }
        catch (const SettingsException& e)
        {
            cerr << e.what() << "\n";
            ret = return_code_from(e);
        }
    }

    return ret;
}

std::string cmd::Set::name() const
{
    return "set";
}

QString cmd::Set::short_help() const
{
    return QStringLiteral("Set a configuration option");
}

QString cmd::Set::description() const
{
    return QStringLiteral("Set, to the given value, the configuration option corresponding to the given key");
}

mp::ParseCode cmd::Set::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument(
        "keyval",
        "A key-value pair. The key specifies a path to the option to configure. The value is its intended value.",
        "<key>=<value>");

    auto status = parser->commandParse(this);
    if (status == ParseCode::Ok)
    {
        const auto args = parser->positionalArguments();
        if (args.size() != 1)
        {
            cerr << "Need exactly one key-value pair (in <key>=<value> form).\n";
            status = ParseCode::CommandLineError;
        }
        else
        {
            const auto keyval = args.at(0).split('=');
            if (keyval.size() != 2 || keyval.contains(QStringLiteral("")))
            {
                cerr << "Bad key-value format.\n";
                status = ParseCode::CommandLineError;
            }
            else
            {
                key = keyval.at(0);
                val = keyval.at(1);
            }
        }
    }

    return status;
}
