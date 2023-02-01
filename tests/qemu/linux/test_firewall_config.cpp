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

#include "tests/common.h"
#include "tests/mock_environment_helpers.h"
#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"
#include "tests/mock_utils.h"
#include "tests/reset_process_factory.h"

#include <src/platform/backends/qemu/linux/firewall_config.h>

#include <multipass/format.h>

#include <QString>

#include <tuple>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct FirewallConfig : public Test
{
    mpt::SetEnvScope env_scope{"DISABLE_APPARMOR", "1"};
    mpt::ResetProcessFactory scope; // will otherwise pollute other tests

    const QString goodbr0{QStringLiteral("goodbr0")};
    const QString evilbr0{QStringLiteral("evilbr0")};
    const std::string subnet{"192.168.2"};

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};

struct FirewallToUseTestSuite : FirewallConfig, WithParamInterface<std::tuple<std::string, QByteArray, QByteArray>>
{
};

struct KernelCheckTestSuite : FirewallConfig, WithParamInterface<std::tuple<std::string, std::string>>
{
};
} // namespace

TEST_F(FirewallConfig, iptablesNftErrorLogsWarningUsesIptablesLegacyByDefault)
{
    const QString error_msg{"Cannot find iptables-nft"};
    mpt::MockProcessFactory::Callback firewall_callback = [&error_msg](mpt::MockProcess* process) {
        if (process->program() == "iptables-nft")
        {
            mp::ProcessState exit_state{1, mp::ProcessState::Error{QProcess::FailedToStart, error_msg}};
            EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(firewall_callback);

    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    logger_scope.mock_logger->expect_log(mpl::Level::warning, fmt::format("Failure: {}", error_msg));

    mp::FirewallConfig firewall_config{goodbr0, subnet};
}

TEST_F(FirewallConfig, firewallVerifyNoErrorDoesNotThrow)
{
    mpt::MockProcessFactory::Callback firewall_callback = [this](mpt::MockProcess* process) {
        if (process->arguments().contains(goodbr0))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 0;
            EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(firewall_callback);

    mp::FirewallConfig firewall_config{goodbr0, subnet};

    EXPECT_NO_THROW(firewall_config.verify_firewall_rules());
}

TEST_F(FirewallConfig, firewallErrorThrowsOnVerify)
{
    const QByteArray msg{"Evil bridge detected!"};

    mpt::MockProcessFactory::Callback firewall_callback = [this, &msg](mpt::MockProcess* process) {
        if (process->arguments().contains(evilbr0))
        {
            mp::ProcessState exit_state;
            exit_state.exit_code = 1;
            EXPECT_CALL(*process, execute(_)).WillOnce(Return(exit_state));
            EXPECT_CALL(*process, read_all_standard_error()).WillOnce(Return(msg));
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(firewall_callback);

    mp::FirewallConfig firewall_config{evilbr0, subnet};

    MP_EXPECT_THROW_THAT(firewall_config.verify_firewall_rules(), std::runtime_error,
                         mpt::match_what(HasSubstr(msg.data())));
}

TEST_F(FirewallConfig, dtorDeletesKnownRules)
{
    const QByteArray base_rule{fmt::format("POSTROUTING -s {}.0/24 ! -d {}.0/24 -m comment --comment \"generated for "
                                           "Multipass network {}\" -j MASQUERADE",
                                           subnet, subnet, goodbr0)
                                   .data()};
    const QByteArray full_rule{"-A " + base_rule};
    bool delete_called{false};

    mpt::MockProcessFactory::Callback firewall_callback = [&base_rule, &full_rule,
                                                           &delete_called](mpt::MockProcess* process) {
        if (process->arguments().contains("--list-rules"))
        {
            EXPECT_CALL(*process, read_all_standard_output()).WillRepeatedly(Return(full_rule));
        }
        else if (process->program() == "sh" && process->arguments().at(1).contains("--delete"))
        {
            delete_called = true;
            EXPECT_TRUE(process->arguments().at(1).contains(base_rule));
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(firewall_callback);

    {
        mp::FirewallConfig firewall_config{goodbr0, subnet};
    }

    EXPECT_TRUE(delete_called);
}

TEST_F(FirewallConfig, dtorDeleteErrorLogsErrorAndContinues)
{
    const QByteArray base_rule{fmt::format("POSTROUTING -s {}.0/24 ! -d {}.0/24 -m comment --comment \"generated for "
                                           "Multipass network {}\" -j MASQUERADE",
                                           subnet, subnet, goodbr0)
                                   .data()};
    const QByteArray full_rule{"-A " + base_rule};
    const QByteArray msg{"Bad stuff happened"};

    mpt::MockProcessFactory::Callback firewall_callback = [&](mpt::MockProcess* process) {
        if (process->arguments().contains("--list-rules"))
        {
            EXPECT_CALL(*process, read_all_standard_output()).WillRepeatedly(Return(full_rule));
        }
        else if (process->program() == "sh" && process->arguments().at(1).contains("--delete"))
        {
            if (process->arguments().at(1).contains(base_rule))
            {
                mp::ProcessState exit_state;
                exit_state.exit_code = 1;
                EXPECT_CALL(*process, execute(_)).WillRepeatedly(Return(exit_state));
                EXPECT_CALL(*process, read_all_standard_error()).WillOnce(Return(msg));
            }
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(firewall_callback);

    logger_scope.mock_logger->screen_logs(mpl::Level::error);
    logger_scope.mock_logger->expect_log(mpl::Level::error, msg.toStdString(), AnyNumber());

    {
        mp::FirewallConfig firewall_config{goodbr0, subnet};
    }
}

TEST_P(FirewallToUseTestSuite, usesExpectedFirewall)
{
    const auto& param = GetParam();

    mpt::MockProcessFactory::Callback firewall_callback = [&param](mpt::MockProcess* process) {
        if (process->program() == "iptables-nft" && process->arguments().contains("--list-rules"))
        {
            EXPECT_CALL(*process, read_all_standard_output()).WillOnce(Return(std::get<1>(param)));
        }
        else if (process->program() == "iptables-legacy" && process->arguments().contains("--list-rules"))
        {
            EXPECT_CALL(*process, read_all_standard_output()).WillOnce(Return(std::get<2>(param)));
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(firewall_callback);

    logger_scope.mock_logger->screen_logs(mpl::Level::info);
    logger_scope.mock_logger->expect_log(mpl::Level::info, std::get<0>(param));

    mp::FirewallConfig firewall_config{goodbr0, subnet};
}

INSTANTIATE_TEST_SUITE_P(FirewallConfig, FirewallToUseTestSuite,
                         Values(std::make_tuple("iptables-legacy", QByteArray(), "-N FOO"),
                                std::make_tuple("iptables-nft", "-N FOO", QByteArray()),
                                std::make_tuple("iptables-nft", QByteArray(), QByteArray()),
                                std::make_tuple("iptables-nft", "-N FOO", "-N FOO")));

TEST_P(KernelCheckTestSuite, usesIptablesAndLogsWithBadKernelInfo)
{
    auto [kernel, msg] = GetParam();
    bool nftables_called{false};

    mpt::MockProcessFactory::Callback firewall_callback = [&nftables_called](mpt::MockProcess* process) {
        if (process->program() == "iptables-legacy" && process->arguments().contains("--list-rules"))
        {
            EXPECT_CALL(*process, read_all_standard_output()).WillOnce(Return(QByteArray()));
        }
        else if (process->program() == "iptables-nft")
        {
            nftables_called = true;
        }
    };

    auto factory = mpt::MockProcessFactory::Inject();
    factory->register_callback(firewall_callback);

    auto [mock_utils, guard] = mpt::MockUtils::inject();
    EXPECT_CALL(*mock_utils, get_kernel_version()).WillOnce(Return(kernel));

    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    logger_scope.mock_logger->expect_log(mpl::Level::info, "iptables-legacy");
    logger_scope.mock_logger->expect_log(mpl::Level::warning, msg);

    mp::FirewallConfig firewall_config{goodbr0, subnet};

    EXPECT_FALSE(nftables_called);
}

INSTANTIATE_TEST_SUITE_P(FirewallConfig, KernelCheckTestSuite,
                         Values(std::make_tuple("undefined", "Cannot parse kernel version \'undefined\'"),
                                std::make_tuple("4.20.1", "Kernel version does not meet minimum requirement of 5.2"),
                                std::make_tuple("5.1.4", "Kernel version does not meet minimum requirement of 5.2")));
