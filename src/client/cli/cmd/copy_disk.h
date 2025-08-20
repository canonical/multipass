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

#ifndef MULTIPASS_COPY_DISK_H
#define MULTIPASS_COPY_DISK_H

#include <multipass/cli/command.h>

namespace multipass
{
namespace cmd
{
class CopyDisk final : public Command
{
public:
    using Command::Command;
    ReturnCode run(ArgParser* parser) override;
    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    ParseCode parse_args(ArgParser* parser);
    
    std::string source_disk_name;
    std::string custom_disk_name; // custom name specified with --name parameter
    
    ListBlocksRequest list_request;
    CreateBlockRequest create_request;
};
} // namespace cmd
} // namespace multipass
#endif // MULTIPASS_COPY_DISK_H
