
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
#include "tests/mock_file_ops.h"
#include "tests/mock_process_factory.h"
#include "tests/mock_standard_paths.h"
#include "tests/mock_utils.h"
#include "tests/temp_dir.h"

#include <src/platform/platform_proprietary.h>

#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/platform.h>
#include <multipass/utils.h>

#include <QKeySequence>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mpu = multipass::utils;
using namespace testing;

namespace
{

std::unordered_map<std::string, QByteArray> ifconfig_output{
    {"lo0", QByteArrayLiteral("lo0: flags=8049<UP,LOOPBACK,RUNNING,MULTICAST> mtu 16384\n"
                              "\toptions=1203<RXCSUM,TXCSUM,TXSTATUS,SW_TIMESTAMP>\n"
                              "\tinet 127.0.0.1 netmask 0xff000000 \n"
                              "\tinet6 ::1 prefixlen 128 \n"
                              "\tinet6 fe80::1%lo0 prefixlen 64 scopeid 0x1 \n"
                              "\tnd6 options=201<PERFORMNUD,DAD>\n")},
    {"gif0", QByteArrayLiteral("gif0: flags=8010<POINTOPOINT,MULTICAST> mtu 1280\n")},
    {"stf0", QByteArrayLiteral("stf0: flags=0<> mtu 1280\n")},
    {"en0", QByteArrayLiteral("en0: flags=8863<UP,BROADCAST,SMART,RUNNING,SIMPLEX,MULTICAST> mtu 1500\n"
                              "\toptions=50b<RXCSUM,TXCSUM,VLAN_HWTAGGING,AV,CHANNEL_IO>\n"
                              "\tether 98:10:e8:f3:14:97 \n"
                              "\tinet 10.2.0.42 netmask 0xffffff00 broadcast 10.2.0.255\n"
                              "\tmedia: autoselect (1000baseT <full-duplex,flow-control,energy-efficient-ethernet>)\n"
                              "\tstatus: active\n")},
    {"en1", QByteArrayLiteral("en1: flags=8823<UP,BROADCAST,SMART,SIMPLEX,MULTICAST> mtu 1500\n"
                              "\toptions=400<CHANNEL_IO>\n"
                              "\tether dc:a4:ca:f3:67:03 \n"
                              "\tmedia: autoselect (<unknown type>)\n"
                              "\tstatus: inactive\n")},
    {"en2", QByteArrayLiteral("en2: flags=8963<UP,BROADCAST,SMART,RUNNING,PROMISC,SIMPLEX,MULTICAST> mtu 1500\n"
                              "\toptions=460<TSO4,TSO6,CHANNEL_IO>\n"
                              "\tether 82:15:07:f3:c5:40 \n"
                              "\tmedia: autoselect <full-duplex>\n"
                              "\tstatus: inactive\n")},
    {"en3", QByteArrayLiteral("en3: flags=8963<UP,BROADCAST,SMART,RUNNING,PROMISC,SIMPLEX,MULTICAST> mtu 1500\n"
                              "\toptions=460<TSO4,TSO6,CHANNEL_IO>\n"
                              "\tether 82:15:07:f3:c5:41 \n"
                              "\tmedia: autoselect <full-duplex>\n"
                              "\tstatus: inactive\n")},
    {"en11", QByteArrayLiteral("en11: flags=8963<UP,BROADCAST,SMART,RUNNING,PROMISC,SIMPLEX,MULTICAST> mtu 1500\n"
                               "\toptions=460<TSO4,TSO6,CHANNEL_IO>\n"
                               "\tether 82:f1:07:f3:c5:41 \n"
                               "\tmedia: autoselect <full-duplex>\n"
                               "\tstatus: inactive\n")},
    {"en12", QByteArrayLiteral("en12: flags=8963<UP,BROADCAST,SMART,RUNNING,PROMISC,SIMPLEX,MULTICAST> mtu 1500\n"
                               "\toptions=460<TSO4,TSO6,CHANNEL_IO>\n"
                               "\tether 82:f1:07:f3:c1:42 \n"
                               "\tmedia: autoselect <full-duplex>\n"
                               "\tstatus: inactive\n")},
    {"bridge0", QByteArrayLiteral("bridge0: flags=8863<UP,BROADCAST,SMART,RUNNING,SIMPLEX,MULTICAST> mtu 1500\n"
                                  "\toptions=63<RXCSUM,TXCSUM,TSO4,TSO6>\n"
                                  "\tether 82:15:07:f3:c5:40 \n"
                                  "\tConfiguration:\n"
                                  "\t	id 0:0:0:0:0:0 priority 0 hellotime 0 fwddelay 0\n"
                                  "\t	maxage 0 holdcnt 0 proto stp maxaddr 100 timeout 1200\n"
                                  "\t	root id 0:0:0:0:0:0 priority 0 ifcost 0 port 0\n"
                                  "\t	ipfilter disabled flags 0x0\n"
                                  "\tmember: en2 flags=3<LEARNING,DISCOVER>\n"
                                  "\t        ifmaxaddr 0 port 6 priority 0 path cost 0\n"
                                  "\tmember: en3 flags=3<LEARNING,DISCOVER>\n"
                                  "\t        ifmaxaddr 0 port 7 priority 0 path cost 0\n"
                                  "\tmedia: <unknown type>\n"
                                  "\tstatus: inactive\n")},
    {"bridge2", QByteArrayLiteral("bridge2: flags=8863<UP,BROADCAST,SMART,RUNNING,SIMPLEX,MULTICAST> mtu 1500\n"
                                  "\toptions=63<RXCSUM,TXCSUM,TSO4,TSO6>\n"
                                  "\tether a6:83:e7:56:07:02\n"
                                  "\tConfiguration:\n"
                                  "\t        id 0:0:0:0:0:0 priority 0 hellotime 0 fwddelay 0\n"
                                  "\t        maxage 0 holdcnt 0 proto stp maxaddr 100 timeout 1200\n"
                                  "\t        root id 0:0:0:0:0:0 priority 0 ifcost 0 port 0\n"
                                  "\t        ipfilter disabled flags 0x0\n"
                                  "\tAddress cache:\n"
                                  "\tnd6 options=201<PERFORMNUD,DAD>\n"
                                  "\tmedia: <unknown type>\n"
                                  "\tstatus: inactive\n")},
    {"p2p0", QByteArrayLiteral("p2p0: flags=8802<BROADCAST,SIMPLEX,MULTICAST> mtu 2304\n"
                               "\toptions=400<CHANNEL_IO>\n"
                               "\tether 0e:a4:ca:f3:67:03 \n"
                               "\tmedia: autoselect\n"
                               "\tstatus: inactive\n")},
    {"awdl0", QByteArrayLiteral("awdl0: flags=8903<UP,BROADCAST,PROMISC,SIMPLEX,MULTICAST> mtu 1484\n"
                                "\toptions=400<CHANNEL_IO>\n"
                                "\tether 86:fd:f6:fe:81:c1 \n"
                                "\tnd6 options=201<PERFORMNUD,DAD>\n"
                                "\tmedia: autoselect\n"
                                "\tstatus: inactive\n")},
    {"llw0", QByteArrayLiteral("llw0: flags=8822<BROADCAST,SMART,SIMPLEX,MULTICAST> mtu 1500\n"
                               "\toptions=400<CHANNEL_IO>\n"
                               "\tether 86:fd:f6:fe:81:c1 \n")},
    {"utun0", QByteArrayLiteral("utun0: flags=8051<UP,POINTOPOINT,RUNNING,MULTICAST> mtu 1380\n"
                                "\tinet6 fe80::a0ac:cff1:2263:c3d2%utun0 prefixlen 64 scopeid 0xc \n"
                                "\tnd6 options=201<PERFORMNUD,DAD>\n")},
    {"utun1", QByteArrayLiteral("utun1: flags=8051<UP,POINTOPOINT,RUNNING,MULTICAST> mtu 2000\n"
                                "\tinet6 fe80::e899:920a:c955:b124%utun1 prefixlen 64 scopeid 0xd \n"
                                "\tnd6 options=201<PERFORMNUD,DAD>\n")},
    {"utun2", QByteArrayLiteral("utun2: flags=8051<UP,POINTOPOINT,RUNNING,MULTICAST> mtu 1500\n"
                                "\tinet 10.8.0.6 --> 10.8.0.5 netmask 0xffffffff\n")},
    {"utun3", QByteArrayLiteral("utun3: flags=8051<UP,POINTOPOINT,RUNNING,MULTICAST> mtu 1380\n"
                                "\tinet6 fe80::4ba1:886c:9e17:fd30%utun3 prefixlen 64 scopeid 0xf \n"
                                "\tnd6 options=201<PERFORMNUD,DAD>\n")},
    {"utun4", QByteArrayLiteral("utun4: flags=8051<UP,POINTOPOINT,RUNNING,MULTICAST> mtu 2000\n"
                                "\tinet6 fe80::15f1:255f:bb39:92a9%utun4 prefixlen 64 scopeid 0x10 \n"
                                "\tnd6 options=201<PERFORMNUD,DAD>\n")}};

QByteArray networksetup_output =
    QByteArrayLiteral("\nHardware Port: Ethernet\nDevice: en0\nEthernet Address: 98:10:e8:f3:14:97\n"
                      "\nHardware Port: Wi-Fi\nDevice: en1\nEthernet Address: dc:a4:ca:f3:67:03\n"
                      "\nHardware Port: Bluetooth PAN\nDevice: en4\nEthernet Address: dc:a4:ca:f3:67:04\n"
                      "\nHardware Port: Thunderbolt 1\nDevice: en2\nEthernet Address: 82:15:07:f3:c5:40\n"
                      "\nHardware Port: Thunderbolt 2\nDevice: en3\nEthernet Address: 82:15:07:f3:c5:41\n"
                      "\nHardware Port: Thunderbolt Bridge\nDevice: bridge0\nEthernet Address: 82:15:07:f3:c5:40\n"
                      "\nHardware Port: USB 10/100/1000 LAN\nDevice: en11\nEthernet Address: 9c:eb:e8:98:0e:62\n"
                      "\nHardware Port: iPhone USB\nDevice: en12\nEthernet Address: 92:8c:43:1e:b2:1c\n"
                      "\nHardware Port: TestBridge2\nDevice: bridge2\nEthernet Address: a6:83:e7:56:07:02\n"
                      "\nVLAN Configurations\n===================\n"
                      "\nVLAN User Defined Name: TestVLAN\nParent Device: en0\nDevice (\"Hardware\" Port): vlan0"
                      "\nTag: 1\n");

std::unordered_map<std::string, mp::NetworkInterfaceInfo> expect_interfaces{
    {"en0", {"en0", "ethernet", "Ethernet"}},
    {"en1", {"en1", "wifi", "Wi-Fi"}},
    {"en2", {"en2", "thunderbolt", "Thunderbolt 1"}},
    {"en3", {"en3", "thunderbolt", "Thunderbolt 2"}},
    {"en11", {"en11", "usb", "USB 10/100/1000 LAN"}},
    {"en12", {"en12", "usb", "iPhone USB"}},
    {"bridge0", {"bridge0", "bridge", "Network bridge with en2, en3"}},
    {"bridge2", {"bridge2", "bridge", "Empty network bridge"}}};

void simulate_ifconfig(const mpt::MockProcess* process, const mp::ProcessState& exit_status)
{
    ASSERT_EQ(process->program(), "ifconfig");

    QByteArray output;

    const auto& args = process->arguments();
    ASSERT_EQ(args.size(), 0);

    for (const auto& [key, val] : ifconfig_output)
        output += val;

    EXPECT_CALL(*process, execute).WillOnce(Return(exit_status));
    if (exit_status.completed_successfully())
        EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(output));
}

