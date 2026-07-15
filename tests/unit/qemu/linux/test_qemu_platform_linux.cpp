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

#include "tests/unit/common.h"
#include "tests/unit/mock_backend_utils.h"
#include "tests/unit/mock_file_ops.h"
#include "tests/unit/mock_logger.h"
#include "tests/unit/mock_utils.h"
#include "tests/unit/stub_availability_zone_manager.h"
#include "tests/unit/temp_dir.h"

#include <multipass/process/process.h>
#include <src/platform/backends/qemu/linux/qemu_platform_linux.h>

#include <QCoreApplication>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpt = multipass::test;

using namespace testing;

namespace
{
struct QemuPlatformLinux : public Test
{
    QemuPlatformLinux() : mock_dnsmasq_server{std::make_unique<mpt::MockDNSMasqServer>()}
    {
        EXPECT_CALL(*mock_utils,
                    run_cmd_for_status(QString("ip"),
                                       Not(ElementsAre(QString("addr"), QString("show"), _)),
                                       _))
            .WillRepeatedly(Return(true));

        for (const auto& vswitch : switches)
        {
            EXPECT_CALL(*mock_firewall_config_factory,
                        make_firewall_config(vswitch.bridge_name, vswitch.subnet))
                .WillOnce([&vswitch](auto...) { return std::move(vswitch.mock_firewall_config); });

            EXPECT_CALL(
                *mock_utils,
                run_cmd_for_status(
                    QString("ip"),
                    ElementsAre(QString("addr"), QString("show"), QString(vswitch.bridge_name)),
                    _))
                .WillOnce(Return(false))
                .WillOnce(Return(true));
        }

        EXPECT_CALL(*mock_dnsmasq_server_factory, make_dnsmasq_server(_, _))
            .WillOnce([this](auto...) { return std::move(mock_dnsmasq_server); });

        EXPECT_CALL(*mock_file_ops, open(_, _)).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_file_ops, write(_, _)).WillRepeatedly(Return(1));
    };

    struct Switch
    {
        QString bridge_name;
        std::string hw_addr;
        mp::Subnet subnet;
        std::string iface_cidr;
        std::string broadcast_addr;
        std::string name;
        mutable std::unique_ptr<multipass::test::MockFirewallConfig> mock_firewall_config;

        Switch(const QString& bridge_name,
               const std::string& hw_addr,
               const mp::Subnet& subnet,
               const std::string& iface_cidr,
               const std::string& broadcast_addr,
               const std::string& name)
            : bridge_name(bridge_name),
              hw_addr(hw_addr),
              subnet(subnet),
              iface_cidr(iface_cidr),
              broadcast_addr(broadcast_addr),
              name(name),
              mock_firewall_config(std::make_unique<mpt::MockFirewallConfig>())
        {
        }

        Switch(const Switch& other)
            : Switch(other.bridge_name,
                     other.hw_addr,
                     other.subnet,
                     other.iface_cidr,
                     other.broadcast_addr,
                     other.name)
        {
        }
    };

    static inline const mp::Subnet zone1_subnet{"192.168.64.0/24"};
    static inline const mp::Subnet zone2_subnet{"192.168.96.0/24"};
    static inline const mp::Subnet zone3_subnet{"192.168.128.0/24"};
    const std::vector<Switch> switches{{"mpqemubrzone1",
                                        "52:54:00:6f:29:7e",
                                        zone1_subnet,
                                        "192.168.64.1/24",
                                        "192.168.64.255",
                                        "foo"},
                                       {"mpqemubrzone2",
                                        "52:54:00:6f:29:7f",
                                        zone2_subnet,
                                        "192.168.96.1/24",
                                        "192.168.96.255",
                                        "bar"},
                                       {"mpqemubrzone3",
                                        "52:54:00:6f:29:80",
                                        zone3_subnet,
                                        "192.168.128.1/24",
                                        "192.168.128.255",
                                        "baz"}};

    mpt::StubAvailabilityZoneManager stub_az_manager{zone1_subnet, zone2_subnet, zone3_subnet};

    mpt::TempDir data_dir;

    std::unique_ptr<mpt::MockDNSMasqServer> mock_dnsmasq_server;

    mpt::MockUtils::GuardedMock utils_attr{mpt::MockUtils::inject<NiceMock>()};
    mpt::MockUtils* mock_utils = utils_attr.first;

    mpt::MockBackend::GuardedMock backend_attr{mpt::MockBackend::inject<NiceMock>()};
    mpt::MockBackend* mock_backend = backend_attr.first;

