/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include <multipass/process.h>
#include <src/platform/backends/shared/linux/process_factory.h>

#include "test_with_mocked_bin_path.h"

#include <gmock/gmock.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

struct LinuxProcessTest : public mpt::TestWithMockedBinPath
{
    const mp::ProcessFactory& process_factory = mp::ProcessFactory::instance();
};

TEST_F(LinuxProcessTest, execute_missing_command)
{
    auto process = process_factory.create_process("a_missing_command");
    auto exit_state = process->execute();

    EXPECT_FALSE(exit_state.success());
    EXPECT_FALSE(exit_state.exit_code);

    EXPECT_TRUE(exit_state.error);
    EXPECT_EQ(QProcess::ProcessError::FailedToStart, exit_state.error->state);
}

TEST_F(LinuxProcessTest, execute_crashing_command)
{
    auto process = process_factory.create_process("mock_process");
    auto exit_state = process->execute();

    EXPECT_FALSE(exit_state.success());
    EXPECT_FALSE(exit_state.exit_code);

    EXPECT_TRUE(exit_state.error);
    EXPECT_EQ(QProcess::ProcessError::Crashed, exit_state.error->state);
}

TEST_F(LinuxProcessTest, execute_good_command_with_positive_exit_code)
{
    const int exit_code = 7;
    auto process = process_factory.create_process("mock_process", {QString::number(exit_code)});
    auto exit_state = process->execute();

    EXPECT_FALSE(exit_state.success());
    EXPECT_TRUE(exit_state.exit_code);
    EXPECT_EQ(exit_code, exit_state.exit_code.value());

    EXPECT_FALSE(exit_state.error);
}

TEST_F(LinuxProcessTest, execute_good_command_with_zero_exit_code)
{
    const int exit_code = 0;
    auto process = process_factory.create_process("mock_process", {QString::number(exit_code)});
    auto exit_state = process->execute();

    EXPECT_TRUE(exit_state.success());
    EXPECT_TRUE(exit_state.exit_code);
    EXPECT_EQ(exit_code, exit_state.exit_code.value());

    EXPECT_FALSE(exit_state.error);
}