void simulate_networksetup(const mpt::MockProcess* process, const mp::ProcessState& exit_status)
{
    ASSERT_EQ(process->program(), "networksetup");

    const auto& args = process->arguments();
    ASSERT_EQ(args.size(), 1);
    EXPECT_EQ(args.constFirst(), "-listallhardwareports");

    EXPECT_CALL(*process, execute).WillOnce(Return(exit_status));

    if (exit_status.completed_successfully())
    {
        QByteArray success_output = networksetup_output;
        EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(success_output));
    }
    else
    {
        QByteArray fail_output("Fail");

        if (exit_status.exit_code)
            EXPECT_CALL(*process, read_all_standard_error).WillOnce(Return(fail_output));
        else
            ON_CALL(*process, read_all_standard_error).WillByDefault(Return(fail_output));
    }
}

void simulate_environment(const mpt::MockProcess* process, const mp::ProcessState& ifconfig_exit,
                          const mp::ProcessState& networksetup_exit)
{
    auto program = process->program();

    if (program == "ifconfig")
        simulate_ifconfig(process, ifconfig_exit);
    else if (program == "networksetup")
        simulate_networksetup(process, networksetup_exit);
    else
        throw std::runtime_error(fmt::format("Program {} not mocked.", program));
}

TEST(PlatformOSX, test_no_extra_client_settings)
{
    EXPECT_THAT(MP_PLATFORM.extra_client_settings(), IsEmpty());
}

