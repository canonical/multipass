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

#include "get.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/constants.h> // TODO remove (only for keys ATTOW and we shouldn't care for particular settings here)
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/settings/settings.h>

#include <QtGlobal>

#include <cassert>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Get::run(mp::ArgParser* parser)
{
    auto parse_code = parse_args(parser);
    auto ret = parser->returnCodeFrom(parse_code);
    if (parse_code == ParseCode::Ok)
    {
        try
        {
            if (keys_opt)
                print_keys();
            else
                print_settings();
        }
        catch (const SettingsException& e)
        {
            cerr << e.what() << "\n";
            ret = return_code_from(e);
        }
    }

    return ret;
}

std::string cmd::Get::name() const
{
    return "get";
}

QString cmd::Get::short_help() const
{
    return QStringLiteral("Get a configuration setting");
}

QString cmd::Get::description() const
{
    auto desc = QStringLiteral("Get the configuration setting corresponding to the given key, or all settings if "
                               "no key is specified.\n(Support for multiple keys and wildcards coming...)");
    return desc + "\n\n" + describe_common_settings_keys();
}

mp::ParseCode cmd::Get::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("arg", "Setting key, i.e. path to the intended setting.", "[<arg>]");

    QCommandLineOption raw_option("raw", "Output in raw format. For now, this affects only the representation of empty "
                                         "values (i.e. \"\" instead of \"<empty>\").");
    QCommandLineOption keys_option("keys", "List available settings keys. This outputs the whole list of currently "
                                           "available settings keys, or just <arg>, if provided and a valid key.");

    parser->addOption(raw_option);
    parser->addOption(keys_option);

    auto status = parser->commandParse(this);
    if (status == ParseCode::Ok)
    {
        keys_opt = parser->isSet(keys_option);
        raw_opt = parser->isSet(raw_option);

        const auto args = parser->positionalArguments();
        if (args.count() == 1)
        {
            arg = args.at(0);
        }
        else if (args.count() > 1)
        {
            cerr << "Need at most one setting key.\n";
            status = ParseCode::CommandLineError;
        }
        else if (!keys_opt) // support 0 or 1 positional arg when --keys is given
        {
            cerr << "Multiple settings not implemented yet. Please try again with one setting key or just the "
                    "`--keys` option for now.\n";
            status = ParseCode::CommandLineError;
        }
    }

    return status;
}

void cmd::Get::print_settings() const
{
    assert(!arg.isEmpty() && "Need single arg until we implement multiple settings");

    if (const auto val = MP_SETTINGS.get(arg); arg == passphrase_key) // TODO integrate into setting specs
        cout << (val.isEmpty() ? "false" : "true");
    else if (val.isEmpty() && !raw_opt)
        cout << "<empty>";
    else
        cout << qUtf8Printable(val);

    cout << "\n";
}

void multipass::cmd::Get::print_keys() const
{
    const auto keys = MP_SETTINGS.keys();
    const auto format = "{}\n";

    if (arg.isEmpty())
        fmt::print(cout, format, fmt::join(keys, "\n"));
    else if (std::find(keys.cbegin(), keys.cend(), arg) != keys.cend()) // TODO implement key globing
        fmt::print(cout, format, arg); // not very useful, but just a particular case of (intended) glob matching
    else
        throw mp::UnrecognizedSettingException(arg); // wildcards not implemented yet
}
