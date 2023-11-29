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

#ifndef MULTIPASS_DELETE_H
#define MULTIPASS_DELETE_H

#include <multipass/cli/alias_dict.h>
#include <multipass/cli/command.h>

namespace multipass
{
namespace cmd
{
class Delete final : public Command
{
public:
    using Command::Command;

    Delete(Rpc::StubInterface& stub, Terminal* term, AliasDict& dict) : Command(stub, term), aliases(dict)
    {
    }

    ReturnCode run(ArgParser* parser) override;

    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    AliasDict aliases;
    DeleteRequest request;
    std::string instance_args;
    std::string snapshot_args;

    ParseCode parse_args(ArgParser* parser);
    ParseCode parse_instances_snapshots(ArgParser* parser);
    std::string generate_snapshot_purge_msg() const;
    bool confirm_snapshot_purge() const;
};
} // namespace cmd
} // namespace multipass
#endif // MULTIPASS_DELETE_H
