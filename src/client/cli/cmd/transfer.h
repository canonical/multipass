/*
 * Copyright (C) 2018-2022 Canonical, Ltd.
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

#ifndef MULTIPASS_TRANSFER_H
#define MULTIPASS_TRANSFER_H

#include <multipass/cli/command.h>
#include <multipass/ssh/sftp_client.h>

#include <string>
#include <vector>

namespace multipass::cmd
{

struct InstanceSourcesLocalTarget
{
    std::unordered_multimap<std::string, std::string> sources;
    std::string target_path;
};

struct LocalSourcesInstanceTarget
{
    std::vector<std::string> source_paths;
    std::string target;
};

struct FromCin
{
    std::string target;
};

struct ToCout
{
    std::string source;
};

enum ArgumentsFormat
{
    INSTANCE_SOURCES_LOCAL_TARGET,
    LOCAL_SOURCES_INSTANCE_TARGET,
    FROM_CIN,
    TO_COUT,
};

class Transfer final : public Command
{
public:
    using Command::Command;
    ReturnCode run(ArgParser* parser) override;

    std::string name() const override;
    std::vector<std::string> aliases() const override;
    QString short_help() const override;
    QString description() const override;

private:
    SSHInfoRequest request;
    std::variant<InstanceSourcesLocalTarget, LocalSourcesInstanceTarget, FromCin, ToCout> arguments;
    QFlags<TransferFlags> flags;

    ParseCode parse_args(ArgParser* parser);
};
} // namespace multipass::cmd
#endif // MULTIPASS_TRANSFER_H
