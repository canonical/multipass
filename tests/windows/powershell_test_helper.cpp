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

namespace mpt = multipass::test;

void mpt::PowerShellTestHelper::mock_ps_exec(const QByteArray& output, bool succeed)
{
    setup(
        [output, succeed](auto* process) {
            InSequence seq;

            auto emit_ready_read = [process] { emit process->ready_read_standard_output(); };
            EXPECT_CALL(*process, start).WillOnce(Invoke(emit_ready_read));
            EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(output));
            EXPECT_CALL(*process, wait_for_finished).WillOnce(Return(succeed));
            EXPECT_CALL(*process, process_id).WillOnce(Return(9999));
        },
        /* auto_exit = */ false);
}

void mpt::PowerShellTestHelper::setup_mocked_run_sequence(std::vector<RunSpec> runs)
{
    setup([this, runs_ = std::move(runs)](auto* process) {
        InSequence seq;
        for (const auto& run : runs_)
            add_mocked_run(process, run);
    });
}

void mpt::PowerShellTestHelper::setup(const MockProcessFactory::Callback& callback, bool auto_exit)
{
    factory_scope->register_callback([this, callback, auto_exit](MockProcess* process) {
        setup_process(process, auto_exit);
        if (callback)
            callback(process);
    });
}

bool mpt::PowerShellTestHelper::ps_write(multipass::PowerShell& ps, const QByteArray& data)
{
    return ps.write(data);
}

QByteArray mpt::PowerShellTestHelper::get_status(bool succeed) const
{
    return succeed ? " True\n" : " False\n";
}

QByteArray mpt::PowerShellTestHelper::end_marker(bool succeed) const
{
    return QByteArray{"\n"}.append(output_end_marker.toUtf8()).append(get_status(succeed));
}

void mpt::PowerShellTestHelper::expect_writes(MockProcess* process, QByteArray cmdlet) const
{
    auto cmdlet_matcher = Truly([expect = std::move(cmdlet)](const QByteArray& got) { return got.contains(expect); });
    EXPECT_CALL(*process, write(cmdlet_matcher)).WillOnce(Return(written));

    auto marker_matcher = Property(&QByteArray::toStdString, HasSubstr(output_end_marker.toStdString()));
    EXPECT_CALL(*process, write(marker_matcher)).WillOnce(Return(written));
}

bool mpt::PowerShellTestHelper::was_ps_run() const
{
    return forked;
}

void mpt::PowerShellTestHelper::setup_process(MockProcess* process, bool auto_exit)
{
    if (process->program() == psexe)
    {
        // succeed these by default
        if (auto_exit)
        {
            EXPECT_CALL(*process, write(Eq(psexit))).WillOnce(Return(written));
            EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(true));
        }

        forked = true;
    }
}

void mpt::PowerShellTestHelper::add_mocked_run(MockProcess* process, const RunSpec& run)
{
    const auto& [cmdlet, output, result] = run;
    expect_writes(process, QByteArray::fromStdString(cmdlet));

    auto ps_output = QByteArray::fromStdString(output).append(end_marker(result));
    EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(ps_output));
}
