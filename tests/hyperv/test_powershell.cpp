/*
 * Copyright (C) 2020 Canonical, Ltd.
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

#include <multipass/format.h>
#include <src/platform/backends/hyperv/powershell.h>

#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace multipass::test
{
struct PowerShellTestAccessor
{
    PowerShellTestAccessor(PowerShell& ps) : ps{ps}
    {
    }

    bool write(const QByteArray& data)
    {
        return ps.write(data);
    }

    inline static const QString& output_end_marker = PowerShell::output_end_marker;

    PowerShell& ps;
};
} // namespace multipass::test

namespace
{
constexpr auto psexe = "powershell.exe";
constexpr auto psexit = "Exit\n";

struct PowerShell : public Test
{
    void TearDown() override
    {
        ASSERT_TRUE(forked);
    }

    void setup(const mpt::MockProcessFactory::Callback& callback = {})
    {
        factory_scope->register_callback([this, callback](mpt::MockProcess* process) {
            setup_process(process);
            if (callback)
                callback(process);
        });
    }

    void setup_process(mpt::MockProcess* process)
    {
        ASSERT_EQ(process->program(), psexe);

        // succeed these by default
        ON_CALL(*process, wait_for_finished(_)).WillByDefault(Return(true));
        ON_CALL(*process, write(_)).WillByDefault(Return(1000));
        EXPECT_CALL(*process, write(Eq(psexit))).Times(AnyNumber());

        forked = true;
    }

    bool forked = false;
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    std::unique_ptr<mpt::MockProcessFactory::Scope> factory_scope = mpt::MockProcessFactory::Inject();
};

TEST_F(PowerShell, creates_ps_process)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    setup([](auto* process) { EXPECT_CALL(*process, start()).Times(1); });

    mp::PowerShell ps{"test"};
}

TEST_F(PowerShell, exits_ps_process)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::info);
    setup([](auto* process) {
        EXPECT_CALL(*process, write(Eq(psexit)));
        EXPECT_CALL(*process, wait_for_finished(_));
    });

    mp::PowerShell ps{"test"};
}

TEST_F(PowerShell, handles_failure_to_write_on_exit)
{
    auto logger = logger_scope.mock_logger;
    logger->screen_logs(mpl::Level::error);
    logger->expect_log(mpl::Level::warning, "Failed to exit");

    setup([](auto* process) {
        EXPECT_CALL(*process, write(Eq(psexit))).WillOnce(Return(-1));
        EXPECT_CALL(*process, kill);
    });

    mp::PowerShell ps{"test"};
}

TEST_F(PowerShell, handles_failure_to_finish_on_exit)
{
    static constexpr auto err = "timeout";
    auto logger = logger_scope.mock_logger;
    logger->screen_logs(mpl::Level::error);

    auto msg_matcher = mpt::MockLogger::make_cstring_matcher(AllOf(HasSubstr("Failed to exit"), HasSubstr(err)));
    EXPECT_CALL(*logger, log(mpl::Level::warning, _, msg_matcher));

    setup([](auto* process) {
        EXPECT_CALL(*process, write(Eq(psexit)));
        EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(false));

        EXPECT_CALL(*process, error_string).WillOnce(Return(err));
        EXPECT_CALL(*process, kill);
    });

    mp::PowerShell ps{"test"};
}

TEST_F(PowerShell, uses_name_in_logs)
{
    auto logger = logger_scope.mock_logger;
    static constexpr auto name = "Shevek";

    logger->screen_logs();
    EXPECT_CALL(*logger, log(_, mpt::MockLogger::make_cstring_matcher(StrEq(name)), _)).Times(AtLeast(1));
    setup();

    mp::PowerShell ps{name};
}

TEST_F(PowerShell, write_silent_on_success)
{
    static constexpr auto data = "Abbenay";
    setup([](auto* process) { EXPECT_CALL(*process, write(Eq(data))).WillOnce(Return(std::strlen(data))); });

    mp::PowerShell ps{"Bedap"};

    logger_scope.mock_logger->screen_logs();
    ASSERT_TRUE(mpt::PowerShellTestAccessor{ps}.write(data));
}

TEST_F(PowerShell, write_logs_on_failure)
{
    static constexpr auto data = "Nio Esseia";
    setup([](auto* process) { EXPECT_CALL(*process, write(Eq(data))).WillOnce(Return(-1)); });

    mp::PowerShell ps{"Takver"};

    auto logger = logger_scope.mock_logger;
    logger->screen_logs();
    logger->expect_log(mpl::Level::warning, "Failed to send");
    ASSERT_FALSE(mpt::PowerShellTestAccessor{ps}.write(data));
}

TEST_F(PowerShell, write_logs_writen_bytes_on_failure)
{
    static constexpr auto data = "Anarres";
    static constexpr auto part = 3;
    setup([](auto* process) { EXPECT_CALL(*process, write(Eq(data))).WillOnce(Return(part)); });

    mp::PowerShell ps{"Palat"};

    auto logger = logger_scope.mock_logger;
    logger->screen_logs();
    logger->expect_log(mpl::Level::warning, fmt::format("{} bytes", part));
    ASSERT_FALSE(mpt::PowerShellTestAccessor{ps}.write(data));
}

TEST_F(PowerShell, run_writes_and_logs_cmd)
{
    static constexpr auto cmdlet = "some cmd and args";
    auto logger = logger_scope.mock_logger;
    logger->screen_logs(mpl::Level::error);
    logger->expect_log(mpl::Level::trace, cmdlet);

    setup([](auto* process) {
        EXPECT_CALL(*process, write(Eq(QByteArray{cmdlet}.append('\n'))))
            .WillOnce(Return(-1)); // short-circuit the attempt
    });

    mp::PowerShell ps{"Tirin"};
    ASSERT_FALSE(ps.run(QString{cmdlet}.split(' ')));
}

struct TestPSStatusAndOutput : public PowerShell, public WithParamInterface<bool>
{
    QByteArray get_status()
    {
        return GetParam() ? " True\n" : " False\n";
    }

    QByteArray end_marker()
    {
        return QByteArray{"\n"}.append(mpt::PowerShellTestAccessor::output_end_marker).append(get_status());
    }

    void expect_writes(mpt::MockProcess* process)
    {
        EXPECT_CALL(*process, write(Eq(QByteArray{cmdlet}.append('\n'))));
        EXPECT_CALL(*process, write(Property(&QByteArray::toStdString,
                                             HasSubstr(mpt::PowerShellTestAccessor::output_end_marker.toStdString()))));
    }

    QString run()
    {
        mp::PowerShell ps{"Gvarab"};
        QString output;
        EXPECT_EQ(ps.run(QString{cmdlet}.split(' '), output), GetParam());

        return output;
    }

    inline static constexpr auto cmdlet = "gimme data";
};

TEST_P(TestPSStatusAndOutput, run_returns_cmdlet_status_and_output)
{
    static constexpr auto data = "here's data";
    auto logger = logger_scope.mock_logger;
    logger->screen_logs(mpl::Level::warning);
    logger->expect_log(mpl::Level::trace, fmt::format("{}", GetParam()));

    setup([this](auto* process) {
        expect_writes(process);
        EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(QByteArray{data}.append(end_marker())));
    });

    ASSERT_EQ(run(), data);
}

TEST_P(TestPSStatusAndOutput, run_handles_trickling_output)
{
    static constexpr auto datum1 = "blah";
    static constexpr auto datum2 = "bleh";
    static constexpr auto datum3 = "blih";
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);

    setup([this](auto* process) {
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

    ASSERT_EQ(run(), QString{datum1} + datum2 + datum3);
};

auto halves(const QString& str)
{
    auto total = str.size();
    auto half = total / 2;

    return std::make_pair(str.leftRef(half).toUtf8(), str.rightRef(total - half).toUtf8());
};

TEST_P(TestPSStatusAndOutput, run_handles_split_end_marker)
{
    static constexpr auto data = "lots of info";
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);

    setup([this](auto* process) {
        const auto marker_halves = halves(mpt::PowerShellTestAccessor::output_end_marker);
        const auto status_halves = halves(get_status());

        expect_writes(process);
        EXPECT_CALL(*process, read_all_standard_output)
            .WillOnce(Return(QByteArray{data}.append('\n')))
            .WillOnce(Return(marker_halves.first))
            .WillOnce(Return(marker_halves.second))
            .WillOnce(Return(status_halves.first))
            .WillOnce(Return(status_halves.second));
    });

    ASSERT_EQ(run(), QString{data});
}

INSTANTIATE_TEST_SUITE_P(PowerShell, TestPSStatusAndOutput, Values(true, false));

TEST_F(PowerShell, exec_runs_given_cmd)
{
    static constexpr auto cmdlet = "make me a sandwich";
    const auto args = QString{cmdlet}.split(' ');

    auto logger = logger_scope.mock_logger;
    const auto log_matcher = mpt::MockLogger::make_cstring_matcher(ContainsRegex(args.join(".*").toStdString()));
    logger->screen_logs(mpl::Level::warning);
    EXPECT_CALL(*logger, log(_, _, log_matcher));

    setup([&args](auto* process) { EXPECT_EQ(process->arguments(), args); });
    mp::PowerShell::exec(args, "Mitis");
}
} // namespace
