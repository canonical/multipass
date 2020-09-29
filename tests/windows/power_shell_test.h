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
class PowerShellTestHelper // TODO@ricab make uncopyable
{
public:
    virtual ~PowerShellTestHelper() = default;

    // notice only the last call to this function has any effect at the moment the PS process is created
    void mock_ps_exec(const QByteArray& output, bool succeed = true)
    {
        setup([output, succeed](auto* process) {
            InSequence seq;

            auto emit_ready_read = [process] { emit process->ready_read_standard_output(); };
            EXPECT_CALL(*process, start).WillOnce(Invoke(emit_ready_read));
            EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(output));
            EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(succeed));
        });
    }

    struct RunSpec
    {
        std::string expect_cmdlet_substr;
        std::string will_output = "";
        bool will_return = true;
    };

    void setup_mocked_run_sequence(std::vector<RunSpec> runs)
    {
        setup([this, runs_ = std::move(runs)](auto* process) {
            InSequence seq;
            for (const auto& run : runs_)
                add_mocked_run(process, run);
        });
    }

    // setup low-level expectations on the powershell process
    void setup(const MockProcessFactory::Callback& callback = {})
    {
        factory_scope->register_callback([this, callback](MockProcess* process) {
            setup_process(process);
            if (callback)
                callback(process);
        });
    }

    // proxy to private PS::write method
    bool ps_write(PowerShell& ps, const QByteArray& data)
    {
        return ps.write(data);
    }

    QByteArray get_status(bool succeed) const
    {
        return succeed ? " True\n" : " False\n";
    }

    QByteArray end_marker(bool succeed) const
    {
        return QByteArray{"\n"}.append(output_end_marker).append(get_status(succeed));
    }

    void expect_writes(MockProcess* process, QByteArray cmdlet) const
    {
        EXPECT_CALL(*process, write(Eq(cmdlet.append('\n'))));
        EXPECT_CALL(*process, write(Property(&QByteArray::toStdString, HasSubstr(output_end_marker.toStdString()))));
    }

    bool was_ps_run() const
    {
        return forked;
    }

    inline static constexpr auto psexit = "Exit\n";
    inline static const QString& output_end_marker = PowerShell::output_end_marker;

private:
    void setup_process(MockProcess* process)
    {
        ASSERT_EQ(process->program(), psexe); // TODO@ricab make this an if instead, to accommodate other processes

        // succeed these by default
        ON_CALL(*process, wait_for_finished(_)).WillByDefault(Return(true));
        ON_CALL(*process, write(_)).WillByDefault(Return(written));
        EXPECT_CALL(*process, write(Eq(psexit))).Times(AnyNumber());

        forked = true;
    }

    void add_mocked_run(MockProcess* process, const RunSpec& run)
    {
        const auto& [cmdlet, output, result] = run;
        const auto& marker = output_end_marker;

        auto cmdlet_matcher = Property(&QByteArray::toStdString, HasSubstr(cmdlet));
        EXPECT_CALL(*process, write(cmdlet_matcher)).WillOnce(Return(written));

        auto marker_matcher = Property(&QByteArray::toStdString, HasSubstr(marker.toStdString()));
        EXPECT_CALL(*process, write(marker_matcher)).WillOnce(Return(written));

        auto ps_output = fmt::format("{}\n{} {}\n", output, marker, result ? "True" : "False");
        EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(QByteArray::fromStdString(ps_output)));
    }

    bool forked = false;
    std::unique_ptr<MockProcessFactory::Scope> factory_scope = MockProcessFactory::Inject();
    inline static constexpr auto psexe = "powershell.exe";
    inline static constexpr auto written = 1'000'000;
};

} // namespace multipass::test

#endif // MULTIPASS_POWER_SHELL_TEST_H
