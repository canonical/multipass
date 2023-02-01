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

#ifndef MULTIPASS_CONNECT_H
#define MULTIPASS_CONNECT_H

#include <multipass/cli/command.h>

#include <QString>

namespace multipass
{
namespace cmd
{
class Shell final : public Command
{
public:
    using Command::Command;
    ReturnCode run(ArgParser *parser) override;

    std::string name() const override;
    std::vector<std::string> aliases() const override;
    QString short_help() const override;
    QString description() const override;

private:
    SSHInfoRequest request;
    QString petenv_name;

    ParseCode parse_args(ArgParser* parser);
};
}
}
#endif // MULTIPASS_CONNECT_H
