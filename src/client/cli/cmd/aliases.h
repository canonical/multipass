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

#ifndef MULTIPASS_ALIASES_H
#define MULTIPASS_ALIASES_H

#include <multipass/cli/command.h>

#include <QString>

namespace multipass
{
class AliasDict;
class Formatter;

namespace cmd
{
class Aliases final : public Command
{
public:
    using Command::Command;

    Aliases(Rpc::StubInterface& stub, Terminal* term, AliasDict& dict) : Command(stub, term), aliases(dict)
    {
    }

    ReturnCode run(ArgParser* parser) override;
    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    ParseCode set_formatter(ArgParser* parser);
    ParseCode parse_args(ArgParser* parser);

    AliasDict aliases;
    Formatter* chosen_formatter;
};
} // namespace cmd
} // namespace multipass
#endif // MULTIPASS_ALIASES_H
