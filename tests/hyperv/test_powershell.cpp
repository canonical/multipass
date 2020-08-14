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

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;

namespace multipass::test
{
bool ps_write_accessor(PowerShell& ps, const QByteArray& data)
{
    return ps.write(data);
}
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

        ON_CALL(*process, wait_for_finished(_)).WillByDefault(Return(true));
        ON_CALL(*process, write(Eq(psexit))).WillByDefault(Return(std::strlen(psexit)));

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
    setup([](auto* process) {
        EXPECT_CALL(*process, write(_)).Times(AnyNumber()).WillRepeatedly(Return(1000));
        EXPECT_CALL(*process, write(Eq(data))).WillOnce(Return(std::strlen(data)));
    });

    mp::PowerShell ps{"asdf"};

    logger_scope.mock_logger->screen_logs();
    mpt::ps_write_accessor(ps, data);
}

TEST_F(PowerShell, write_logs_on_failure)
{
    static constexpr auto data = "Nio Esseia";
    setup([](auto* process) {
        EXPECT_CALL(*process, write(_)).Times(AnyNumber()).WillRepeatedly(Return(1000));
        EXPECT_CALL(*process, write(Eq(data))).WillOnce(Return(-1));
    });

    mp::PowerShell ps{"Takver"};

    auto logger = logger_scope.mock_logger;
    logger->screen_logs();
    logger->expect_log(mpl::Level::warning, "Failed to send");
    mpt::ps_write_accessor(ps, data);
}

TEST_F(PowerShell, write_logs_writen_bytes_on_failure)
{
    static constexpr auto data = "Anarres";
    static constexpr auto part = 3;
    setup([](auto* process) {
        EXPECT_CALL(*process, write(_)).Times(AnyNumber()).WillRepeatedly(Return(1000));
        EXPECT_CALL(*process, write(Eq(data))).WillOnce(Return(part));
    });

    mp::PowerShell ps{"Palat"};

    auto logger = logger_scope.mock_logger;
    logger->screen_logs();
    logger->expect_log(mpl::Level::warning, fmt::format("{} bytes", part));
    mpt::ps_write_accessor(ps, data);
}

} // namespace
