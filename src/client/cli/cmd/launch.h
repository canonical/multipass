/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#ifndef MULTIPASS_LAUNCH_H
#define MULTIPASS_LAUNCH_H

#include <multipass/cli/command.h>
#include <multipass/timer.h>

#include <QString>

namespace multipass
{
namespace cmd
{
class Launch final : public Command
{
public:
    using Command::Command;
    ReturnCode run(ArgParser* parser) override;

    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    ParseCode parse_args(ArgParser* parser) override;
    ReturnCode request_launch();

    LaunchRequest request;
    QString petenv_name;
    ArgParser* arg_parser;
    std::unique_ptr<multipass::utils::Timer> timer;
};
} // namespace cmd
} // namespace multipass
#endif // MULTIPASS_LAUNCH_H
