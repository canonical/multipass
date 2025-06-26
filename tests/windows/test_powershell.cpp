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

#include "powershell_test_helper.h"

#include "tests/common.h"
#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"

#include <src/platform/backends/shared/windows/powershell.h>

#include <multipass/format.h>

#include <cstring>
#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

struct PowerShellTest : public Test
{
    void TearDown() override
    {
        ASSERT_TRUE(ps_helper.was_ps_run());
    }

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    mpt::PowerShellTestHelper ps_helper;
};

TEST_F(PowerShellTest, createsPsProcess)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    ps_helper.setup([](auto* process) { EXPECT_CALL(*process, start()).Times(1); });

    mp::PowerShell ps{"test"};
}

TEST_F(PowerShellTest, exitsPsProcess)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::info);
    ps_helper.setup(
        [](auto* process) {
            EXPECT_CALL(*process, write(Eq(mpt::PowerShellTestHelper::psexit)))
                .WillOnce(Return(mpt::PowerShellTestHelper::written));
            EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(true));
        },
        /* auto_exit = */ false);

    mp::PowerShell ps{"test"};
}

TEST_F(PowerShellTest, handlesFailureToWriteOnExit)
{
    auto& logger = *logger_scope.mock_logger;
    logger.screen_logs(mpl::Level::error);
    logger.expect_log(mpl::Level::warning, "Failed to exit");

    ps_helper.setup(
        [](auto* process) {
            EXPECT_CALL(*process, write(Eq(mpt::PowerShellTestHelper::psexit)))
                .WillOnce(Return(-1));
            EXPECT_CALL(*process, kill);
        },
        /* auto_exit = */ false);

    mp::PowerShell ps{"test"};
}

TEST_F(PowerShellTest, handlesFailureToFinishOnExit)
{
    static constexpr auto err = "timeout";
    auto& logger = *logger_scope.mock_logger;
    logger.screen_logs(mpl::Level::error);

    auto msg_matcher = AllOf(HasSubstr("Failed to exit"), HasSubstr(err));
    EXPECT_CALL(logger, log(mpl::Level::warning, _, msg_matcher));

    ps_helper.setup(
        [](auto* process) {
            EXPECT_CALL(*process, write(Eq(mpt::PowerShellTestHelper::psexit)))
                .WillOnce(Return(mpt::PowerShellTestHelper::written));
            EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(false));
            EXPECT_CALL(*process, error_string).WillOnce(Return(err));
            EXPECT_CALL(*process, kill);
        },
        /* auto_exit = */ false);

    mp::PowerShell ps{"test"};
}

TEST_F(PowerShellTest, usesNameInLogs)
{
    auto& logger = *logger_scope.mock_logger;
    static constexpr auto name = "Shevek";

    logger.screen_logs();
    EXPECT_CALL(logger, log(_, StrEq(name), _)).Times(AtLeast(1));
    ps_helper.setup();

    mp::PowerShell ps{name};
}

TEST_F(PowerShellTest, writeSilentOnSuccess)
{
    static constexpr auto data = "Abbenay";
    ps_helper.setup([](auto* process) {
        EXPECT_CALL(*process, write(Eq(data))).WillOnce(Return(std::strlen(data)));
    });

    mp::PowerShell ps{"Bedap"};

    logger_scope.mock_logger->screen_logs();
    ASSERT_TRUE(ps_helper.ps_write(ps, data));
}

TEST_F(PowerShellTest, writeLogsOnFailure)
{
    static constexpr auto data = "Nio Esseia";
    ps_helper.setup(
        [](auto* process) { EXPECT_CALL(*process, write(Eq(data))).WillOnce(Return(-1)); });

    mp::PowerShell ps{"Takver"};

    auto& logger = *logger_scope.mock_logger;
    logger.screen_logs();
    logger.expect_log(mpl::Level::warning, "Failed to send");
    ASSERT_FALSE(ps_helper.ps_write(ps, data));
}

TEST_F(PowerShellTest, writeLogsWritenBytesOnFailure)
{
    static constexpr auto data = "Anarres";
    static constexpr auto part = 3;
    ps_helper.setup(
        [](auto* process) { EXPECT_CALL(*process, write(Eq(data))).WillOnce(Return(part)); });

    mp::PowerShell ps{"Palat"};

    auto& logger = *logger_scope.mock_logger;
    logger.screen_logs();
    logger.expect_log(mpl::Level::warning, fmt::format("{} bytes", part));
    ASSERT_FALSE(ps_helper.ps_write(ps, data));
}

