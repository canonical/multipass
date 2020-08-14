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
}

TEST(PowerShell, creates_ps_process)
{
    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::error);

    auto scope = mpt::MockProcessFactory::Inject();
    scope->register_callback([](mpt::MockProcess* process) {
        ASSERT_EQ(process->program(), psexe);
        EXPECT_CALL(*process, start()).Times(1);
    });

    mp::PowerShell ps{"test"};
}

TEST(PowerShell, exits_ps_process)
{
    auto logger_scope = mpt::MockLogger::inject();
    logger_scope.mock_logger->screen_logs(mpl::Level::info);

    auto scope = mpt::MockProcessFactory::Inject();
    scope->register_callback([](mpt::MockProcess* process) {
        ASSERT_EQ(process->program(), psexe);
        EXPECT_CALL(*process, write(Eq(psexit))).WillOnce(Return(std::strlen(psexit)));
        EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(true));
    });

    mp::PowerShell ps{"test"};
}
