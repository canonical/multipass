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

#ifndef MULTIPASS_LAUNCH_H
#define MULTIPASS_LAUNCH_H

#include "animated_spinner.h"

#include <multipass/cli/alias_dict.h>
#include <multipass/cli/command.h>
#include <multipass/timer.h>

#include <QString>

#include <memory>
#include <string>
#include <utility>

namespace multipass
{
namespace cmd
{
class Launch final : public Command
{
public:
    using Command::Command;

    Launch(Rpc::StubInterface& stub, Terminal* term, AliasDict& dict) : Command(stub, term), aliases(dict)
    {
    }

    ReturnCode run(ArgParser* parser) override;
    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    ParseCode parse_args(ArgParser* parser);
    ReturnCode request_launch(const ArgParser* parser);
    ReturnCode mount(const ArgParser* parser, const QString& mount_source, const QString& mount_target);
    bool ask_bridge_permission(multipass::LaunchReply& reply);

    LaunchRequest request;
    QString petenv_name;
    std::unique_ptr<multipass::AnimatedSpinner> spinner;
    std::unique_ptr<multipass::utils::Timer> timer;

    std::vector<std::pair<QString, QString>> mount_routes;
    QString instance_name;

    AliasDict aliases;
};
} // namespace cmd
} // namespace multipass
#endif // MULTIPASS_LAUNCH_H
