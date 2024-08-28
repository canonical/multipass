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

#include "mock_dnsmasq_server.h"
#include "mock_firewall_config.h"

#include "tests/common.h"
#include "tests/mock_backend_utils.h"
#include "tests/mock_file_ops.h"
#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"
#include "tests/mock_utils.h"
#include "tests/temp_dir.h"

#include <src/platform/backends/qemu/linux/qemu_platform_detail.h>

#include <QCoreApplication>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct QemuPlatformDetail : public Test
{
    QemuPlatformDetail()
        : mock_dnsmasq_server{std::make_unique<mpt::MockDNSMasqServer>()},
          mock_firewall_config{std::make_unique<mpt::MockFirewallConfig>()}
    {
        EXPECT_CALL(*mock_backend, get_subnet(_, _)).WillOnce([this](auto...) { return subnet; });

        EXPECT_CALL(*mock_dnsmasq_server_factory, make_dnsmasq_server(_, _, _)).WillOnce([this](auto...) {
            return std::move(mock_dnsmasq_server);
        });

        EXPECT_CALL(*mock_firewall_config_factory, make_firewall_config(_, _)).WillOnce([this](auto...) {
            return std::move(mock_firewall_config);
        });

        EXPECT_CALL(*mock_utils, run_cmd_for_status(QString("ip"), _, _)).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_utils,
                    run_cmd_for_status(QString("ip"), QStringList({"addr", "show", multipass_bridge_name}), _))
            .WillOnce(Return(false))
            .WillOnce(Return(true));

        EXPECT_CALL(*mock_file_ops, open(_, _)).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_file_ops, write(_, _)).WillRepeatedly(Return(1));
    };

    mpt::TempDir data_dir;
    const QString multipass_bridge_name{"mpqemubr0"};
    const std::string hw_addr{"52:54:00:6f:29:7e"};
    const std::string subnet{"192.168.64"};
    const std::string name{"foo"};

    std::unique_ptr<mpt::MockDNSMasqServer> mock_dnsmasq_server;
    std::unique_ptr<mpt::MockFirewallConfig> mock_firewall_config;

    mpt::MockUtils::GuardedMock utils_attr{mpt::MockUtils::inject<NiceMock>()};
    mpt::MockUtils* mock_utils = utils_attr.first;

    mpt::MockBackend::GuardedMock backend_attr{mpt::MockBackend::inject<NiceMock>()};
    mpt::MockBackend* mock_backend = backend_attr.first;

    mpt::MockDNSMasqServerFactory::GuardedMock dnsmasq_server_factory_attr{
        mpt::MockDNSMasqServerFactory::inject<NiceMock>()};
    mpt::MockDNSMasqServerFactory* mock_dnsmasq_server_factory = dnsmasq_server_factory_attr.first;

    mpt::MockFirewallConfigFactory::GuardedMock firewall_config_factory_attr{
        mpt::MockFirewallConfigFactory::inject<NiceMock>()};
    mpt::MockFirewallConfigFactory* mock_firewall_config_factory = firewall_config_factory_attr.first;

    mpt::MockFileOps::GuardedMock file_ops_attr{mpt::MockFileOps::inject<NiceMock>()};
    mpt::MockFileOps* mock_file_ops = file_ops_attr.first;

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};
} // namespace

TEST_F(QemuPlatformDetail, ctor_sets_up_expected_virtual_switch)
{
    const QString qstring_subnet{QString::fromStdString(subnet)};

    EXPECT_CALL(*mock_utils, run_cmd_for_status(QString("ip"),
                                                ElementsAre(QString("link"), QString("add"), multipass_bridge_name,
                                                            QString("address"), _, QString("type"), QString("bridge")),
                                                _))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_utils, run_cmd_for_status(QString("ip"),
                                                ElementsAre(QString("address"), QString("add"),
                                                            QString("%1.1/24").arg(qstring_subnet), QString("dev"),
                                                            multipass_bridge_name, "broadcast",
                                                            QString("%1.255").arg(qstring_subnet)),
                                                _))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_utils, run_cmd_for_status(
                                 QString("ip"),
                                 ElementsAre(QString("link"), QString("set"), multipass_bridge_name, QString("up")), _))
        .WillOnce(Return(true));

    mp::QemuPlatformDetail qemu_platform_detail{data_dir.path()};
}

TEST_F(QemuPlatformDetail, get_ip_for_returns_expected_info)
{
    const mp::IPAddress ip_address{fmt::format("{}.5", subnet)};

    EXPECT_CALL(*mock_dnsmasq_server, get_ip_for(hw_addr)).WillOnce([&ip_address](auto...) { return ip_address; });

    mp::QemuPlatformDetail qemu_platform_detail{data_dir.path()};

    auto addr = qemu_platform_detail.get_ip_for(hw_addr);

    EXPECT_EQ(*addr, ip_address);
}

