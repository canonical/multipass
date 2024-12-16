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

#include "common.h"
#include "test_with_mocked_bin_path.h"

#include <multipass/process/basic_process.h>
#include <multipass/process/simple_process_spec.h>

namespace mp = multipass;
namespace mpt = multipass::test;

using namespace testing;

struct BasicProcessTest : public mpt::TestWithMockedBinPath
{
};

TEST_F(BasicProcessTest, execute_missing_command)
{
    mp::BasicProcess process(mp::simple_process_spec("a_missing_command"));
    auto process_state = process.execute();

    EXPECT_FALSE(process_state.completed_successfully());
    EXPECT_FALSE(process_state.exit_code);

    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::ProcessError::FailedToStart, process_state.error->state);
}

TEST_F(BasicProcessTest, execute_crashing_command)
{
    mp::BasicProcess process(mp::simple_process_spec("mock_process"));
    auto process_state = process.execute();

    EXPECT_FALSE(process_state.completed_successfully());
    EXPECT_FALSE(process_state.exit_code);

    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::ProcessError::Crashed, process_state.error->state);
}

TEST_F(BasicProcessTest, execute_good_command_with_positive_exit_code)
{
    const int exit_code = 7;
    mp::BasicProcess process(mp::simple_process_spec("mock_process", {QString::number(exit_code)}));
    auto process_state = process.execute();

    EXPECT_FALSE(process_state.completed_successfully());
    ASSERT_TRUE(process_state.exit_code);
    EXPECT_EQ(exit_code, *process_state.exit_code);
    EXPECT_EQ("Process returned exit code: 7", process_state.failure_message());

    EXPECT_FALSE(process_state.error);
}

TEST_F(BasicProcessTest, execute_good_command_with_zero_exit_code)
{
    const int exit_code = 0;
    mp::BasicProcess process(mp::simple_process_spec("mock_process", {QString::number(exit_code)}));
    auto process_state = process.execute();

    EXPECT_TRUE(process_state.completed_successfully());
    ASSERT_TRUE(process_state.exit_code);
    EXPECT_EQ(exit_code, *process_state.exit_code);
    EXPECT_EQ(QString(), process_state.failure_message());

    EXPECT_FALSE(process_state.error);
}

TEST_F(BasicProcessTest, process_state_when_runs_and_stops_ok)
{
    const int exit_code = 7;
    mp::BasicProcess process(mp::simple_process_spec("mock_process", {QString::number(exit_code), "stay-alive"}));
    process.start();

    EXPECT_TRUE(process.wait_for_started());
    auto process_state = process.process_state();

    EXPECT_FALSE(process_state.exit_code);
    EXPECT_FALSE(process_state.error);

    process.write(QByteArray(1, '\0')); // will make mock_process quit
    EXPECT_TRUE(process.wait_for_finished());

    process_state = process.process_state();
    ASSERT_TRUE(process_state.exit_code);
    EXPECT_EQ(exit_code, *process_state.exit_code);

    EXPECT_FALSE(process_state.error);
}

TEST_F(BasicProcessTest, process_state_when_runs_but_fails_to_stop)
{
    const int exit_code = 2;
    mp::BasicProcess process(mp::simple_process_spec("mock_process", {QString::number(exit_code), "stay-alive"}));
    process.start();

    EXPECT_TRUE(process.wait_for_started());
    auto process_state = process.process_state();

    EXPECT_FALSE(process_state.exit_code);
    EXPECT_FALSE(process_state.error);

    EXPECT_FALSE(process.wait_for_finished(100)); // will hit timeout

    process_state = process.process_state();
    EXPECT_FALSE(process_state.exit_code);

    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::Timedout, process_state.error->state);
}

TEST_F(BasicProcessTest, process_state_when_crashes_on_start)
{
    mp::BasicProcess process(mp::simple_process_spec("mock_process")); // will crash immediately
    process.start();

    // EXPECT_TRUE(process.wait_for_started()); // on start too soon, app hasn't crashed yet!
    EXPECT_TRUE(process.wait_for_finished());
    auto process_state = process.process_state();

    EXPECT_FALSE(process_state.exit_code);
    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::Crashed, process_state.error->state);
}

TEST_F(BasicProcessTest, process_state_when_crashes_while_running)
{
    mp::BasicProcess process(mp::simple_process_spec("mock_process", {QString::number(0), "stay-alive"}));
    process.start();

    process.write("crash"); // will make mock_process crash
    process.write(QByteArray(1, '\0'));

    EXPECT_TRUE(process.wait_for_finished());

    auto process_state = process.process_state();
    EXPECT_FALSE(process_state.exit_code);
    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::Crashed, process_state.error->state);
}

