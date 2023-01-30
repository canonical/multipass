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

#ifndef MULTIPASS_TRANSFER_H
#define MULTIPASS_TRANSFER_H

#include <multipass/cli/command.h>
#include <multipass/ssh/sftp_client.h>

#include "multipass/cli/return_codes.h"
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace multipass::cmd
{

struct InstanceSourcesLocalTarget
{
    std::unordered_multimap<std::string, fs::path> sources;
    fs::path target_path;
};

struct LocalSourcesInstanceTarget
{
    std::vector<fs::path> source_paths;
    fs::path target;
};

struct FromCin
{
    fs::path target;
};

struct ToCout
{
    fs::path source;
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
    SFTPClient::Flags flags;

    ParseCode parse_args(ArgParser* parser);
    std::vector<std::pair<std::string, fs::path>> args_to_instance_and_path(const QStringList& args);
    std::optional<ParseCode> parse_streaming(const QStringList& full_sources, const QString& full_target,
                                             std::vector<std::pair<std::string, fs::path>> split_sources,
                                             std::pair<std::string, fs::path> split_target);
    ParseCode parse_non_streaming(std::vector<std::pair<std::string, fs::path>>& split_sources,
                                  std::pair<std::string, fs::path>& split_target);
};
} // namespace multipass::cmd
#endif // MULTIPASS_TRANSFER_H
