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

    void check(mpt::MockProcess* process)
    {
        ASSERT_EQ(process->program(), psexe);
        forked = true;
    }

    bool forked = false;
};

TEST_F(PowerShell, creates_ps_process)
{
    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::error);

    auto factory_scope = mpt::MockProcessFactory::Inject();
    factory_scope->register_callback([this](mpt::MockProcess* process) {
        check(process);
        EXPECT_CALL(*process, start()).Times(1);
    });

    mp::PowerShell ps{"test"};
}

TEST_F(PowerShell, exits_ps_process)
{
    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::info);

    auto factory_scope = mpt::MockProcessFactory::Inject();
    factory_scope->register_callback([this](mpt::MockProcess* process) {
        check(process);
        EXPECT_CALL(*process, write(Eq(psexit))).WillOnce(Return(std::strlen(psexit)));
        EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(true));
    });

    mp::PowerShell ps{"test"};
}

TEST_F(PowerShell, handles_failure_to_write_on_exit)
{
    auto logger_scope = mpt::MockLogger::inject();
    auto logger = logger_scope.mock_logger;
    logger->screen_logs(mpl::Level::error);

    auto factory_scope = mpt::MockProcessFactory::Inject();
    factory_scope->register_callback([this, logger](mpt::MockProcess* process) {
        check(process);
        EXPECT_CALL(*process, write(Eq(psexit))).WillOnce(Return(-1));
        logger->expect_log(mpl::Level::warning, "Failed to exit");
        EXPECT_CALL(*process, kill);
    });

    mp::PowerShell ps{"test"};
}

TEST_F(PowerShell, handles_failure_to_finish_on_exit)
{
    auto logger_scope = mpt::MockLogger::inject();
    auto logger = logger_scope.mock_logger;
    logger->screen_logs(mpl::Level::error);

    auto factory_scope = mpt::MockProcessFactory::Inject();
    factory_scope->register_callback([this, logger](mpt::MockProcess* process) {
        check(process);
        EXPECT_CALL(*process, write(Eq(psexit))).WillOnce(Return(std::strlen(psexit)));
        EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(false));

        auto err = "timeout";
        EXPECT_CALL(*process, error_string).WillOnce(Return(err));

        auto msg_matcher = mpt::MockLogger::make_cstring_matcher(AllOf(HasSubstr("Failed to exit"), HasSubstr(err)));
        EXPECT_CALL(*logger, log(mpl::Level::warning, _, msg_matcher));
        EXPECT_CALL(*process, kill);
    });

    mp::PowerShell ps{"test"};
}
} // namespace
