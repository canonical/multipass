/*
 * Copyright (C) 2019-2021 Canonical, Ltd.
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

#include <src/platform/backends/qemu/firewall_config.h>

#include "tests/mock_environment_helpers.h"
#include "tests/mock_process_factory.h"
#include "tests/reset_process_factory.h"

#include <QString>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct IPTablesConfig : public Test
{
    mpt::SetEnvScope env_scope{"DISABLE_APPARMOR", "1"};
    mpt::ResetProcessFactory scope; // will otherwise pollute other tests

    const QString goodbr0{QStringLiteral("goodbr0")};
    const QString evilbr0{QStringLiteral("evilbr0")};
    const std::string subnet{"192.168.2"};

    mpt::MockProcessFactory::Callback iptables_callback = [this](mpt::MockProcess* process) {
        if (process->program() == "iptables")
        {
            if (process->arguments().contains(goodbr0))
            {
                mp::ProcessState exit_state;
                exit_state.exit_code = 0;
                EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
            }
            else if (process->arguments().contains(evilbr0))
            {
                mp::ProcessState exit_state;
                exit_state.exit_code = 1;
                EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
                ON_CALL(*process, read_all_standard_error()).WillByDefault(Return("Evil bridge detected!\n"));
            }
        }
    };
};
} // namespace

TEST_F(IPTablesConfig, iptables_no_error_no_throw)
{
    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(iptables_callback);

    mp::IPTablesConfig iptables_config{goodbr0, subnet};

    EXPECT_NO_THROW(iptables_config.verify_iptables_rules());
}

TEST_F(IPTablesConfig, iptables_error_throws)
{
    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(iptables_callback);

    mp::IPTablesConfig iptables_config{evilbr0, subnet};

    EXPECT_THROW(iptables_config.verify_iptables_rules(), std::runtime_error);
}
