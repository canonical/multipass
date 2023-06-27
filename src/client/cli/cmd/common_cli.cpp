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

#include "common_cli.h"

#include "animated_spinner.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/format_utils.h>
#include <multipass/constants.h>
#include <multipass/exceptions/cmd_exceptions.h>
#include <multipass/exceptions/settings_exceptions.h>

#include <QCommandLineOption>
#include <QString>

#include <chrono>
#include <fmt/ostream.h>
#include <sstream>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ParseCode cmd::check_for_name_and_all_option_conflict(const mp::ArgParser* parser, std::ostream& cerr,
                                                          bool allow_empty)
{
    auto num_names = parser->positionalArguments().count();
    if (num_names == 0 && !parser->isSet(all_option_name) && !allow_empty)
    {
        fmt::print(cerr, "Name argument or --all is required\n");
        return ParseCode::CommandLineError;
    }

    if (num_names > 0 && parser->isSet(all_option_name))
    {
        fmt::print(cerr, "Cannot specify name{} when --all option set\n", num_names > 1 ? "s" : "");
        return ParseCode::CommandLineError;
    }

    return ParseCode::Ok;
}

mp::InstanceNames cmd::add_instance_names(const mp::ArgParser* parser)
{
    InstanceNames instance_names;

    for (const auto& arg : parser->positionalArguments())
    {
        auto instance_name = instance_names.add_instance_name();
        instance_name->append(arg.toStdString());
    }

    return instance_names;
}

mp::InstanceNames cmd::add_instance_names(const mp::ArgParser* parser, const std::string& default_name)
{
    auto instance_names = add_instance_names(parser);
    if (!instance_names.instance_name_size() && !parser->isSet(all_option_name))
        instance_names.add_instance_name(default_name);

    return instance_names;
}

std::vector<mp::InstanceSnapshotPair> cmd::add_instance_and_snapshot_names(const mp::ArgParser* parser)
{
    std::vector<mp::InstanceSnapshotPair> instance_snapshot_names;
    instance_snapshot_names.reserve(parser->positionalArguments().count());

    for (const auto& arg : parser->positionalArguments())
    {
        mp::InstanceSnapshotPair inst_snap_name;
        auto index = arg.indexOf('.');
        inst_snap_name.set_instance_name(arg.left(index).toStdString());
        if (index >= 0)
            inst_snap_name.set_snapshot_name(arg.right(arg.length() - index - 1).toStdString());

        instance_snapshot_names.push_back(inst_snap_name);
    }

    return instance_snapshot_names;
}

mp::ParseCode cmd::handle_format_option(const mp::ArgParser* parser, mp::Formatter** chosen_formatter,
                                        std::ostream& cerr)
{
    *chosen_formatter = mp::format::formatter_for(parser->value(format_option_name).toStdString());

    if (*chosen_formatter == nullptr)
    {
        fmt::print(cerr, "Invalid format type given.\n");
        return ParseCode::CommandLineError;
    }

    return ParseCode::Ok;
}

std::string cmd::instance_action_message_for(const mp::InstanceNames& instance_names, const std::string& action_name)
{
    std::string message{action_name};

    if (instance_names.instance_name().empty())
        message.append("all instances");
    else if (instance_names.instance_name().size() > 1)
        message.append("requested instances");
    else
        message.append(instance_names.instance_name().Get(0));

    return message;
}

mp::ReturnCode cmd::run_cmd(const QStringList& args, const mp::ArgParser* parser, std::ostream& cout,
                            std::ostream& cerr)
{
    ArgParser aux_parser{args, parser->getCommands(), cout, cerr};
    aux_parser.setVerbosityLevel(parser->verbosityLevel());

    [[maybe_unused]] auto code = aux_parser.parse();
    assert(code == mp::ParseCode::Ok);

    return aux_parser.chosenCommand()->run(&aux_parser);
}

namespace
{
mp::ReturnCode ok2retry(mp::ReturnCode code)
{
    return code == mp::ReturnCode::Ok ? mp::ReturnCode::Retry : code;
}
} // namespace

mp::ReturnCode cmd::run_cmd_and_retry(const QStringList& args, const mp::ArgParser* parser, std::ostream& cout,
                                      std::ostream& cerr)
{
    return ok2retry(run_cmd(args, parser, cout, cerr));
}

auto cmd::return_code_from(const mp::SettingsException& e) -> mp::ReturnCode
{
    if (dynamic_cast<const InvalidSettingException*>(&e) || dynamic_cast<const UnrecognizedSettingException*>(&e))
        return ReturnCode::CommandLineError;

    return ReturnCode::CommandFail;
}

QString multipass::cmd::describe_common_settings_keys()
{
    return std::accumulate(cbegin(mp::key_examples), cend(mp::key_examples),
                           QStringLiteral("Some common settings keys are:"),
                           [](const auto& a, const auto& b) { return a + "\n  - " + b; }) +
           "\n\nUse `" + mp::client_name +
           " get --keys` to obtain the full list of available settings at any given time.";
}

void multipass::cmd::add_timeout(multipass::ArgParser* parser)
{
    QCommandLineOption timeout_option(
        "timeout",
        QString("Maximum time, in seconds, to wait for the command to complete. "
                "Note that some background operations may continue beyond that. "
                "By default, instance startup and initialization is limited to "
                "%1 minutes each.")
            .arg(std::chrono::duration_cast<std::chrono::minutes>(multipass::default_timeout).count()),
        "timeout");
    parser->addOption(timeout_option);
}

int multipass::cmd::parse_timeout(const multipass::ArgParser* parser)
{
    if (parser->isSet("timeout"))
    {
        bool ok;
        const auto timeout = parser->value("timeout").toInt(&ok);
        if (!ok || timeout <= 0)
            throw mp::ValidationException("--timeout value has to be a positive integer");
        return timeout;
    }
    return -1;
}

std::unique_ptr<multipass::utils::Timer> multipass::cmd::make_timer(int timeout, AnimatedSpinner* spinner,
                                                                    std::ostream& cerr, const std::string& msg)
{
    auto timer = std::make_unique<multipass::utils::Timer>(std::chrono::seconds(timeout), [spinner, &cerr, msg]() {
        if (spinner)
            spinner->stop();
        cerr << msg << std::endl;
        MP_UTILS.exit(mp::timeout_exit_code);
    });

    return timer;
}