    mpt::MockDNSMasqServerFactory::GuardedMock dnsmasq_server_factory_attr{
        mpt::MockDNSMasqServerFactory::inject<NiceMock>()};
    mpt::MockDNSMasqServerFactory* mock_dnsmasq_server_factory = dnsmasq_server_factory_attr.first;

    mpt::MockFirewallConfigFactory::GuardedMock firewall_config_factory_attr{
        mpt::MockFirewallConfigFactory::inject<NiceMock>()};
    mpt::MockFirewallConfigFactory* mock_firewall_config_factory =
        firewall_config_factory_attr.first;

    mpt::MockFileOps::GuardedMock file_ops_attr{mpt::MockFileOps::inject<NiceMock>()};
    mpt::MockFileOps* mock_file_ops = file_ops_attr.first;

    mpt::MockLogger::Scope logger_scope = mpt::MockLogger::inject();
};
} // namespace

TEST_F(QemuPlatformLinux, ctorSetsUpExpectedVirtualSwitches)
{
    for (const auto& vswitch : switches)
    {
        EXPECT_CALL(*mock_utils,
                    run_cmd_for_status(QString("ip"),
                                       ElementsAre(QString("link"),
                                                   QString("add"),
                                                   vswitch.bridge_name,
                                                   QString("address"),
                                                   _,
                                                   QString("type"),
                                                   QString("bridge")),
                                       _))
            .WillOnce(Return(true));
        EXPECT_CALL(*mock_utils,
                    run_cmd_for_status(QString("ip"),
                                       ElementsAre(QString("address"),
                                                   QString("add"),
                                                   QString::fromStdString(vswitch.iface_cidr),
                                                   QString("dev"),
                                                   vswitch.bridge_name,
                                                   "broadcast",
                                                   QString::fromStdString(vswitch.broadcast_addr)),
                                       _))
            .WillOnce(Return(true));

        EXPECT_CALL(
            *mock_utils,
            run_cmd_for_status(
                QString("ip"),
                ElementsAre(QString("link"), QString("set"), vswitch.bridge_name, QString("up")),
                _))
            .WillOnce(Return(true));
    }

    mp::QemuPlatformLinux qemu_platform_linux{data_dir.path(), stub_az_manager};
}

TEST_F(QemuPlatformLinux, getIpForReturnsExpectedInfo)
{
    for (const auto& vswitch : switches)
    {
        const mp::IPAddress ip_address{vswitch.subnet.min_address() + 4};
        EXPECT_CALL(*mock_dnsmasq_server, get_ip_for(vswitch.hw_addr))
            .WillOnce([ip = ip_address](auto...) { return ip; });
    }

    mp::QemuPlatformLinux qemu_platform_linux{data_dir.path(), stub_az_manager};

    for (const auto& vswitch : switches)
    {
        const mp::IPAddress ip_address{vswitch.subnet.min_address() + 4};
        auto addr = qemu_platform_linux.get_ip_for(vswitch.hw_addr);

        ASSERT_TRUE(addr.has_value());
        EXPECT_EQ(*addr, ip_address);
    }
}

