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

#ifndef MULTIPASS_UNALIAS_H
#define MULTIPASS_UNALIAS_H

#include <multipass/cli/alias_dict.h>
#include <multipass/cli/command.h>

#include <unordered_set>

#include <QString>

namespace multipass
{
namespace cmd
{
class Unalias final : public Command
{
public:
    using Command::Command;

    Unalias(Rpc::StubInterface& stub, Terminal* term, AliasDict& dict) : Command(stub, term), aliases(dict)
    {
    }

    ReturnCode run(ArgParser* parser) override;
    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    ParseCode parse_args(ArgParser* parser);

    AliasDict aliases;

    struct str_pair_hash
    {
        inline std::size_t operator()(const std::pair<std::string, std::string> p) const
        {
            return std::hash<std::string>()(p.first) + std::hash<std::string>()(p.second);
        }
    };

    std::vector<std::pair<std::string, std::string>> aliases_to_remove;
};
} // namespace cmd
} // namespace multipass
#endif // MULTIPASS_UNALIAS_H
