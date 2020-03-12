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

#include "tests/mock_environment_helpers.h"
#include "tests/reset_process_factory.h"
#include "tests/test_with_mocked_bin_path.h"

#include <gmock/gmock.h>

#include <QFile>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
const auto apparmor_output_file = "/tmp/multipass-apparmor-profile.txt";
const auto apparmor_profile_text = "profile test_apparmor_profile() { stuff }";
class TestProcessSpec : public mp::ProcessSpec
{
    QString program() const override
    {
        return "test_prog";
    }
    QStringList arguments() const override
    {
        return {"one", "two"};
    }
    QString apparmor_profile() const override
    {
        return apparmor_profile_text;
    }
};
} // namespace

struct ApparmoredProcessTest : public mpt::TestWithMockedBinPath
{
    mpt::UnsetEnvScope env{"DISABLE_APPARMOR"};
    mpt::ResetProcessFactory scope; // will otherwise pollute other tests
    const mp::ProcessFactory& process_factory{mp::ProcessFactory::instance()};
    void TearDown() override
    {
        QFile::remove("apparmor_output_file");
    }
};

TEST_F(ApparmoredProcessTest, loads_profile_with_apparmor)
{
    auto process = process_factory.create_process(std::make_unique<TestProcessSpec>());

    // apparmor profile should have been installed
    QFile apparmor_input(apparmor_output_file);
    ASSERT_TRUE(apparmor_input.open(QIODevice::ReadOnly | QIODevice::Text));
    auto input = apparmor_input.readAll();

    EXPECT_TRUE(input.contains("args: -W, --abort-on-error, -r,"));
    EXPECT_TRUE(input.contains(apparmor_profile_text));
}

TEST_F(ApparmoredProcessTest, unloads_profile_with_apparmor_on_process_out_of_scope)
{
    auto process = process_factory.create_process(std::make_unique<TestProcessSpec>());
    process.reset();

    // apparmor profile should have been removed
    QFile apparmor_input(apparmor_output_file);
    ASSERT_TRUE(apparmor_input.open(QIODevice::ReadOnly | QIODevice::Text));
    auto input = apparmor_input.readAll();

    EXPECT_TRUE(input.contains("args: -W, -R,"));
    EXPECT_TRUE(input.contains(apparmor_profile_text));
}

// Copies of tests in LinuxProcessTest
TEST_F(ApparmoredProcessTest, execute_missing_command)
{
    auto process = process_factory.create_process("a_missing_command");
    auto process_state = process->execute();

    EXPECT_FALSE(process_state.completed_successfully());
    EXPECT_FALSE(process_state.exit_code);

    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::ProcessError::FailedToStart, process_state.error->state);
}

TEST_F(ApparmoredProcessTest, execute_crashing_command)
{
    auto process = process_factory.create_process("mock_process");
    auto process_state = process->execute();

    EXPECT_FALSE(process_state.completed_successfully());
    EXPECT_FALSE(process_state.exit_code);

    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::ProcessError::Crashed, process_state.error->state);
}

TEST_F(ApparmoredProcessTest, execute_good_command_with_positive_exit_code)
{
    const int exit_code = 7;
    auto process = process_factory.create_process("mock_process", {QString::number(exit_code)});
    auto process_state = process->execute();

    EXPECT_FALSE(process_state.completed_successfully());
    EXPECT_TRUE(process_state.exit_code);
    EXPECT_EQ(exit_code, process_state.exit_code.value());
    EXPECT_EQ("Process returned exit code: 7", process_state.failure_message());

    EXPECT_FALSE(process_state.error);
}

TEST_F(ApparmoredProcessTest, execute_good_command_with_zero_exit_code)
{
    const int exit_code = 0;
    auto process = process_factory.create_process("mock_process", {QString::number(exit_code)});
    auto process_state = process->execute();

    EXPECT_TRUE(process_state.completed_successfully());
    EXPECT_TRUE(process_state.exit_code);
    EXPECT_EQ(exit_code, process_state.exit_code.value());
    EXPECT_EQ(QString(), process_state.failure_message());

    EXPECT_FALSE(process_state.error);
}

TEST_F(ApparmoredProcessTest, process_state_when_runs_and_stops_ok)
{
    const int exit_code = 7;
    auto process = process_factory.create_process("mock_process", {QString::number(exit_code), "stay-alive"});
    process->start();

    EXPECT_TRUE(process->wait_for_started());
    auto process_state = process->process_state();

    EXPECT_FALSE(process_state.exit_code);
    EXPECT_FALSE(process_state.error);

    process->write(QByteArray(1, '\0')); // will make mock_process quit
    EXPECT_TRUE(process->wait_for_finished());

    process_state = process->process_state();
    EXPECT_TRUE(process_state.exit_code);
    EXPECT_EQ(exit_code, process_state.exit_code.value());

    EXPECT_FALSE(process_state.error);
}

TEST_F(ApparmoredProcessTest, process_state_when_runs_but_fails_to_stop)
{
    const int exit_code = 2;
    auto process = process_factory.create_process("mock_process", {QString::number(exit_code), "stay-alive"});
    process->start();

    EXPECT_TRUE(process->wait_for_started());
    auto process_state = process->process_state();

    EXPECT_FALSE(process_state.exit_code);
    EXPECT_FALSE(process_state.error);

    EXPECT_FALSE(process->wait_for_finished(100)); // will hit timeout

    process_state = process->process_state();
    EXPECT_FALSE(process_state.exit_code);

    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::Timedout, process_state.error->state);
}

TEST_F(ApparmoredProcessTest, process_state_when_crashes_on_start)
{
    auto process = process_factory.create_process("mock_process"); // will crash immediately
    process->start();

    // EXPECT_TRUE(process->wait_for_started()); // on start too soon, app hasn't crashed yet!
    EXPECT_TRUE(process->wait_for_finished());
    auto process_state = process->process_state();

    EXPECT_FALSE(process_state.exit_code);
    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::Crashed, process_state.error->state);
}

TEST_F(ApparmoredProcessTest, process_state_when_crashes_while_running)
{
    auto process = process_factory.create_process("mock_process", {QString::number(0), "stay-alive"});
    process->start();

    process->write("crash"); // will make mock_process crash
    process->write(QByteArray(1, '\0'));

    EXPECT_TRUE(process->wait_for_finished());

    auto process_state = process->process_state();
    EXPECT_FALSE(process_state.exit_code);
    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::Crashed, process_state.error->state);
}

TEST_F(ApparmoredProcessTest, process_state_when_failed_to_start)
{
    auto process = process_factory.create_process("a_missing_process");
    process->start();

    EXPECT_FALSE(process->wait_for_started());

    auto process_state = process->process_state();

    EXPECT_FALSE(process_state.exit_code);
    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::FailedToStart, process_state.error->state);
}

TEST_F(ApparmoredProcessTest, process_state_when_runs_and_stops_immediately)
{
    const int exit_code = 7;
    auto process = process_factory.create_process("mock_process", {QString::number(exit_code)});
    process->start();

    EXPECT_TRUE(process->wait_for_started());
    auto process_state = process->process_state();

    EXPECT_FALSE(process_state.exit_code);
    EXPECT_FALSE(process_state.error);

    EXPECT_TRUE(process->wait_for_finished());

    process_state = process->process_state();
    EXPECT_TRUE(process_state.exit_code);
    EXPECT_EQ(exit_code, process_state.exit_code.value());

    EXPECT_FALSE(process_state.error);
}