TEST(PlatformOSX, test_no_extra_daemon_settings)
{
    EXPECT_THAT(MP_PLATFORM.extra_daemon_settings(), IsEmpty());
}

TEST(PlatformOSX, test_interpretation_of_winterm_setting_not_supported)
{
    for (const auto x : {"no", "matter", "what"})
        EXPECT_THROW(mp::platform::interpret_setting(mp::winterm_key, x), mp::InvalidSettingException);
}

TEST(PlatformOSX, test_interpretation_of_unknown_settings_not_supported)
{
    for (const auto k : {"unimaginable", "katxama", "katxatxa"})
        for (const auto v : {"no", "matter", "what"})
            EXPECT_THROW(mp::platform::interpret_setting(k, v), mp::InvalidSettingException);
}

TEST(PlatformOSX, test_empty_sync_winterm_profiles)
{
    EXPECT_NO_THROW(mp::platform::sync_winterm_profiles());
}

template <typename Matcher>
void check_interpreted_hotkey(const QString& hotkey, Matcher&& matcher)
{
    auto interpreted_hotkey = mp::platform::interpret_setting(mp::hotkey_key, hotkey);
    EXPECT_THAT(QKeySequence{interpreted_hotkey}.toString(QKeySequence::PortableText).toLower(),
                Property(&QString::toStdString, std::forward<Matcher>(matcher)));
}