TEST_F(QemuPlatformDetail, platform_args_generate_net_resources_removes_works_as_expected)
{
    mp::VirtualMachineDescription vm_desc;
    mp::NetworkInterface extra_interface{"br-en0", "52:54:00:98:76:54", true};
    vm_desc.vm_name = "foo";
    vm_desc.default_mac_address = hw_addr;
    vm_desc.extra_interfaces = {extra_interface};

    QString tap_name;

    EXPECT_CALL(*mock_dnsmasq_server, release_mac(hw_addr)).WillOnce(Return());

    EXPECT_CALL(
        *mock_utils,
        run_cmd_for_status(QString("ip"),
                           ElementsAre(QString("addr"), QString("show"), mpt::match_qstring(StartsWith("tap-"))), _))
        .WillOnce([&tap_name](auto& cmd, auto& opts, auto...) {
            tap_name = opts.last();
            return false;
        });

    mp::QemuPlatformDetail qemu_platform_detail{data_dir.path()};

    const auto platform_args = qemu_platform_detail.vm_platform_args(vm_desc);

    // Tests the order and correctness of the arguments returned
    std::vector<QString> expected_platform_args
    {
#if defined Q_PROCESSOR_X86
        "-bios", "OVMF.fd",
#elif defined Q_PROCESSOR_ARM
        "-bios", "QEMU_EFI.fd",
#endif
            "--enable-kvm", "-cpu", "host", "-nic",
            QString::fromStdString(fmt::format("tap,ifname={},script=no,downscript=no,model=virtio-net-pci,mac={}",
                                               tap_name,
                                               vm_desc.default_mac_address)),
            "-nic",
            QString::fromStdString(fmt::format("bridge,br={},model=virtio-net-pci,mac={},helper={}",
                                               extra_interface.id,
                                               extra_interface.mac_address,
                                               QCoreApplication::applicationDirPath() + "/bridge_helper"))
    };

    EXPECT_THAT(platform_args, ElementsAreArray(expected_platform_args));

    EXPECT_CALL(*mock_utils,
                run_cmd_for_status(QString("ip"), ElementsAre(QString("addr"), QString("show"), tap_name), _))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_utils,
                run_cmd_for_status(QString("ip"), ElementsAre(QString("link"), QString("delete"), tap_name), _))
        .WillOnce(Return(true));

    qemu_platform_detail.remove_resources_for(name);
}

TEST_F(QemuPlatformDetail, platform_health_check_calls_expected_methods)
{
    EXPECT_CALL(*mock_backend, check_for_kvm_support()).WillOnce(Return());
    EXPECT_CALL(*mock_backend, check_if_kvm_is_in_use()).WillOnce(Return());
    EXPECT_CALL(*mock_dnsmasq_server, check_dnsmasq_running()).WillOnce(Return());
    EXPECT_CALL(*mock_firewall_config, verify_firewall_rules()).WillOnce(Return());

    mp::QemuPlatformDetail qemu_platform_detail{data_dir.path()};

    qemu_platform_detail.platform_health_check();
}

TEST_F(QemuPlatformDetail, opening_ipforward_file_failure_logs_expected_message)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::warning); // warning and above expected explicitly in tests
    logger_scope.mock_logger->expect_log(mpl::Level::warning, "Unable to open /proc/sys/net/ipv4/ip_forward");

    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(false));

    mp::QemuPlatformDetail qemu_platform_detail{data_dir.path()};
}

TEST_F(QemuPlatformDetail, writing_ipforward_file_failure_logs_expected_message)
{
    logger_scope.mock_logger->screen_logs(mpl::Level::warning); // warning and above expected explicitly in tests
    logger_scope.mock_logger->expect_log(mpl::Level::warning, "Failed to write to /proc/sys/net/ipv4/ip_forward");

    EXPECT_CALL(*mock_file_ops, write(_, QByteArray("1"))).WillOnce(Return(-1));

    mp::QemuPlatformDetail qemu_platform_detail{data_dir.path()};
}

TEST_F(QemuPlatformDetail, platformCorrectlySetsAuthorization)
{
    mp::QemuPlatformDetail qemu_platform_detail{data_dir.path()};

    std::vector<mp::NetworkInterfaceInfo> networks{mp::NetworkInterfaceInfo{"br-en0", "bridge", "", {"en0"}, false},
                                                   mp::NetworkInterfaceInfo{"mpbr0", "bridge", "", {}, false}};
    const auto& bridged_network = networks.emplace_back(mp::NetworkInterfaceInfo{"en0", "ethernet", "", {}, false});
    const auto& non_bridged_network = networks.emplace_back(mp::NetworkInterfaceInfo{"en1", "ethernet", "", {}, false});

    qemu_platform_detail.set_authorization(networks);

    EXPECT_FALSE(bridged_network.needs_authorization);
    EXPECT_TRUE(non_bridged_network.needs_authorization);
}

TEST_F(QemuPlatformDetail, CreateBridgeWithCallsExpectedMethods)
{
    EXPECT_CALL(*mock_backend, create_bridge_with("en0")).WillOnce(Return("br-en0"));

    mp::QemuPlatformDetail qemu_platform_detail{data_dir.path()};

    EXPECT_EQ(qemu_platform_detail.create_bridge_with(mp::NetworkInterfaceInfo{"en0", "ethernet", "", {}, true}),
              "br-en0");
}
