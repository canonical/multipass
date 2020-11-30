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

#ifndef MULTIPASS_POWER_SHELL_TEST_H
#define MULTIPASS_POWER_SHELL_TEST_H

#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"

#include <gtest/gtest.h>

namespace mpt = multipass::test;
using namespace testing;

namespace multipass::test
{
struct PowerShellTest : public Test
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

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    std::unique_ptr<mpt::MockProcessFactory::Scope> factory_scope = mpt::MockProcessFactory::Inject();
    inline static constexpr auto psexit = "Exit\n";

private:
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
    inline static constexpr auto psexe = "powershell.exe";
};
} // namespace multipass::test

#endif // MULTIPASS_POWER_SHELL_TEST_H