TEST(PlatformOSX, test_hotkey_interpretation_replaces_meta_and_opt)
{
    const auto matcher = AllOf(Not(HasSubstr("opt")), Not(HasSubstr("meta")), HasSubstr("alt"));
    for (QString sequence : {"shift+opt+u", "Option+3", "meta+Opt+.", "Meta+Shift+Space"})
        check_interpreted_hotkey(sequence, matcher);
}

TEST(PlatformOSX, test_hotkey_interpretation_replaces_ctrl)
{
    const auto matcher = AllOf(Not(HasSubstr("ctrl")), Not(HasSubstr("control")), HasSubstr("meta"));
    for (QString sequence : {"ctrl+m", "Alt+Ctrl+/", "Control+opt+-"})
        check_interpreted_hotkey(sequence, matcher);
}

TEST(PlatformOSX, test_hotkey_interpretation_replaces_cmd)
{
    const auto matcher = AllOf(Not(HasSubstr("cmd")), Not(HasSubstr("command")), HasSubstr("ctrl"));
    for (QString sequence : {"cmd+t", "ctrl+cmd+u", "Alt+Command+i", "Command+=", "Command+shift+]"})
        check_interpreted_hotkey(sequence, matcher);
}

TEST(PlatformOSX, test_hotkey_interpretation_replaces_mix)
{
    const auto check_cmd = AllOf(Not(HasSubstr("cmd")), Not(HasSubstr("command")), HasSubstr("ctrl"));
    const auto check_opt = AllOf(Not(HasSubstr("opt")), HasSubstr("alt"), Not(HasSubstr("ion"))); // option - opt = ion
    const auto check_ctrl = HasSubstr("meta");
    const auto check_dot = HasSubstr(".");
    const auto check_all = AllOf(check_cmd, check_opt, check_ctrl, check_dot);

    for (QString sequence : {"cmd+meta+ctrl+.", "Control+Command+Option+."})
        check_interpreted_hotkey(sequence, check_all);
}

