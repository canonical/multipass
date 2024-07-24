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
#include "tests/file_operations.h"
#include "tests/mock_environment_helpers.h"
#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"
#include "tests/reset_process_factory.h"
#include "tests/temp_dir.h"
#include "tests/test_with_mocked_bin_path.h"

#include <src/platform/backends/qemu/linux/dnsmasq_process_spec.h>
#include <src/platform/backends/qemu/linux/dnsmasq_server.h>

#include <multipass/logging/log.h>
#include <multipass/logging/logger.h>

#include <QDir>

#include <memory>
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

    void make_lease_entry(const std::string& expected_hw_addr)
    {
        mpt::make_file_with_content(
            QDir{data_dir.path()}.filePath("dnsmasq.leases"),
            fmt::format("0 {} {} dummy_name 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f:10:11:12", expected_hw_addr,
                        expected_ip));
    }

    void make_lease_entry()
    {
        make_lease_entry(hw_addr);
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

    mp::DNSMasqServer make_default_dnsmasq_server()
    {
        return mp::DNSMasqServer{data_dir.path(), bridge_name, subnet};
    }
};

TEST_F(DNSMasqServer, starts_dnsmasq_process)
{
    EXPECT_NO_THROW(mp::DNSMasqServer dns(data_dir.path(), bridge_name, subnet));
}

TEST_F(DNSMasqServer, finds_ip)
{
    auto dns = make_default_dnsmasq_server();
    make_lease_entry();

    auto ip = dns.get_ip_for(hw_addr);

    ASSERT_TRUE(ip);
    EXPECT_EQ(ip.value(), mp::IPAddress(expected_ip));
}

TEST_F(DNSMasqServer, returns_null_ip_when_leases_file_does_not_exist)
{
    auto dns = make_default_dnsmasq_server();

    const std::string hw_addr{"00:01:02:03:04:05"};
    auto ip = dns.get_ip_for(hw_addr);

    EXPECT_FALSE(ip);
}

TEST_F(DNSMasqServer, release_mac_releases_ip)
{
    const QString dhcp_release_called{QDir{data_dir.path()}.filePath("dhcp_release_called")};

    mp::DNSMasqServer dns{data_dir.path(), dhcp_release_called, subnet};
    make_lease_entry();

    dns.release_mac(hw_addr);

    EXPECT_TRUE(QFile::exists(dhcp_release_called));
}

TEST_F(DNSMasqServer, release_mac_logs_failure_on_missing_ip)
{
    const QString dhcp_release_called{QDir{data_dir.path()}.filePath("dhcp_release_called")};

    mp::DNSMasqServer dns{data_dir.path(), dhcp_release_called, subnet};
    dns.release_mac(hw_addr);

    EXPECT_FALSE(QFile::exists(dhcp_release_called));
    EXPECT_TRUE(logger->logged_lines.size() > 0);
}

TEST_F(DNSMasqServer, release_mac_logs_failures)
{
    const QString dhcp_release_called{QDir{data_dir.path()}.filePath("dhcp_release_called.fail")};

    mp::DNSMasqServer dns{data_dir.path(), dhcp_release_called, subnet};
    make_lease_entry();

    dns.release_mac(hw_addr);

    EXPECT_TRUE(QFile::exists(dhcp_release_called));
    EXPECT_TRUE(logger->logged_lines.size() > 0);
}

TEST_F(DNSMasqServer, release_mac_crashes_logs_failure)
{
    const QString dhcp_release_called{QDir{data_dir.path()}.filePath("dhcp_release_called")};
    const std::string crash_hw_addr{"00:00:00:00:00:00"};

    mp::DNSMasqServer dns{data_dir.path(), dhcp_release_called, subnet};
    make_lease_entry(crash_hw_addr);

    dns.release_mac(crash_hw_addr);

    EXPECT_THAT(logger->logged_lines,
                Contains(fmt::format("failed to release ip addr {} with mac {}: Crashed", expected_ip, crash_hw_addr)));
}

TEST_F(DNSMasqServer, dnsmasq_starts_and_does_not_throw)
{
    auto dns = make_default_dnsmasq_server();

    EXPECT_NO_THROW(dns.check_dnsmasq_running());
}

