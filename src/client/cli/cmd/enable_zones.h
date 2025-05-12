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

#ifndef MULTIPASS_ENABLE_ZONES_H
#define MULTIPASS_ENABLE_ZONES_H

#include <multipass/cli/command.h>

namespace multipass::cmd
{
class EnableZones : public Command
{
public:
    using Command::Command;
    ReturnCode run(ArgParser* parser) override;
    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    ZonesStateRequest request{};
    ParseCode parse_args(ArgParser* parser);
};
} // namespace multipass::cmd
#endif // MULTIPASS_ENABLE_ZONES_H