TEST(PlatformOSX, test_native_hotkey_interpretation)
{
    const QString cmd = u8"⌘", opt = u8"⌥", shift = u8"⇧", ctrl = u8"⌃", tab = u8"⇥";
    check_interpreted_hotkey(cmd + opt + tab, AnyOf(Eq("ctrl+alt+tab"), Eq("alt+ctrl+tab")));
    check_interpreted_hotkey(ctrl + shift + tab, AnyOf(Eq("meta+shift+tab"), Eq("shift+meta+tab")));
    check_interpreted_hotkey(ctrl + opt + tab, AnyOf(Eq("meta+alt+tab"), Eq("alt+meta+tab")));
    check_interpreted_hotkey(cmd + shift + tab, AnyOf(Eq("ctrl+shift+tab"), Eq("shit+ctrl+tab")));
    check_interpreted_hotkey(shift + opt + tab, AnyOf(Eq("shift+alt+tab"), Eq("alt+shift+tab")));
}

TEST(PlatformOSX, test_mixed_hotkey_interpretation)
{
    const QString cmd = u8"⌘", opt = u8"⌥", shift = u8"⇧", ctrl = u8"⌃", tab = u8"⇥";
    check_interpreted_hotkey(cmd + "shift+" + tab, AnyOf(Eq("ctrl+shift+tab"), Eq("shift+ctrl+tab")));
    check_interpreted_hotkey(QString{"Cmd+"} + shift + tab, AnyOf(Eq("ctrl+shift+tab"), Eq("shift+ctrl+tab")));
    check_interpreted_hotkey(ctrl + "opt+" + tab, AnyOf(Eq("meta+alt+tab"), Eq("alt+meta+tab")));
    check_interpreted_hotkey(QString{"ctrl+"} + opt + tab, AnyOf(Eq("meta+alt+tab"), Eq("alt+meta+tab")));

    EXPECT_THAT(mp::platform::interpret_setting(mp::hotkey_key, QString{"Control+"} + shift + "opt+" + tab),
                UnorderedElementsAreArray(ctrl + shift + opt + tab));
}

TEST(PlatformOSX, test_default_driver)
{
    EXPECT_THAT(MP_PLATFORM.default_driver(), AnyOf("qemu", "virtualbox"));
}

TEST(PlatformOSX, test_default_privileged_mounts)
{
    EXPECT_EQ(MP_PLATFORM.default_privileged_mounts(), "true");
}

TEST(PlatformOSX, test_network_interfaces)
{
    std::unique_ptr<mp::test::MockProcessFactory::Scope> mock_factory_scope = mpt::MockProcessFactory::Inject();
    const mp::ProcessState success{0, std::nullopt};
    mock_factory_scope->register_callback(
        [&](mpt::MockProcess* process) { simulate_environment(process, success, success); });

    auto got_interfaces = MP_PLATFORM.get_network_interfaces_info();

    ASSERT_EQ(got_interfaces.size(), expect_interfaces.size());
    for (const auto& [got_name, got_iface] : got_interfaces)
        EXPECT_NO_THROW({
            ASSERT_EQ(got_name, got_iface.id);

            const auto& expected_iface = expect_interfaces.at(got_name);
            EXPECT_EQ(got_iface.id, expected_iface.id);
            EXPECT_EQ(got_iface.type, expected_iface.type);
            EXPECT_EQ(got_iface.description, expected_iface.description);
        });
}

TEST(PlatformOSX, blueprintsURLOverrideSetUnlockSetReturnsExpectedData)
{
    const QString fake_url{"https://a.fake.url"};
    mpt::SetEnvScope blueprints_url("MULTIPASS_BLUEPRINTS_URL", fake_url.toUtf8());
    mpt::SetEnvScope unlock{"MULTIPASS_UNLOCK", mp::platform::unlock_code};

    EXPECT_EQ(MP_PLATFORM.get_blueprints_url_override(), fake_url);
}