TEST_F(DNSMasqServer, dnsmasq_fails_and_throws)
{
    EXPECT_THROW((mp::DNSMasqServer{data_dir.path(), bridge_name, error_subnet}), std::runtime_error);
}

TEST_F(DNSMasqServer, dnsmasq_creates_conf_file)
{
    auto dns = make_default_dnsmasq_server();

    EXPECT_FALSE(QDir(data_dir.path()).entryList({"dnsmasq-??????.conf"}, QDir::Files).isEmpty());
}

TEST_F(DNSMasqServer, dnsmasq_creates_empty_dnsmasq_hosts_file)
{
    const QString dnsmasq_hosts{QDir{data_dir.path()}.filePath("dnsmasq.hosts")};
    auto dns = make_default_dnsmasq_server();

    EXPECT_TRUE(QFile::exists(dnsmasq_hosts));
}

struct DNSMasqServerMockedProcess : public DNSMasqServer
{
    void SetUp() override
    {
        logger_scope.mock_logger->screen_logs(mpl::Level::warning); // warning and above expected explicitly in tests
    }

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

TEST_F(DNSMasqServerMockedProcess, dnsmasq_check_skips_start_if_already_running)
{
    setup([this](auto* process) {
        InSequence seq;

        setup_successful_start(process);
        EXPECT_CALL(*process, running()).WillOnce(Return(true));
        setup_successful_finish(process);
    });

    auto dns = make_default_dnsmasq_server();
    dns.check_dnsmasq_running();
}

TEST_F(DNSMasqServerMockedProcess, dnsmasq_check_warns_and_starts_if_not_running)
{
    logger_scope.mock_logger->expect_log(mpl::Level::warning, "Not running");
    setup([this](auto* process) {
        InSequence seq;

        setup_successful_start(process);
        EXPECT_CALL(*process, running()).WillOnce(Return(false));
        setup_successful_start(process);
        setup_successful_finish(process);
    });

    auto dns = make_default_dnsmasq_server();
    dns.check_dnsmasq_running();
}

TEST_F(DNSMasqServerMockedProcess, dnsmasq_throws_on_failure_to_start)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "died");
    setup([](auto* process) {
        InSequence seq;

        EXPECT_CALL(*process, start()).Times(1);
        EXPECT_CALL(*process, wait_for_started(_)).WillOnce(Return(false));
        EXPECT_CALL(*process, kill());
    });

    MP_EXPECT_THROW_THAT(make_default_dnsmasq_server(), std::runtime_error,
                         mpt::match_what(HasSubstr("failed to start")));
}

TEST_F(DNSMasqServerMockedProcess, dnsmasq_throws_when_it_dies_immediately)
{
    constexpr auto msg = "an error msg";
    setup([](auto* process) {
        InSequence seq;

        EXPECT_CALL(*process, start()).Times(1);
        EXPECT_CALL(*process, wait_for_started(_)).WillOnce(Return(true));
        EXPECT_CALL(*process, wait_for_finished(_)).WillOnce(Return(true));

        mp::ProcessState state{2, mp::ProcessState::Error{QProcess::Crashed, msg}};
        EXPECT_CALL(*process, process_state()).WillOnce(Return(state));
    });

    MP_EXPECT_THROW_THAT(make_default_dnsmasq_server(), std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr(msg), HasSubstr("died"), HasSubstr("port 53"))));
}

TEST_F(DNSMasqServerMockedProcess, dnsmasq_logs_error_when_it_dies)
{
    constexpr auto msg = "crash test dummy";
    logger_scope.mock_logger->expect_log(mpl::Level::error, msg);

    mp::Process* dnsmasq_proc = nullptr;
    setup([this, &dnsmasq_proc](auto* process) {
        InSequence seq;

        setup_successful_start(process);
        EXPECT_CALL(*process, running).WillOnce(Return(false));
        dnsmasq_proc = process;
    });

    auto dns = make_default_dnsmasq_server();
    ASSERT_TRUE(dnsmasq_proc);

    mp::ProcessState state{-1, mp::ProcessState::Error{QProcess::Crashed, msg}};
    emit dnsmasq_proc->finished(state);
}
} // namespace
