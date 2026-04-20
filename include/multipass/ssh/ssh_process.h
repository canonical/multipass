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

#include <chrono>
#include <string>

namespace multipass
{
class SSHProcess
{
public:
    virtual ~SSHProcess() = default;

    virtual bool exit_recognized(
        std::chrono::milliseconds timeout = std::chrono::milliseconds(10)) = 0;
    virtual int exit_code(std::chrono::milliseconds timeout = std::chrono::seconds(5)) = 0;

    virtual std::string read_std_output() = 0;
    virtual std::string read_std_error() = 0;

protected:
    SSHProcess() = default;

    // movable but not copyable
    SSHProcess(const SSHProcess&) = delete;
    SSHProcess& operator=(const SSHProcess&) = delete;
    SSHProcess(SSHProcess&&) = default;
    SSHProcess& operator=(SSHProcess&&) = default;
};
} // namespace multipass