TEST_F(PowerShellTest, runWritesAndLogsCmd)
{
    static constexpr auto cmdlet = "some cmd and args";
    auto& logger = *logger_scope.mock_logger;
    logger.screen_logs(mpl::Level::error);
    logger.expect_log(mpl::Level::debug, cmdlet);

    ps_helper.setup([](auto* process) {
        EXPECT_CALL(*process, write(Eq(QByteArray{cmdlet}.append('\n'))))
            .WillOnce(Return(-1)); // short-circuit the attempt
    });

    mp::PowerShell ps{"Tirin"};
    ASSERT_FALSE(ps.run(QString{cmdlet}.split(' ')));
}

struct TestPSStatusAndOutput : public PowerShellTest, public WithParamInterface<bool>
{
    QByteArray get_status()
    {
        return ps_helper.get_status(GetParam());
    }

    QByteArray end_marker()
    {
        return ps_helper.end_marker(GetParam());
    }

    void expect_writes(mpt::MockProcess* process)
    {
        return ps_helper.expect_writes(process, cmdlet);
    }

    std::pair<QString, QString> run()
    {
        mp::PowerShell ps{"Gvarab"};
        QString output;
        QString output_err;
        EXPECT_EQ(ps.run(QString{cmdlet}.split(' '), &output, &output_err), GetParam());

        return {output, output_err};
    }

    inline static constexpr auto cmdlet = "gimme data";
};

TEST_P(TestPSStatusAndOutput, runReturnsCmdletStatusAndOutput)
{
    static constexpr auto data = "here's data";
    auto& logger = *logger_scope.mock_logger;
    logger.screen_logs(mpl::Level::warning);
    logger.expect_log(mpl::Level::debug, fmt::format("{}", GetParam()));

    ps_helper.setup([this](auto* process) {
        expect_writes(process);
        EXPECT_CALL(*process, read_all_standard_output)
            .WillOnce(Return(QByteArray{data}.append(end_marker())));
    });

    auto [out, err] = run();
    EXPECT_TRUE(err.isEmpty());
    ASSERT_EQ(out, data);
}

TEST_P(TestPSStatusAndOutput, runHandlesTricklingOutput)
{
    static constexpr auto datum1 = "blah";
    static constexpr auto datum2 = "bleh";
    static constexpr auto datum3 = "blih";
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);

    ps_helper.setup([this](auto* process) {
        expect_writes(process);
        EXPECT_CALL(*process, read_all_standard_output)
            .WillOnce(Return(""))
            .WillOnce(Return(datum1))
            .WillOnce(Return(""))
            .WillOnce(Return(datum2))
            .WillOnce(Return(datum3))
            .WillOnce(Return(""))
            .WillOnce(Return(""))
            .WillOnce(Return(end_marker()));
    });

    auto [out, err] = run();
    EXPECT_TRUE(err.isEmpty());
    ASSERT_EQ(out, QString{datum1} + datum2 + datum3);
};

auto halves(const QString& str)
{
    auto total = str.size();
    auto half = total / 2;

    return std::make_pair(str.left(half).toUtf8(), str.right(total - half).toUtf8());
};

TEST_P(TestPSStatusAndOutput, runHandlesSplitEndMarker)
{
    static constexpr auto data = "lots of info";
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);

    ps_helper.setup([this](auto* process) {
        const auto marker_halves = halves(ps_helper.output_end_marker);
        const auto status_halves = halves(get_status());

        expect_writes(process);
        EXPECT_CALL(*process, read_all_standard_output)
            .WillOnce(Return(QByteArray{data}.append('\n')))
            .WillOnce(Return(marker_halves.first))
            .WillOnce(Return(marker_halves.second))
            .WillOnce(Return(status_halves.first))
            .WillOnce(Return(status_halves.second));
    });

    auto [out, err] = run();
    EXPECT_TRUE(err.isEmpty());
    ASSERT_EQ(out, QString{data});
}