TEST_F(QemuPlatformLinux, platformArgsGenerateNetResourcesRemovesWorksAsExpected)
{
    mp::VirtualMachineDescription vm_desc;
    mp::NetworkInterface extra_interface{"br-en0", "52:54:00:98:76:54", true};

    const auto& vswitch = switches.front();
    vm_desc.vm_name = vswitch.name;
    vm_desc.zone = "zone1";
    vm_desc.default_mac_address = vswitch.hw_addr;
    vm_desc.extra_interfaces = {extra_interface};

    QStringList saved_opts;

    EXPECT_CALL(*mock_dnsmasq_server, release_mac(vswitch.hw_addr, vswitch.bridge_name))
        .WillOnce(Return());

    EXPECT_CALL(
        *mock_utils,
        run_cmd_for_status(
            QString("ip"),
            ElementsAre(QString("addr"), QString("show"), mpt::match_qstring(StartsWith("tap-"))),
            _))
        .WillOnce(DoAll(SaveArg<1>(&saved_opts), Return(false)));

    mp::QemuPlatformLinux qemu_platform_linux{data_dir.path(), stub_az_manager};

    const auto platform_args = qemu_platform_linux.vm_platform_args(vm_desc);

#if defined Q_PROCESSOR_S390
    const auto network_interface = "virtio-net-ccw";
#else
    const auto network_interface = "virtio-net-pci";
#endif

    // Tests the order and correctness of the arguments returned
    std::vector<QString> expected_platform_args{
    // clang-format off
#if defined Q_PROCESSOR_X86
        "-drive",
        QString("file=%1/edk2-x86_64-code.fd,if=pflash,format=raw,readonly=on,file.locking=off")
            .arg(mp::QemuPlatform::firmware_path()),
#elif defined Q_PROCESSOR_ARM
        "-bios",
        "QEMU_EFI.fd",
        "-machine",
        "virt",
#elif defined Q_PROCESSOR_S390
        "-machine",
        "s390-ccw-virtio",
#elif defined Q_PROCESSOR_POWER
        "-machine",
        "pseries,cap-large-decr=off",
#endif
        "--enable-kvm",
#if defined Q_PROCESSOR_POWER
        "-cpu",
        "POWER9",
#else
        "-cpu",
        "host",
#endif
        // clang-format on
        "-nic",
        QString::fromStdString(fmt::format("tap,ifname={},script=no,downscript=no,model={},mac={}",
                                           saved_opts.last(),
                                           network_interface,
                                           vm_desc.default_mac_address)),
        "-nic",
        QString::fromStdString(fmt::format("bridge,br={},model={},mac={},helper={}",
                                           extra_interface.id,
                                           network_interface,
                                           extra_interface.mac_address,
                                           "./bridge_helper"))};

    EXPECT_THAT(platform_args, ElementsAreArray(expected_platform_args));

    EXPECT_CALL(*mock_utils,
                run_cmd_for_status(QString("ip"),
                                   ElementsAre(QString("addr"), QString("show"), saved_opts.last()),
                                   _))
        .WillOnce(Return(true));
    EXPECT_CALL(
        *mock_utils,
        run_cmd_for_status(QString("ip"),
                           ElementsAre(QString("link"), QString("delete"), saved_opts.last()),
                           _))
        .WillOnce(Return(true));

    qemu_platform_linux.remove_resources_for(vswitch.name);
}

TEST_F(QemuPlatformLinux, tapDevicesAreRemovedOnDestruction)
{
    mp::VirtualMachineDescription vm_desc;
    mp::NetworkInterface extra_interface{"br-en0", "52:54:00:98:76:54", true};

    const auto& vswitch = switches.front();
    vm_desc.vm_name = vswitch.name;
    vm_desc.zone = "zone1";
    vm_desc.default_mac_address = vswitch.hw_addr;
    vm_desc.extra_interfaces = {extra_interface};

    QStringList saved_opts;

    EXPECT_CALL(
        *mock_utils,
        run_cmd_for_status(
            QString("ip"),
            ElementsAre(QString("addr"), QString("show"), mpt::match_qstring(StartsWith("tap-"))),
            _))
        .WillOnce(DoAll(SaveArg<1>(&saved_opts), Return(false)));

    mp::QemuPlatformLinux qemu_platform_linux{data_dir.path(), stub_az_manager};

    const auto platform_args = qemu_platform_linux.vm_platform_args(vm_desc);

    EXPECT_CALL(*mock_utils,
                run_cmd_for_status(QString("ip"),
                                   ElementsAre(QString("addr"), QString("show"), saved_opts.last()),
                                   _))
        .WillOnce(Return(true));
    EXPECT_CALL(
        *mock_utils,
        run_cmd_for_status(QString("ip"),
                           ElementsAre(QString("link"), QString("delete"), saved_opts.last()),
                           _))
        .WillOnce(Return(true));
}

