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

#pragma once

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

    Delete(Rpc::StubInterface& stub, Terminal* term, AliasDict& dict)
        : Command(stub, term), aliases(dict)
    {
    }

    ReturnCodeVariant run(ArgParser* parser) override;

    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    AliasDict aliases;
    DeleteRequest request;
    bool delete_all = false;
    bool ask_for_confirmation = true;

    ParseCode parse_args(ArgParser* parser);
    bool confirm() const;
    void remove_aliases_for(const std::string& instance);
};
} // namespace cmd
} // namespace multipass