INSTANTIATE_TEST_SUITE_P(PowerShellTest, TestPSStatusAndOutput, Values(true, false));

TEST_F(PowerShellTest, execRunsGivenCmd)
{
    static constexpr auto cmdlet = "make me a sandwich";
    const auto args = QString{cmdlet}.split(' ');

    auto& logger = *logger_scope.mock_logger;
    const auto log_matcher = ContainsRegex(args.join(".*").toStdString());
    logger.screen_logs(mpl::Level::warning);
    EXPECT_CALL(logger, log(_, _, log_matcher));

    ps_helper.setup(
        [&args](auto* process) {
            EXPECT_EQ(process->arguments(), args);
            EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(true));
        },
        /* auto_exit = */ false);
    mp::PowerShell::exec(args, "Mitis");
}

TEST_F(PowerShellTest, execSucceedsWhenNoTimeoutAndProcessSuccessful)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    ps_helper.setup(
        [](auto* process) {
            InSequence seq;
            EXPECT_CALL(*process, start);
            EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(true));
            EXPECT_CALL(*process, process_state)
                .WillOnce(Return(mp::ProcessState{0, std::nullopt}));
        },
        /* auto_exit = */ false);

    EXPECT_TRUE(mp::PowerShell::exec({}, "Efor"));
}

TEST_F(PowerShellTest, execFailsWhenTimeout)
{
    auto& logger = *logger_scope.mock_logger;
    logger.screen_logs(mpl::Level::warning);

    static constexpr auto msg = "timeout";
    logger.expect_log(mpl::Level::warning, msg);

    ps_helper.setup(
        [](auto* process) {
            InSequence seq;

            EXPECT_CALL(*process, start);
            EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(false));
            EXPECT_CALL(*process, process_id).WillOnce(Return(123));
            EXPECT_CALL(*process, error_string).WillOnce(Return(msg));
        },
        /* auto_exit = */ false);

    EXPECT_FALSE(mp::PowerShell::exec({}, "Sabul"));
}

TEST_F(PowerShellTest, execFailsWhenCmdReturnsBadExitCode)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);

    ps_helper.setup(
        [](auto* process) {
            InSequence seq;

            EXPECT_CALL(*process, start);
            EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(true));
            EXPECT_CALL(*process, process_state)
                .WillOnce(Return(mp::ProcessState{-1, std::nullopt}));
        },
        /* auto_exit = */ false);

    EXPECT_FALSE(mp::PowerShell::exec({}, "Rulag"));
}

TEST_F(PowerShellTest, execReturnsCmdOutput)
{
    static constexpr auto datum1 = "bloh";
    static constexpr auto datum2 = "bluh";
    const auto cmdlet = QStringList{"sudo", "make", "me", "a", "sandwich"};

    logger_scope.mock_logger->screen_logs(mpl::Level::warning);

    ps_helper.setup(
        [](auto* process) {
            InSequence seq;
            auto emit_ready_read = [process] { emit process->ready_read_standard_output(); };

            EXPECT_CALL(*process, start).WillOnce(Invoke(emit_ready_read));
            EXPECT_CALL(*process, read_all_standard_output)
                .WillOnce(
                    DoAll(Invoke(emit_ready_read), Return(datum2))) // the invoke needs to be 1st
                .WillOnce(Return(datum1)); // which results in the last return happening 1st
            EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(true));
        },
        /* auto_exit = */ false);

    QString output;
    QString output_err;
    mp::PowerShell::exec(cmdlet, "Gimar", &output, &output_err);
    EXPECT_TRUE(output_err.isEmpty());
    EXPECT_EQ(output, QString{datum1} + datum2);
}

TEST_F(PowerShellTest, execReturnsCmdErrorOutput)
{
    static constexpr auto msg = "A horrible chill runs down your spine...";
    const auto cmdlet = QStringList{"sudo", "make", "me", "an", "error"};

    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    logger_scope.mock_logger->expect_log(mpl::Level::warning, "stderr");

    ps_helper.mock_ps_exec(std::nullopt, msg);

    QString output{}, output_err{};
    mp::PowerShell::exec(cmdlet, "Tiamat", &output, &output_err);
    EXPECT_TRUE(output.isEmpty());
    EXPECT_EQ(output_err, msg);
}
