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

#include "common.h"

#include <multipass/ssh/ssh_process.h>

namespace multipass::test
{
struct MockSSHProcess : public SSHProcess
{
    MOCK_METHOD(bool, exit_recognized, (std::chrono::milliseconds timeout), (override));
    MOCK_METHOD(int, exit_code, (std::chrono::milliseconds timeout), (override));
    MOCK_METHOD(std::string, read_std_output, (), (override));
    MOCK_METHOD(std::string, read_std_error, (), (override));
};
} // namespace multipass::test
