/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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
#include "exec.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/format_utils.h>
#include <multipass/exceptions/settings_exceptions.h>

#include <fmt/ostream.h>
#include <sstream>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
std::string message_box(const std::string& message)
{
    std::string::size_type divider_length = 50;
    {
        std::istringstream m(message);
        std::string s;
        while (getline(m, s, '\n'))
        {
            divider_length = std::max(divider_length, s.length());
        }
    }

    const auto divider = std::string(divider_length, '#');

    return '\n' + divider + '\n' + message + '\n' + divider + '\n';
}
} // namespace

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

std::string cmd::update_notice(const mp::UpdateInfo& update_info)
{
    return ::message_box("A new Multipass version " + update_info.version() +
                         " is available!\n"
                         "Find out more: " +
                         update_info.url());
}

namespace
{
void check(mp::ParseCode code)
{
    assert(code == mp::ParseCode::Ok);
    static_cast<void>(code); // replace with [[maybe_unused]] in param decl in C++17
}
} // namespace

mp::ReturnCode cmd::run_cmd(const QStringList& args, const mp::ArgParser* parser, std::ostream& cout,
                            std::ostream& cerr)
{
    ArgParser aux_parser{args, parser->getCommands(), cout, cerr};
    check(aux_parser.parse());

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

auto cmd::return_code_from(const SettingsException& e) -> ReturnCode
{
    return dynamic_cast<const InvalidSettingsException*>(&e) ? ReturnCode::CommandLineError : ReturnCode::CommandFail;
}