TEST_F(BasicProcessTest, process_state_when_failed_to_start)
{
    mp::BasicProcess process(mp::simple_process_spec("a_missing_process"));
    process.start();

    EXPECT_FALSE(process.wait_for_started());

    auto process_state = process.process_state();

    EXPECT_FALSE(process_state.exit_code);
    ASSERT_TRUE(process_state.error);
    EXPECT_EQ(QProcess::FailedToStart, process_state.error->state);
}

TEST_F(BasicProcessTest, process_state_when_runs_and_stops_immediately)
{
    const int exit_code = 7;
    mp::BasicProcess process(mp::simple_process_spec("mock_process", {QString::number(exit_code)}));
    process.start();

    EXPECT_TRUE(process.wait_for_started());
    auto process_state = process.process_state();

    EXPECT_FALSE(process_state.exit_code);
    EXPECT_FALSE(process_state.error);

    EXPECT_TRUE(process.wait_for_finished());

    process_state = process.process_state();
    ASSERT_TRUE(process_state.exit_code);
    EXPECT_EQ(exit_code, *process_state.exit_code);

    EXPECT_FALSE(process_state.error);
}

TEST_F(BasicProcessTest, error_string_when_not_run)
{
    const auto program = "foo";
    mp::BasicProcess process{mp::simple_process_spec(program)};
    EXPECT_THAT(process.error_string().toStdString(), HasSubstr(program));
    EXPECT_THAT(process.error_string().toStdString(), HasSubstr("Unknown"));
}

TEST_F(BasicProcessTest, error_string_when_completing_successfully)
{
    const auto program = "mock_process";
    mp::BasicProcess process{mp::simple_process_spec(program, {"0"})};

    EXPECT_TRUE(process.execute().completed_successfully());
    EXPECT_THAT(process.error_string().toStdString(), HasSubstr(program));
    EXPECT_THAT(process.error_string().toStdString(), HasSubstr("Unknown"));
}

TEST_F(BasicProcessTest, error_string_when_crashing)
{
    const auto program = "mock_process";
    mp::BasicProcess process(mp::simple_process_spec(program));

    EXPECT_FALSE(process.execute().completed_successfully());
    EXPECT_THAT(process.error_string().toStdString(), HasSubstr(program));
}

TEST_F(BasicProcessTest, error_string_when_missing_command)
{
    const auto program = "no_bin_named_like_this";
    mp::BasicProcess process{mp::simple_process_spec(program)};

    EXPECT_FALSE(process.execute().completed_successfully());
    EXPECT_THAT(process.error_string().toStdString(), HasSubstr(program));
}

TEST_F(BasicProcessTest, reports_pid_0_until_started)
{
    const auto program = "mock_process";
    mp::BasicProcess process{mp::simple_process_spec(program)};

    ASSERT_EQ(process.process_id(), 0);
}

TEST_F(BasicProcessTest, reports_positive_pid_after_started)
{
    const auto program = "mock_process";
    auto ran = false;
    mp::BasicProcess process{mp::simple_process_spec(program)};
    QObject::connect(&process, &mp::Process::started, [&process, &ran] {
        EXPECT_GT(process.process_id(), 0);
        ran = true;
    });

    process.start();
    EXPECT_TRUE(process.wait_for_finished());
    EXPECT_TRUE(ran);
}

TEST_F(BasicProcessTest, reports_previous_pid_after_finished)
{
    const auto program = "mock_process";
    auto pid = 0ll;
    mp::BasicProcess process{mp::simple_process_spec(program)};
    QObject::connect(&process, &mp::Process::started, [&process, &pid] { pid = process.process_id(); });

    process.start();
    EXPECT_TRUE(process.wait_for_finished());
    ASSERT_GT(pid, 0);
    EXPECT_EQ(process.process_id(), pid);
}

TEST_F(BasicProcessTest, reads_expected_data_from_stdout_and_stderr)
{
    const QByteArray data{"Some data the mock process will return"};
    mp::BasicProcess process(mp::simple_process_spec("mock_process", {"0", "stay-alive"}));

    QObject::connect(&process, &mp::Process::ready_read_standard_output, [&process, &data] {
        auto stdout_data = process.read_all_standard_output();
        EXPECT_EQ(data, stdout_data);
    });

    QObject::connect(&process, &mp::Process::ready_read_standard_error, [&process, &data] {
        auto stderr_data = process.read_all_standard_error();
        EXPECT_EQ(data, stderr_data);
    });

    EXPECT_TRUE(process.working_directory().isEmpty());

    process.start();

    process.write(data);
    process.write(QByteArray(1, '\0'));

    EXPECT_TRUE(process.wait_for_finished());
}
