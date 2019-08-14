/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <src/platform/backends/qemu/dnsmasq_server.h>

#include <multipass/logging/log.h>
#include <multipass/logging/logger.h>

#include "file_operations.h"
#include "temp_dir.h"
#include "test_with_mocked_bin_path.h"
#include <QDir>

#include <memory>
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

    mpt::TempDir data_dir;
    std::shared_ptr<CapturingLogger> logger = std::make_shared<CapturingLogger>();
    const QString bridge_name{"dummy-bridge"};
    const std::string subnet{"192.168.64"};
    const std::string hw_addr{"00:01:02:03:04:05"};
    const std::string expected_ip{"10.177.224.22"};
    const std::string lease_entry =
        "0 "s + hw_addr + " "s + expected_ip + " dummy_name 00:01:02:03:04:05:06:07:08:09:0a:0b:0c:0d:0e:0f:10:11:12";
};
} // namespace

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
