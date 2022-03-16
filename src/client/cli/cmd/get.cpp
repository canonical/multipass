/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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
            if (keys)
                print_keys();
            else if (!key.isEmpty())
            {
                if (const auto val = MP_SETTINGS.get(key);
                    key == mp::passphrase_key) // TODO integrate into setting specs
                    cout << (val.isEmpty() ? "false" : "true");
                else if (val.isEmpty() && !raw)
                    cout << "<empty>";
                else
                    cout << qUtf8Printable(val);

                cout << "\n";
            }
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
    auto desc = QStringLiteral("Get the configuration setting corresponding to the given key(s), or all settings if no "
                               "key is specified.");
    return desc + "\n\n" + describe_settings_keys();
}

mp::ParseCode cmd::Get::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("key", "Path(s) to the setting(s) whose configured value(s) should be obtained.",
                                  "[<key> ...]");

    QCommandLineOption raw_option("raw", "Output in raw format. For now, this affects only the representation of empty "
                                         "values (i.e. \"\" instead of \"<empty>\").");
    QCommandLineOption keys_option("keys", "List available settings keys.");

    parser->addOption(raw_option);
    parser->addOption(keys_option);

    auto status = parser->commandParse(this);
    if (status == ParseCode::Ok)
    {
        keys = parser->isSet(keys_option);
        raw = parser->isSet(raw_option);

        const auto args = parser->positionalArguments();
        if (args.count() == 1)
        {
            key = args.at(0);
        }
        else if (!keys)
        {
            cerr << "Multiple settings values not implemented yet. Please try again with one single settings key for "
                    "now.\n";
            status = ParseCode::CommandLineError;
        }
    }

    return status;
}

void multipass::cmd::Get::print_keys() const
{
}