TEST(PlatformOSX, blueprintsURLOverrideSetUnlockNotSetReturnsEmptyString)
{
    const QString fake_url{"https://a.fake.url"};
    mpt::SetEnvScope blueprints_url("MULTIPASS_BLUEPRINTS_URL", fake_url.toUtf8());
    mpt::SetEnvScope unlock{"MULTIPASS_UNLOCK", ""};

    EXPECT_TRUE(MP_PLATFORM.get_blueprints_url_override().isEmpty());
}

TEST(PlatformOSX, blueprintsURLOverrideNotSetReturnsEmptyString)
{
    EXPECT_TRUE(MP_PLATFORM.get_blueprints_url_override().isEmpty());
}

TEST(PlatformOSX, create_alias_script_works)
{
    const mpt::TempDir tmp_dir;

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::AppLocalDataLocation))
        .WillOnce(Return(tmp_dir.path()));

    EXPECT_NO_THROW(MP_PLATFORM.create_alias_script("alias_name", mp::AliasDefinition{"instance", "command"}));

    QFile checked_script(tmp_dir.path() + "/bin/alias_name");
    checked_script.open(QFile::ReadOnly);

    EXPECT_EQ(checked_script.readLine().toStdString(), "#!/bin/sh\n");
    EXPECT_EQ(checked_script.readLine().toStdString(), "\n");
    EXPECT_THAT(checked_script.readLine().toStdString(), HasSubstr("alias_name -- \"${@}\"\n"));
    EXPECT_TRUE(checked_script.atEnd());

    auto script_permissions = checked_script.permissions();
    EXPECT_TRUE(script_permissions & QFileDevice::ExeOwner);
    EXPECT_TRUE(script_permissions & QFileDevice::ExeGroup);
    EXPECT_TRUE(script_permissions & QFileDevice::ExeOther);
}

TEST(PlatformOSX, create_alias_script_overwrites)
{
    auto [mock_utils, guard1] = mpt::MockUtils::inject();
    auto [mock_file_ops, guard2] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_utils, make_file_with_content(_, _, true)).Times(1);
    EXPECT_CALL(*mock_file_ops, permissions(_)).WillOnce(Return(QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    EXPECT_CALL(*mock_file_ops, setPermissions(_, _)).WillOnce(Return(true));

    EXPECT_NO_THROW(MP_PLATFORM.create_alias_script("alias_name", mp::AliasDefinition{"instance", "other_command"}));
}

TEST(PlatformOSX, create_alias_script_throws_if_cannot_create_path)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_PLATFORM.create_alias_script("alias_name", mp::AliasDefinition{"instance", "command"}),
                         std::runtime_error, mpt::match_what(HasSubstr("failed to create dir '")));
}

TEST(PlatformOSX, create_alias_script_throws_if_cannot_write_script)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, write(_, _, _)).WillOnce(Return(747));

    MP_EXPECT_THROW_THAT(MP_PLATFORM.create_alias_script("alias_name", mp::AliasDefinition{"instance", "command"}),
                         std::runtime_error, mpt::match_what(HasSubstr("failed to write to file '")));
}

TEST(PlatformOSX, remove_alias_script_works)
{
    const mpt::TempDir tmp_dir;
    QFile script_file(tmp_dir.path() + "/bin/alias_name");

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::AppLocalDataLocation))
        .WillOnce(Return(tmp_dir.path()));

    MP_UTILS.make_file_with_content(script_file.fileName().toStdString(), "script content\n");

    EXPECT_NO_THROW(MP_PLATFORM.remove_alias_script("alias_name"));

    EXPECT_FALSE(script_file.exists());
}

TEST(PlatformOSX, remove_alias_script_throws_if_cannot_remove_script)
{
    const mpt::TempDir tmp_dir;
    QFile script_file(tmp_dir.path() + "/bin/alias_name");

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(mp::StandardPaths::AppLocalDataLocation))
        .WillOnce(Return(tmp_dir.path()));

    MP_EXPECT_THROW_THAT(MP_PLATFORM.remove_alias_script("alias_name"), std::runtime_error,
                         mpt::match_what(StrEq("No such file or directory")));
}
} // namespace
