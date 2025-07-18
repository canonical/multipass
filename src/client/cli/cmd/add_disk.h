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

#ifndef MULTIPASS_ADD_DISK_H
#define MULTIPASS_ADD_DISK_H

#include <multipass/cli/command.h>

namespace multipass
{
namespace cmd
{
class AddDisk final : public Command
{
public:
    using Command::Command;
    ReturnCode run(ArgParser* parser) override;
    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    ParseCode parse_args(ArgParser* parser);

    // We'll use both create and attach requests depending on the input
    CreateBlockRequest create_request;
    AttachBlockRequest attach_request;

    bool is_size_input = false; // true if input is a size, false if it's a file path
    std::string vm_name;
    std::string disk_input; // either size or file path
};
} // namespace cmd
} // namespace multipass
#endif // MULTIPASS_ADD_DISK_H