TEST_F(QemuPlatformLinux, createTapDeviceReconfiguresExistingDevice)
{
    mp::VirtualMachineDescription vm_desc;

    const auto& vswitch = switches.front();
    vm_desc.vm_name = vswitch.name;
    vm_desc.zone = "zone1";
    vm_desc.default_mac_address = vswitch.hw_addr;

    QString tap_name;

    auto addr_show_args = ElementsAre(QString("addr"),
                                      QString("show"),
                                      mpt::match_qstring(StartsWith("tap-")));
    auto tuntap_add_args = ElementsAre(QString("tuntap"),
                                       QString("add"),
                                       mpt::match_qstring(StartsWith("tap-")),
                                       QString("mode"),
                                       QString("tap"));

    auto tap_name_matcher = mpt::match_qstring(
        Truly([&tap_name](const std::string& s) { return s == tap_name.toStdString(); }));
    auto link_set_master_args = ElementsAre(QString("link"),
                                            QString("set"),
                                            tap_name_matcher,
                                            QString("master"),
                                            vswitch.bridge_name);
    auto link_set_up_args = ElementsAre(QString("link"),
                                        QString("set"),
                                        tap_name_matcher,
                                        QString("up"));

    // The tap device already exists, so creation must be skipped...
    EXPECT_CALL(*mock_utils, run_cmd_for_status(QString("ip"), addr_show_args, _))
        .WillOnce([&tap_name](auto&, auto& opts, auto...) {
            tap_name = opts.last();
            return true;
        })
        .WillOnce(Return(true));

    EXPECT_CALL(*mock_utils, run_cmd_for_status(QString("ip"), tuntap_add_args, _)).Times(0);

    // ...but the device must still be (re)linked to the bridge and brought up.
    EXPECT_CALL(*mock_utils, run_cmd_for_status(QString("ip"), link_set_master_args, _))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_utils, run_cmd_for_status(QString("ip"), link_set_up_args, _))
        .WillOnce(Return(true));

    mp::QemuPlatformLinux qemu_platform_linux{data_dir.path(), stub_az_manager};

    qemu_platform_linux.vm_platform_args(vm_desc);
}

TEST_F(QemuPlatformLinux, platformHealthCheckCallsExpectedMethods)
{
    EXPECT_CALL(*mock_backend, check_for_kvm_support()).WillOnce(Return());
    EXPECT_CALL(*mock_backend, check_if_kvm_is_in_use()).WillOnce(Return());
    EXPECT_CALL(*mock_dnsmasq_server, check_dnsmasq_running()).WillOnce(Return());

    for (const auto& vswitch : switches)
    {
        EXPECT_CALL(*vswitch.mock_firewall_config, verify_firewall_rules()).WillOnce(Return());
    }

    mp::QemuPlatformLinux qemu_platform_linux{data_dir.path(), stub_az_manager};

    qemu_platform_linux.platform_health_check();
}

TEST_F(QemuPlatformLinux, openingIpforwardFileFailureLogsExpectedMessage)
{
    logger_scope.mock_logger->screen_logs(
        mpl::Level::warning); // warning and above expected explicitly in tests
    logger_scope.mock_logger->expect_log(mpl::Level::warning,
                                         "Unable to open /proc/sys/net/ipv4/ip_forward");

    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(false));

    mp::QemuPlatformLinux qemu_platform_linux{data_dir.path(), stub_az_manager};
}

TEST_F(QemuPlatformLinux, writingIpforwardFileFailureLogsExpectedMessage)
{
    logger_scope.mock_logger->screen_logs(
        mpl::Level::warning); // warning and above expected explicitly in tests
    logger_scope.mock_logger->expect_log(mpl::Level::warning,
                                         "Failed to write to /proc/sys/net/ipv4/ip_forward");

    EXPECT_CALL(*mock_file_ops, write(_, QByteArray("1"))).WillOnce(Return(-1));

    mp::QemuPlatformLinux qemu_platform_linux{data_dir.path(), stub_az_manager};
}

TEST_F(QemuPlatformLinux, platformCorrectlySetsAuthorization)
{
    mp::QemuPlatformLinux qemu_platform_linux{data_dir.path(), stub_az_manager};

    std::vector<mp::NetworkInterfaceInfo> networks{
        mp::NetworkInterfaceInfo{"br-en0", "bridge", "", {"en0"}, false},
        mp::NetworkInterfaceInfo{"mpbr0", "bridge", "", {}, false}};
    const auto& bridged_network =
        networks.emplace_back(mp::NetworkInterfaceInfo{"en0", "ethernet", "", {}, false});
    const auto& non_bridged_network =
        networks.emplace_back(mp::NetworkInterfaceInfo{"en1", "ethernet", "", {}, false});

    qemu_platform_linux.set_authorization(networks);

    EXPECT_FALSE(bridged_network.needs_authorization);
    EXPECT_TRUE(non_bridged_network.needs_authorization);
}

TEST_F(QemuPlatformLinux, createBridgeWithCallsExpectedMethods)
{
    EXPECT_CALL(*mock_backend, create_bridge_with("en0")).WillOnce(Return("br-en0"));

    mp::QemuPlatformLinux qemu_platform_linux{data_dir.path(), stub_az_manager};

    EXPECT_EQ(qemu_platform_linux.create_bridge_with(
                  mp::NetworkInterfaceInfo{"en0", "ethernet", "", {}, true}),
              "br-en0");
}
