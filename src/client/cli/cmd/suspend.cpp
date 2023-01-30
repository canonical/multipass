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

#include "suspend.h"
#include "common_cli.h"

#include "animated_spinner.h"
#include "common_callbacks.h"

#include <multipass/cli/argparser.h>
#include <multipass/constants.h>
#include <multipass/settings/settings.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Suspend::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [](mp::SuspendReply& reply) { return ReturnCode::Ok; };

    AnimatedSpinner spinner{cout};
    auto on_failure = [this, &spinner](grpc::Status& status) {
        spinner.stop();
        return standard_failure_handler_for(name(), cerr, status);
    };

    spinner.start(instance_action_message_for(request.instance_names(), "Suspending "));
    request.set_verbosity_level(parser->verbosityLevel());
    return dispatch(&RpcMethod::suspend, request, on_success, on_failure,
                    make_logging_spinner_callback<SuspendRequest, SuspendReply>(spinner, cerr));
}

std::string cmd::Suspend::name() const
{
    return "suspend";
}

QString cmd::Suspend::short_help() const
{
    return QStringLiteral("Suspend running instances");
}

QString cmd::Suspend::description() const
{
    return QStringLiteral("Suspend the named instances, if running. Exits with\n"
                          "return code 0 if successful.");
}

mp::ParseCode cmd::Suspend::parse_args(mp::ArgParser* parser)
{
    const auto petenv_name = MP_SETTINGS.get(petenv_key);

    const auto& [description, syntax] =
        petenv_name.isEmpty()
            ? std::make_pair(QString{"Names of instances to suspend."}, QString{"<name> [<name> ...]"})
            : std::make_pair(
                  QString{
                      "Names of instances to suspend. If omitted, and without the --all option, '%1' will be assumed."}
                      .arg(petenv_name),
                  QString{"[<name> ...]"});

    parser->addPositionalArgument("name", description, syntax);

    QCommandLineOption all_option("all", "Suspend all instances");
    parser->addOptions({all_option});

    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    auto parse_code = check_for_name_and_all_option_conflict(parser, cerr, /*allow_empty=*/!petenv_name.isEmpty());
    if (parse_code != ParseCode::Ok)
    {
        if (petenv_name.isEmpty() && parser->positionalArguments().isEmpty())
            fmt::print(cerr, "Note: the primary instance is disabled.\n");

        return parse_code;
    }

    request.mutable_instance_names()->CopyFrom(add_instance_names(parser, /*default_name=*/petenv_name.toStdString()));

    return status;
}
