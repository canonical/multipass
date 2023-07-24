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

#include "set.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/cli/prompters.h>
#include <multipass/constants.h>
#include <multipass/exceptions/cli_exceptions.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/platform.h> // temporary
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/settings/settings.h>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCode cmd::Set::run(mp::ArgParser* parser)
{
    auto parse_code = parse_args(parser);
    auto ret = parser->returnCodeFrom(parse_code);
    if (parse_code == ParseCode::Ok)
    {
        try
        {
            if (ret == ReturnCode::Ok)
                MP_SETTINGS.set(key, val);
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
    return QStringLiteral("Set a configuration setting");
}

QString cmd::Set::description() const
{
    auto desc = QStringLiteral("Set, to the given value, the configuration setting corresponding to the given key.");
    return desc + "\n\n" + describe_common_settings_keys();
}

mp::ParseCode cmd::Set::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("keyval",
                                  "A key, or a key-value pair. The key specifies a path to the setting to configure. "
                                  "The value is its intended value. If only the key is given, "
                                  "the value will be prompted for.",
                                  "<key>[=<value>]");

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
            const auto keyval = args.at(0).split('=', Qt::KeepEmptyParts);
            if ((keyval.size() != 1 && keyval.size() != 2) || keyval[0].isEmpty())
            {
                cerr << "Bad key-value format.\n";
                status = ParseCode::CommandLineError;
            }
            else if (keyval.size() == 2)
            {
                key = keyval.at(0);
                val = keyval.at(1);
            }
            else
            {
                key = keyval.at(0);
                status = checked_prompt(key);
            }
        }
    }

    return status;
}

mp::ParseCode cmd::Set::checked_prompt(const QString& key)
{
    try
    {
        if (key == passphrase_key) // TODO integrate into setting handlers
        {
            mp::NewPassphrasePrompter prompter(term);
            val = QString::fromStdString(prompter.prompt());
        }
        else
        {
            mp::PlainPrompter prompter(term);
            val = QString::fromStdString(prompter.prompt(key.toStdString()));
        }

        return ParseCode::Ok;
    }
    catch (const mp::PromptException& e)
    {
        cerr << e.what() << std::endl;
        return ParseCode::CommandLineError;
    }
}
