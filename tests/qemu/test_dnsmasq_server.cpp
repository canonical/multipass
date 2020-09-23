/*
 * Copyright (C) 2018-2020 Canonical, Ltd.
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

#include <src/platform/backends/qemu/dnsmasq_process_spec.h>
#include <src/platform/backends/qemu/dnsmasq_server.h>

#include <multipass/logging/log.h>
#include <multipass/logging/logger.h>

#include "tests/extra_assertions.h"
#include "tests/file_operations.h"
#include "tests/mock_environment_helpers.h"
#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"
#include "tests/reset_process_factory.h"
#include "tests/temp_dir.h"
#include "tests/test_with_mocked_bin_path.h"

#include <QDir>

#include <memory>
#include <src/platform/backends/qemu/dnsmasq_process_spec.h>
#include <stdexcept>
#include <string>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;
using namespace testing;
using namespace std::string_literals;

namespace
{
struct CapturingLogger : public mp::logging::Logger
{
    void log(mpl::Level level, mpl::CString category, mpl::CString message) const override
    {
        logged_lines.push_back(message.c_str());
    }

    mutable std::vector<std::string> logged_lines;
};

struct DNSMasqServer : public mpt::TestWithMockedBinPath
{
    DNSMasqServer()
    {
        mpl::set_logger(logger);
    }

    ~DNSMasqServer()
    {
        mpl::set_logger(nullptr);
    }

    void make_lease_entry()
    {
        mpt::make_file_with_content(QDir{data_dir.path()}.filePath("dnsmasq.leases"), lease_entry);
    }

    mpt::SetEnvScope env_scope{"DISABLE_APPARMOR", "1"};
    mpt::ResetProcessFactory scope; // will otherwise pollute other tests
    mpt::TempDir data_dir;
    std::shared_ptr<CapturingLogger> logger = std::make_shared<CapturingLogger>();
    const QString bridge_name{"dummy-bridge"};
    const std::string subnet{"192.168.64"};
    const std::string error_subnet{"0.0.0"}; // This forces the mock dnsmasq process to exit with error
    const std::string hw_addr{"00:01:02:03:04:05"};
    const std::string expected_ip{"10.177.224.22"};
    const std::string lease_entry =
        "0 "s + hw_addr + " "s + expected_ip + " dummy_name 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f:10:11:12";
};

struct DNSMasqServerMockedProcess : public DNSMasqServer
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
        ASSERT_EQ(process->program(), exe);
        forked = true;
    }

    void setup_successful_start(mpt::MockProcess* process)
    {
        EXPECT_CALL(*process, start()).Times(1);
        EXPECT_CALL(*process, wait_for_started(_)).WillOnce(Return(true));
        EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(false));
    }

    void setup_successful_finish(mpt::MockProcess* process)
    {
        EXPECT_CALL(*process, running()).WillOnce(Return(true));
        EXPECT_CALL(*process, terminate()).Times(1);
        EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(true));
    }

    bool forked = false;
    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
    std::unique_ptr<mpt::MockProcessFactory::Scope> factory_scope = mpt::MockProcessFactory::Inject();

    inline static const auto exe = mp::DNSMasqProcessSpec{{}, {}, {}, {}}.program();
};
} // namespace

TEST_F(DNSMasqServerMockedProcess, dnsmasq_check_skips_start_if_already_running)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    setup([this](auto* process) {
        InSequence seq;

        setup_successful_start(process);
        EXPECT_CALL(*process, running()).WillOnce(Return(true));
        setup_successful_finish(process);
    });

    mp::DNSMasqServer dns{data_dir.path(), bridge_name, subnet};
    dns.check_dnsmasq_running();
}

TEST_F(DNSMasqServerMockedProcess, dnsmasq_check_warns_and_starts_if_not_running)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    logger_scope.mock_logger->expect_log(mpl::Level::warning, "Not running");
    setup([this](auto* process) {
        InSequence seq;

        setup_successful_start(process);
        EXPECT_CALL(*process, running()).WillOnce(Return(false));
        setup_successful_start(process);
        setup_successful_finish(process);
    });

    mp::DNSMasqServer dns{data_dir.path(), bridge_name, subnet};
    dns.check_dnsmasq_running();
}

TEST_F(DNSMasqServerMockedProcess, dnsmasq_throws_on_failure_to_start)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    logger_scope.mock_logger->expect_log(mpl::Level::error, "died");
    setup([](auto* process) {
        InSequence seq;

        EXPECT_CALL(*process, start()).Times(1);
        EXPECT_CALL(*process, wait_for_started(_)).WillOnce(Return(false));
        EXPECT_CALL(*process, kill());
    });

    MP_EXPECT_THROW_THAT((mp::DNSMasqServer{data_dir.path(), bridge_name, subnet}), std::runtime_error,
                         Property(&std::runtime_error::what, HasSubstr("failed to start")));
}

TEST_F(DNSMasqServerMockedProcess, dnsmasq_throws_when_it_dies_immediately)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);

    constexpr auto msg = "an error msg";
    setup([msg](auto* process) {
        InSequence seq;

        EXPECT_CALL(*process, start()).Times(1);
        EXPECT_CALL(*process, wait_for_started(_)).WillOnce(Return(true));
        EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(true));

        mp::ProcessState state{2, mp::ProcessState::Error{QProcess::Crashed, msg}};
        EXPECT_CALL(*process, process_state()).WillOnce(Return(state));
    });

    MP_EXPECT_THROW_THAT(
        (mp::DNSMasqServer{data_dir.path(), bridge_name, subnet}), std::runtime_error,
        Property(&std::runtime_error::what, AllOf(HasSubstr(msg), HasSubstr("died"), HasSubstr("port 53"))));
}

TEST_F(DNSMasqServerMockedProcess, dnsmasq_logs_error_when_it_dies)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::warning);

    constexpr auto msg = "crash test dummy";
    logger_scope.mock_logger->expect_log(mpl::Level::error, msg);

    mp::Process* dnsmasq_proc = nullptr;
    setup([this, &dnsmasq_proc](auto* process) {
        InSequence seq;

        setup_successful_start(process);
        EXPECT_CALL(*process, running).WillOnce(Return(false));
        dnsmasq_proc = process;
    });

    mp::DNSMasqServer dns{data_dir.path(), bridge_name, subnet};
    ASSERT_TRUE(dnsmasq_proc);

    mp::ProcessState state{-1, mp::ProcessState::Error{QProcess::Crashed, msg}};
    emit dnsmasq_proc->finished(state);
}

TEST_F(DNSMasqServer, starts_dnsmasq_process)
{
    EXPECT_NO_THROW(mp::DNSMasqServer dns(data_dir.path(), bridge_name, subnet));
}

TEST_F(DNSMasqServer, finds_ip)
{
    mp::DNSMasqServer dns{data_dir.path(), bridge_name, subnet};
    make_lease_entry();

    auto ip = dns.get_ip_for(hw_addr);

    ASSERT_TRUE(ip);
    EXPECT_THAT(ip.value(), Eq(mp::IPAddress(expected_ip)));
}

TEST_F(DNSMasqServer, returns_null_ip_when_leases_file_does_not_exist)
{
    mp::DNSMasqServer dns{data_dir.path(), bridge_name, subnet};

    const std::string hw_addr{"00:01:02:03:04:05"};
    auto ip = dns.get_ip_for(hw_addr);

    EXPECT_FALSE(ip);
}

TEST_F(DNSMasqServer, release_mac_releases_ip)
{
    const QString dchp_release_called{QDir{data_dir.path()}.filePath("dhcp_release_called")};

    mp::DNSMasqServer dns{data_dir.path(), dchp_release_called, subnet};
    make_lease_entry();

    dns.release_mac(hw_addr);

    EXPECT_TRUE(QFile::exists(dchp_release_called));
}

TEST_F(DNSMasqServer, release_mac_logs_failure_on_missing_ip)
{
    const QString dchp_release_called{QDir{data_dir.path()}.filePath("dhcp_release_called")};

    mp::DNSMasqServer dns{data_dir.path(), dchp_release_called, subnet};
    dns.release_mac(hw_addr);

    EXPECT_FALSE(QFile::exists(dchp_release_called));
    EXPECT_TRUE(logger->logged_lines.size() > 0);
}

TEST_F(DNSMasqServer, release_mac_logs_failures)
{
    const QString dchp_release_called{QDir{data_dir.path()}.filePath("dhcp_release_called.fail")};

    mp::DNSMasqServer dns{data_dir.path(), dchp_release_called, subnet};
    make_lease_entry();

    dns.release_mac(hw_addr);

    EXPECT_TRUE(QFile::exists(dchp_release_called));
    EXPECT_TRUE(logger->logged_lines.size() > 0);
}

TEST_F(DNSMasqServer, dnsmasq_starts_and_does_not_throw)
{
    mp::DNSMasqServer dns{data_dir.path(), bridge_name, subnet};

    EXPECT_NO_THROW(dns.check_dnsmasq_running());
}

TEST_F(DNSMasqServer, dnsmasq_fails_and_throws)
{
    EXPECT_THROW((mp::DNSMasqServer{data_dir.path(), bridge_name, error_subnet}), std::runtime_error);
}

TEST_F(DNSMasqServer, dnsmasq_creates_conf_file)
{
    mp::DNSMasqServer dns{data_dir.path(), bridge_name, subnet};

    EXPECT_FALSE(QDir(data_dir.path()).entryList({"dnsmasq-??????.conf"}, QDir::Files).isEmpty());
}
