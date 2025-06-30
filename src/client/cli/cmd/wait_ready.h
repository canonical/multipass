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

#ifndef MULTIPASS_WAIT_READY_H
#define MULTIPASS_WAIT_READY_H

#include <multipass/cli/command.h>
#include "animated_spinner.h"
#include <multipass/timer.h>


#include <chrono>
#include <thread>

namespace multipass
{
class Formatter;

namespace cmd
{
class WaitReady final : public Command
{
public:
    using Command::Command;
    ReturnCode run(ArgParser* parser) override;

    std::string name() const override;
    QString short_help() const override;
    QString description() const override;

private:
    WaitReadyRequest request;

    ParseCode parse_args(ArgParser* parser);
    std::unique_ptr<multipass::AnimatedSpinner> spinner;
    std::unique_ptr<multipass::utils::Timer> timer;
};
} // namespace cmd
} // namespace multipass
#endif // MULTIPASS_WAIT_READY_H
