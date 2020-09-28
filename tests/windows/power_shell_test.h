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

#include <src/platform/backends/shared/win/powershell.h>

#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"

#include <gtest/gtest.h>

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

class PowerShellTest : public Test
{
public:
    void TearDown() override
    {
        ASSERT_TRUE(forked);
    }

    void setup(const MockProcessFactory::Callback& callback = {})
    {
        factory_scope->register_callback([this, callback](MockProcess* process) {
            setup_process(process);
            if (callback)
                callback(process);
        });
    }

    QByteArray get_status(bool succeed)
    {
        return succeed ? " True\n" : " False\n";
    }

    QByteArray end_marker(bool succeed)
    {
        return QByteArray{"\n"}.append(PowerShellTestAccessor::output_end_marker).append(get_status(succeed));
    }

    void expect_writes(MockProcess* process, QByteArray cmdlet)
    {
        EXPECT_CALL(*process, write(Eq(cmdlet.append('\n'))));
        EXPECT_CALL(*process, write(Property(&QByteArray::toStdString,
                                             HasSubstr(PowerShellTestAccessor::output_end_marker.toStdString()))));
    }

    MockLogger::Scope logger_scope = MockLogger::inject();
    std::unique_ptr<MockProcessFactory::Scope> factory_scope = MockProcessFactory::Inject();
    inline static constexpr auto psexit = "Exit\n";

private:
    void setup_process(MockProcess* process)
    {
        ASSERT_EQ(process->program(), psexe);

        // succeed these by default
        ON_CALL(*process, wait_for_finished(_)).WillByDefault(Return(true));
        ON_CALL(*process, write(_)).WillByDefault(Return(written));
        EXPECT_CALL(*process, write(Eq(psexit))).Times(AnyNumber());

        forked = true;
    }

    bool forked = false;
    inline static constexpr auto psexe = "powershell.exe";
    inline static constexpr auto written = 1'000'000;
};

} // namespace multipass::test

#endif // MULTIPASS_POWER_SHELL_TEST_H
