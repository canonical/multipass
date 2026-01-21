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

#pragma once

#include <multipass/disabled_copy_move.h>

#include "tests/unit/mock_logger.h"
#include "tests/unit/mock_process_factory.h"

#include <src/platform/backends/shared/windows/powershell.h>

#include <optional>
#include <string>

using namespace testing;

namespace multipass::test
{
class PowerShellTestHelper : private DisabledCopyMove
{
public:
    PowerShellTestHelper() = default;
    virtual ~PowerShellTestHelper() = default;

    // Mocks powershell to have output in stdout, output_err in stderr, and return succeed.
    // use std::nullopt to specify no stdout/stderr, "" is still some output.
    // only the last call to this function has any effect at the moment the PS process is created
    void mock_ps_exec(const std::optional<QByteArray>& output,
                      const std::optional<QByteArray>& output_err = std::nullopt,
                      bool succeed = true);

    struct RunSpec
    {
        std::string expect_cmdlet_substr;
        std::string will_output = "";
        bool will_return = true;
    };

    // mock the specified sequence of runs
    void setup_mocked_run_sequence(std::vector<RunSpec> runs);

    // setup low-level expectations on the powershell process
    void setup(const MockProcessFactory::Callback& callback = {}, bool auto_exit = true);

    // proxy to private PS::write method
    bool ps_write(PowerShell& ps, const QByteArray& data);

    bool was_ps_run() const;
    QByteArray get_status(bool succeed) const;
    QByteArray end_marker(bool succeed) const;
    void expect_writes(MockProcess* process, QByteArray cmdlet) const;

    inline static constexpr auto psexit = "Exit\n";
    inline static constexpr auto written = 1'000'000;
    inline static const QString& output_end_marker = PowerShell::output_end_marker;

private:
    void setup_process(MockProcess* process, bool auto_exit);
    void add_mocked_run(MockProcess* process, const RunSpec& run);

    bool forked = false;
    std::unique_ptr<MockProcessFactory::Scope> factory_scope = MockProcessFactory::Inject();
    inline static constexpr auto psexe = "powershell.exe";
};

} // namespace multipass::test
