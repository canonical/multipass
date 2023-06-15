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

#include "purge.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/platform.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Purge::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::PurgeReply& reply) {
        auto size = reply.purged_instances_size();
        for (auto i = 0; i < size; ++i)
        {
            const auto purged_instance = reply.purged_instances(i);
            const auto removed_aliases = aliases.remove_aliases_for_instance(purged_instance);

            for (const auto& [removal_context, removed_alias_name] : removed_aliases)
            {
                try
                {
                    MP_PLATFORM.remove_alias_script(removal_context + "." + removed_alias_name);

                    if (!aliases.exists_alias(removed_alias_name))
                    {
                        MP_PLATFORM.remove_alias_script(removed_alias_name);
                    }
                }
                catch (const std::runtime_error& e)
                {
                    cerr << fmt::format("Warning: '{}' when removing alias script for {}.{}\n", e.what(),
                                        removal_context, removed_alias_name);
                }
            }
        }

        return mp::ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) { return standard_failure_handler_for(name(), cerr, status); };

    mp::PurgeRequest request;
    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::purge, request, on_success, on_failure);
}

std::string cmd::Purge::name() const
{
    return "purge";
}

QString cmd::Purge::short_help() const
{
    return QStringLiteral("Purge all deleted instances permanently");
}

QString cmd::Purge::description() const
{
    return QStringLiteral("Purge all deleted instances permanently, including all their data.");
}

mp::ParseCode cmd::Purge::parse_args(mp::ArgParser* parser)
{
    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() > 0)
    {
        cerr << "This command takes no arguments\n";
        return ParseCode::CommandLineError;
    }

    return status;
}
