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
    void log(mpl::Level level,
             std::string_view /*category*/,
             std::string_view message) const override
    {
        logged_lines.emplace_back(message);
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
            fmt::format(
                "0 {} {} dummy_name 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f:10:11:12",
                expected_hw_addr,
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

    const QString dummy_bridge{"dummy-bridge"};
    const std::string default_subnet{"192.168.64"};
    const std::string error_subnet{"0.0.0"}; // This forces the mock dnsmasq process to exit with error
    const std::string hw_addr{"00:01:02:03:04:05"};
    const std::string expected_ip{"10.177.224.22"};
    const std::string lease_entry =
        "0 "s + hw_addr + " "s + expected_ip +
        " dummy_name 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f:10:11:12";

    [[nodiscard]] static mp::SubnetList make_subnets(const QString& bridge, const std::string& subnet)
    {
        return {{bridge, subnet}};
    }

    mp::DNSMasqServer make_default_dnsmasq_server() const
    {
        return mp::DNSMasqServer{data_dir.path(), make_subnets(dummy_bridge, default_subnet)};
    }
};

TEST_F(DNSMasqServer, startsDnsmasqProcess)
{
    EXPECT_NO_THROW(mp::DNSMasqServer dns(data_dir.path(), make_subnets(dummy_bridge, default_subnet)));
}

TEST_F(DNSMasqServer, factory_creates_dnsmasq_process)
{
    EXPECT_NO_THROW(
        MP_DNSMASQ_SERVER_FACTORY.make_dnsmasq_server(data_dir.path(), make_subnets(dummy_bridge, default_subnet)));
}

TEST_F(DNSMasqServer, findsIp)
{
    auto dns = make_default_dnsmasq_server();
    make_lease_entry();

    auto ip = dns.get_ip_for(hw_addr);

    ASSERT_TRUE(ip);
    EXPECT_EQ(ip.value(), mp::IPAddress(expected_ip));
}

TEST_F(DNSMasqServer, returnsNullIpWhenLeasesFileDoesNotExist)
{
    auto dns = make_default_dnsmasq_server();

    const std::string hw_addr{"00:01:02:03:04:05"};
    auto ip = dns.get_ip_for(hw_addr);

    EXPECT_FALSE(ip);
}

TEST_F(DNSMasqServer, releaseMacReleasesIp)
{
    const QString dhcp_release_called{QDir{data_dir.path()}.filePath("dhcp_release_called")};

    auto subnets = make_subnets(dhcp_release_called, default_subnet);
    ASSERT_EQ(subnets.size(), 1);

    mp::DNSMasqServer dns{data_dir.path(), subnets};
    make_lease_entry();

    dns.release_mac(hw_addr, subnets.front().first);

    EXPECT_TRUE(QFile::exists(dhcp_release_called));
}

TEST_F(DNSMasqServer, releaseMacLogsFailureOnMissingIp)
{
    const QString dhcp_release_called{QDir{data_dir.path()}.filePath("dhcp_release_called")};

    auto subnets = make_subnets(dhcp_release_called, default_subnet);
    ASSERT_EQ(subnets.size(), 1);

    mp::DNSMasqServer dns{data_dir.path(), subnets};
    dns.release_mac(hw_addr, subnets.front().first);

    EXPECT_FALSE(QFile::exists(dhcp_release_called));
    EXPECT_TRUE(logger->logged_lines.size() > 0);
}

TEST_F(DNSMasqServer, releaseMacLogsFailures)
{
    const QString dhcp_release_called{QDir{data_dir.path()}.filePath("dhcp_release_called.fail")};

    auto subnets = make_subnets(dhcp_release_called, default_subnet);
    ASSERT_EQ(subnets.size(), 1);

    mp::DNSMasqServer dns{data_dir.path(), subnets};
    make_lease_entry();

    dns.release_mac(hw_addr, subnets.front().first);

    EXPECT_TRUE(QFile::exists(dhcp_release_called));
    EXPECT_TRUE(logger->logged_lines.size() > 0);
}

TEST_F(DNSMasqServer, releaseMacCrashesLogsFailure)
{
    const QString dhcp_release_called{QDir{data_dir.path()}.filePath("dhcp_release_called")};
    const std::string crash_hw_addr{"00:00:00:00:00:00"};

    auto subnets = make_subnets(dhcp_release_called, default_subnet);
    ASSERT_EQ(subnets.size(), 1);

    mp::DNSMasqServer dns{data_dir.path(), subnets};
    make_lease_entry(crash_hw_addr);

    dns.release_mac(crash_hw_addr, subnets.front().first);

    EXPECT_THAT(logger->logged_lines,
                Contains(fmt::format("failed to release ip addr {} with mac {}: Crashed",
                                     expected_ip,
                                     crash_hw_addr)));
}

TEST_F(DNSMasqServer, dnsmasqStartsAndDoesNotThrow)
{
    auto dns = make_default_dnsmasq_server();

    EXPECT_NO_THROW(dns.check_dnsmasq_running());
}

TEST_F(DNSMasqServer, dnsmasqFailsAndThrows)
{
    auto error_subnets = make_subnets(dummy_bridge, error_subnet);
    ASSERT_EQ(error_subnets.size(), 1);
    EXPECT_THROW((mp::DNSMasqServer{data_dir.path(), error_subnets}), std::runtime_error);
}

TEST_F(DNSMasqServer, dnsmasqCreatesConfFile)
{
    auto dns = make_default_dnsmasq_server();

    EXPECT_FALSE(QDir(data_dir.path()).entryList({"dnsmasq-??????.conf"}, QDir::Files).isEmpty());
}

TEST_F(DNSMasqServer, dnsmasqCreatesEmptyDnsmasqHostsFile)
{
    const QString dnsmasq_hosts{QDir{data_dir.path()}.filePath("dnsmasq.hosts")};
    auto dns = make_default_dnsmasq_server();

    EXPECT_TRUE(QFile::exists(dnsmasq_hosts));
}

struct DNSMasqServerMockedProcess : public DNSMasqServer
{
    void SetUp() override
    {
        logger_scope.mock_logger->screen_logs(
            mpl::Level::warning); // warning and above expected explicitly in tests
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
    std::unique_ptr<mpt::MockProcessFactory::Scope> factory_scope =
        mpt::MockProcessFactory::Inject();

    inline static const auto exe = mp::DNSMasqProcessSpec{{}, {}, {}}.program();
};

TEST_F(DNSMasqServerMockedProcess, dnsmasqCheckSkipsStartIfAlreadyRunning)
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

TEST_F(DNSMasqServerMockedProcess, dnsmasqCheckWarnsAndStartsIfNotRunning)
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

TEST_F(DNSMasqServerMockedProcess, dnsmasqThrowsOnFailureToStart)
{
    logger_scope.mock_logger->expect_log(mpl::Level::error, "died");
    setup([](auto* process) {
        InSequence seq;

        EXPECT_CALL(*process, start()).Times(1);
        EXPECT_CALL(*process, wait_for_started(_)).WillOnce(Return(false));
        EXPECT_CALL(*process, kill());
    });

    MP_EXPECT_THROW_THAT(make_default_dnsmasq_server(),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("failed to start")));
}

TEST_F(DNSMasqServerMockedProcess, dnsmasqThrowsWhenItDiesImmediately)
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

    MP_EXPECT_THROW_THAT(
        make_default_dnsmasq_server(),
        std::runtime_error,
        mpt::match_what(AllOf(HasSubstr(msg), HasSubstr("died"), HasSubstr("port 53"))));
}

TEST_F(DNSMasqServerMockedProcess, dnsmasqLogsErrorWhenItDies)
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
