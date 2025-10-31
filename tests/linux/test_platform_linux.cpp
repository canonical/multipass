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
#include "tests/fake_handle.h"
#include "tests/file_operations.h"
#include "tests/mock_environment_helpers.h"
#include "tests/mock_file_ops.h"
#include "tests/mock_process_factory.h"
#include "tests/mock_settings.h"
#include "tests/mock_standard_paths.h"
#include "tests/mock_utils.h"
#include "tests/temp_dir.h"
#include "tests/test_with_mocked_bin_path.h"

#include "tests/qemu/linux/mock_dnsmasq_server.h"
#include <src/platform/backends/qemu/qemu_virtual_machine_factory.h>
#include <src/platform/platform_linux_detail.h>

#include <multipass/constants.h>
#include <multipass/exceptions/autostart_setup_exception.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/platform.h>
#include <multipass/utils.h>

#include <scope_guard.hpp>

#include <QDir>
#include <QFile>
#include <QString>

#include <stdexcept>
#include <tests/mock_platform.h>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mpu = multipass::utils;
using namespace testing;

namespace
{
const auto backend_path = QStringLiteral("/tmp");

struct PlatformLinux : public mpt::TestWithMockedBinPath
{
    template <typename VMFactoryType>
    void aux_test_driver_factory(const QString& driver)
    {
        const mpt::MockDNSMasqServerFactory::GuardedMock dnsmasq_server_factory_attr{
            mpt::MockDNSMasqServerFactory::inject<NiceMock>()};

        auto factory = mpt::MockProcessFactory::Inject();
        setup_driver_settings(driver);

        decltype(mp::platform::vm_backend("")) factory_ptr;
        EXPECT_NO_THROW(factory_ptr = mp::platform::vm_backend(backend_path););

        EXPECT_TRUE(dynamic_cast<VMFactoryType*>(factory_ptr.get()));
    }

    void setup_driver_settings(const QString& driver)
    {
        EXPECT_CALL(mock_settings, get(Eq(mp::driver_key))).WillRepeatedly(Return(driver));
    }

    mpt::MockSettings::GuardedMock mock_settings_injection = mpt::MockSettings::inject();
    mpt::MockSettings& mock_settings = *mock_settings_injection.first;
    mpt::SetEnvScope disable_apparmor{"DISABLE_APPARMOR", "1"};
};

TEST_F(PlatformLinux, testInterpretationOfWintermSettingNotSupported)
{
    for (const auto* x : {"no", "matter", "what"})
        EXPECT_THROW(mp::platform::interpret_setting(mp::winterm_key, x),
                     mp::InvalidSettingException);
}

TEST_F(PlatformLinux, testInterpretationOfUnknownSettingsNotSupported)
{
    for (const auto* k : {"unimaginable", "katxama", "katxatxa"})
        for (const auto* v : {"no", "matter", "what"})
            EXPECT_THROW(mp::platform::interpret_setting(k, v), mp::InvalidSettingException);
}

TEST_F(PlatformLinux, testNoExtraClientSettings)
{
    EXPECT_THAT(MP_PLATFORM.extra_client_settings(), IsEmpty());
}

TEST_F(PlatformLinux, testNoExtraDaemonSettings)
{
    EXPECT_THAT(MP_PLATFORM.extra_daemon_settings(), IsEmpty());
}

TEST_F(PlatformLinux, testEmptySyncWintermProfiles)
{
    EXPECT_NO_THROW(mp::platform::sync_winterm_profiles());
}

TEST_F(PlatformLinux, testDefaultDriver)
{
    EXPECT_EQ(MP_PLATFORM.default_driver(), "qemu");
}

TEST_F(PlatformLinux, testDefaultPrivilegedMounts)
{
    EXPECT_EQ(MP_PLATFORM.default_privileged_mounts(), "true");
}

TEST_F(PlatformLinux, testDefaultDriverProducesCorrectFactory)
{
    aux_test_driver_factory<mp::QemuVirtualMachineFactory>("qemu");
}

TEST_F(PlatformLinux, testLibvirtInEnvVarIsIgnored)
{
    mpt::SetEnvScope env(mp::driver_env_var, "LIBVIRT");
    aux_test_driver_factory<mp::QemuVirtualMachineFactory>("qemu");
}

TEST_F(PlatformLinux, testSnapReturnsExpectedDefaultAddress)
{
    const QByteArray base_dir{"/tmp"};
    const QByteArray snap_name{"multipass"};

    mpt::SetEnvScope env("SNAP_COMMON", base_dir);
    mpt::SetEnvScope env2("SNAP_NAME", snap_name);

    EXPECT_EQ(mp::platform::default_server_address(),
              fmt::format("unix:{}/multipass_socket", base_dir.toStdString()));
}

TEST_F(PlatformLinux, testNotSnapReturnsExpectedDefaultAddress)
{
    const QByteArray snap_name{"multipass"};

    mpt::UnsetEnvScope unset_env("SNAP_COMMON");
    mpt::SetEnvScope env2("SNAP_NAME", snap_name);

    EXPECT_EQ(mp::platform::default_server_address(), fmt::format("unix:/run/multipass_socket"));
}

struct TestUnsupportedDrivers : public PlatformLinux, WithParamInterface<QString>
{
};

TEST_P(TestUnsupportedDrivers, testUnsupportedDriver)
{
    ASSERT_FALSE(MP_PLATFORM.is_backend_supported(GetParam()));

    setup_driver_settings(GetParam());
    EXPECT_THROW(mp::platform::vm_backend(backend_path), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(PlatformLinux,
                         TestUnsupportedDrivers,
                         Values(QStringLiteral("hyper-v"), QStringLiteral("other")));

TEST_F(PlatformLinux, retrievesEmptyBridges)
{
    const mpt::TempDir tmp_dir;
    const auto fake_bridge = "somebridge";

    QDir fake_sys_class_net{tmp_dir.path()};
    QDir bridge_dir{fake_sys_class_net.filePath(fake_bridge)};
    mpt::make_file_with_content(bridge_dir.filePath("type"), "1");
    ASSERT_TRUE(bridge_dir.mkpath("bridge"));

    auto net_map = mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path());

    using value_type = decltype(net_map)::value_type;
    using Net = mp::NetworkInterfaceInfo;
    EXPECT_THAT(
        net_map,
        ElementsAre(AllOf(Field(&value_type::first, fake_bridge),
                          Field(&value_type::second,
                                AllOf(Field(&Net::id, fake_bridge),
                                      Field(&Net::type, "bridge"),
                                      Field(&Net::description, StrEq("Network bridge")))))));
}

TEST_F(PlatformLinux, retrievesEthernetDevices)
{
    const mpt::TempDir tmp_dir;
    const auto fake_eth = "someth";

    QDir fake_sys_class_net{tmp_dir.path()};
    mpt::make_file_with_content(fake_sys_class_net.filePath(fake_eth) + "/type", "1");

    auto net_map = mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path());

    ASSERT_EQ(net_map.size(), 1u);

    auto it = net_map.cbegin();
    EXPECT_EQ(it->first, fake_eth);
    EXPECT_EQ(it->second.id, fake_eth);
    EXPECT_EQ(it->second.type, "ethernet");
    EXPECT_EQ(it->second.description, "Ethernet device");
}

TEST_F(PlatformLinux, doesNotRetrieveUnknownNetworks)
{
    const mpt::TempDir tmp_dir;
    const auto fake_nets = {"eth0", "foo", "kkkkk"};

    QDir fake_sys_class_net{tmp_dir.path()};
    for (const auto& net : fake_nets)
        ASSERT_TRUE(fake_sys_class_net.mkpath(net));

    EXPECT_THAT(mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path()),
                IsEmpty());
}

TEST_F(PlatformLinux, doesNotRetrieveOtherVirtual)
{
    const mpt::TempDir tmp_dir;
    const auto fake_virt = "somevirt";

    QDir fake_sys_class_net{tmp_dir.path() + "/virtual"};
    mpt::make_file_with_content(fake_sys_class_net.filePath(fake_virt) + "/type", "1");

    EXPECT_THAT(mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path()),
                IsEmpty());
}

TEST_F(PlatformLinux, doesNotRetrieveWireless)
{
    const mpt::TempDir tmp_dir;
    const auto fake_wifi = "somewifi";

    QDir fake_sys_class_net{tmp_dir.path()};
    QDir wifi_dir{fake_sys_class_net.filePath(fake_wifi)};
    mpt::make_file_with_content(wifi_dir.filePath("type"), "1");
    ASSERT_TRUE(wifi_dir.mkpath("wireless"));

    EXPECT_THAT(mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path()),
                IsEmpty());
}

TEST_F(PlatformLinux, doesNotRetrieveProtocols)
{
    const mpt::TempDir tmp_dir;
    const auto fake_net = "somenet";

    QDir fake_sys_class_net{tmp_dir.path()};
    mpt::make_file_with_content(fake_sys_class_net.filePath(fake_net) + "/type", "32");

    EXPECT_THAT(mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path()),
                IsEmpty());
}

TEST_F(PlatformLinux, doesNotRetrieveOtherSpecifiedDeviceTypes)
{
    const mpt::TempDir tmp_dir;
    const auto fake_net = "somenet";
    const auto uevent_contents = std::string{"asdf\nDEVTYPE=crazytype\nfdsa"};

    QDir fake_sys_class_net{tmp_dir.path()};
    QDir net_dir{fake_sys_class_net.filePath(fake_net)};
    mpt::make_file_with_content(net_dir.filePath("type"), "1");
    mpt::make_file_with_content(net_dir.filePath("uevent"), uevent_contents);

    auto net_map = mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path());

    EXPECT_THAT(mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path()),
                IsEmpty());
}

struct BridgeMemberTest : public PlatformLinux,
                          WithParamInterface<std::vector<std::pair<std::string, bool>>>
{
};

TEST_P(BridgeMemberTest, retrievesBridgesWithMembers)
{
    const mpt::TempDir tmp_dir;
    const auto fake_bridge = "aeiou";

    QDir fake_sys_class_net{tmp_dir.path()};
    QDir interface_dir{fake_sys_class_net.filePath(fake_bridge)};
    QDir members_dir{interface_dir.filePath("brif")};

    mpt::make_file_with_content(interface_dir.filePath("type"), "1");
    ASSERT_TRUE(interface_dir.mkpath("bridge"));
    ASSERT_TRUE(members_dir.mkpath("."));

    using net_value_type = decltype(MP_PLATFORM.get_network_interfaces_info())::value_type;
    std::vector<Matcher<net_value_type>> network_matchers{};
    std::vector<Matcher<std::string>> substrs_matchers{};
    for (const auto& [member, recognized] : GetParam())
    {
        QDir member_dir = fake_sys_class_net.filePath(QString::fromStdString(member));
        ASSERT_TRUE(member_dir.mkpath("."));
        ASSERT_TRUE(members_dir.mkpath(QString::fromStdString(member)));

        if (recognized)
        {
            mpt::make_file_with_content(member_dir.filePath("type"), "1");

            substrs_matchers.push_back(HasSubstr(member));
            network_matchers.push_back(Field(&net_value_type::first, member));
        }
        else
            substrs_matchers.push_back(Not(HasSubstr(member)));
    }

    auto net_map = mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path());

    using Net = mp::NetworkInterfaceInfo;
    network_matchers.push_back(
        AllOf(Field(&net_value_type::first, fake_bridge),
              Field(&net_value_type::second,
                    AllOf(Field(&Net::id, fake_bridge),
                          Field(&Net::type, "bridge"),
                          Field(&Net::description, AllOfArray(substrs_matchers))))));

    EXPECT_THAT(net_map, UnorderedElementsAreArray(network_matchers));
}

using Param = std::vector<std::pair<std::string, bool>>;
INSTANTIATE_TEST_SUITE_P(PlatformLinux,
                         BridgeMemberTest,
                         Values(Param{{"en0", true}},
                                Param{{"en0", false}},
                                Param{{"en0", false}, {"en1", true}},
                                Param{{"asdf", true},
                                      {"ggi", true},
                                      {"a1", true},
                                      {"fu", false},
                                      {"ho", true},
                                      {"ra", false}}));

using OSReleaseTestParam = std::pair<QStringList, std::pair<std::string, std::string>>;
struct OSReleaseTest : public PlatformLinux, WithParamInterface<OSReleaseTestParam>
{
};

OSReleaseTestParam parse_os_release_empty = {
    {{"NAME=\"\""},
     {"VERSION=\"21.04 (Hirsute Hippo)\""},
     {"ID=ubuntu"},
     {"ID_LIKE=debian"},
     {"PRETTY_NAME=\"Ubuntu 21.04\""},
     {"VERSION_ID=\"\""},
     {"HOME_URL=\"https://www.ubuntu.com/\""},
     {"SUPPORT_URL=\"https://help.ubuntu.com/\""},
     {"BUG_REPORT_URL=\"https://bugs.launchpad.net/ubuntu/\""},
     {"PRIVACY_POLICY_URL=\"https://www.ubuntu.com/legal/terms-and-policies/privacy-policy\""},
     {"VERSION_CODENAME=hirsute"},
     {"UBUNTU_CODENAME=hirsute"}},
    {"unknown", "unknown"}};

OSReleaseTestParam parse_os_release_single_char_fields = {
    {{"NAME=\"A\""},
     {"VERSION=\"21.04 (Hirsute Hippo)\""},
     {"ID=ubuntu"},
     {"ID_LIKE=debian"},
     {"PRETTY_NAME=\"Ubuntu 21.04\""},
     {"VERSION_ID=\"B\""},
     {"HOME_URL=\"https://www.ubuntu.com/\""},
     {"SUPPORT_URL=\"https://help.ubuntu.com/\""},
     {"BUG_REPORT_URL=\"https://bugs.launchpad.net/ubuntu/\""},
     {"PRIVACY_POLICY_URL=\"https://www.ubuntu.com/legal/terms-and-policies/privacy-policy\""},
     {"VERSION_CODENAME=hirsute"},
     {"UBUNTU_CODENAME=hirsute"}},
    {"A", "B"}};

OSReleaseTestParam parse_os_release_ubuntu2104lts = {
    {{"NAME=\"Ubuntu\""},
     {"VERSION=\"21.04 (Hirsute Hippo)\""},
     {"ID=ubuntu"},
     {"ID_LIKE=debian"},
     {"PRETTY_NAME=\"Ubuntu 21.04\""},
     {"VERSION_ID=\"21.04\""},
     {"HOME_URL=\"https://www.ubuntu.com/\""},
     {"SUPPORT_URL=\"https://help.ubuntu.com/\""},
     {"BUG_REPORT_URL=\"https://bugs.launchpad.net/ubuntu/\""},
     {"PRIVACY_POLICY_URL=\"https://www.ubuntu.com/legal/terms-and-policies/privacy-policy\""},
     {"VERSION_CODENAME=hirsute"},
     {"UBUNTU_CODENAME=hirsute"}},
    {"Ubuntu", "21.04"}};

OSReleaseTestParam parse_os_release_ubuntu2104lts_rotation = {
    {{"VERSION=\"21.04 (Hirsute Hippo)\""},
     {"ID=ubuntu"},
     {"ID_LIKE=debian"},
     {"VERSION_ID=\"21.04\""},
     {"PRETTY_NAME=\"Ubuntu 21.04\""},
     {"HOME_URL=\"https://www.ubuntu.com/\""},
     {"SUPPORT_URL=\"https://help.ubuntu.com/\""},
     {"BUG_REPORT_URL=\"https://bugs.launchpad.net/ubuntu/\""},
     {"PRIVACY_POLICY_URL=\"https://www.ubuntu.com/legal/terms-and-policies/privacy-policy\""},
     {"VERSION_CODENAME=hirsute"},
     {"NAME=\"Ubuntu\""},
     {"UBUNTU_CODENAME=hirsute"}},
    {"Ubuntu", "21.04"}};

TEST_P(OSReleaseTest, testParseOsRelease)
{
    const auto& [input, expected] = GetParam();

    auto output = multipass::platform::detail::parse_os_release(input);

    EXPECT_EQ(expected.first, output.first.toStdString());
    EXPECT_EQ(expected.second, output.second.toStdString());
}

INSTANTIATE_TEST_SUITE_P(PlatformLinux,
                         OSReleaseTest,
                         Values(OSReleaseTestParam{{}, {"unknown", "unknown"}},
                                parse_os_release_empty,
                                parse_os_release_single_char_fields,
                                parse_os_release_ubuntu2104lts,
                                parse_os_release_ubuntu2104lts_rotation));

TEST_F(PlatformLinux, findOsReleaseNoneFound)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, open(_, _)).Times(2).WillRepeatedly(Return(false));

    auto output = multipass::platform::detail::find_os_release();
    EXPECT_EQ(output->fileName(), "");
}

TEST_F(PlatformLinux, findOsReleaseEtc)
{
    const auto expected_filename = QStringLiteral("/var/lib/snapd/hostfs/etc/os-release");

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    InSequence seq;
    EXPECT_CALL(*mock_file_ops,
                open(Property(&QFileDevice::fileName, Eq(expected_filename)),
                     QIODevice::ReadOnly | QIODevice::Text))
        .Times(1)
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).Times(0); // no other open attempts

    auto output = multipass::platform::detail::find_os_release();
    EXPECT_EQ(output->fileName(), expected_filename);
}

TEST_F(PlatformLinux, findOsReleaseUsrLib)
{
    const auto expected_filename = QStringLiteral("/var/lib/snapd/hostfs/usr/lib/os-release");

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    InSequence seq;
    EXPECT_CALL(*mock_file_ops,
                open(Property(&QFileDevice::fileName, Eq("/var/lib/snapd/hostfs/etc/os-release")),
                     QIODevice::ReadOnly | QIODevice::Text))
        .WillOnce(Return(false));
    EXPECT_CALL(*mock_file_ops,
                open(Property(&QFileDevice::fileName, Eq(expected_filename)),
                     QIODevice::ReadOnly | QIODevice::Text))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).Times(0); // no other open attempts

    auto output = multipass::platform::detail::find_os_release();
    EXPECT_EQ(output->fileName(), expected_filename);
}

TEST_F(PlatformLinux, readOsReleaseFromFileNotFound)
{
    const std::string expected = "unknown-unknown";

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, open(_, _)).Times(2).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_file_ops, is_open(testing::A<const QFile&>())).WillOnce(Return(false));

    auto output = multipass::platform::detail::read_os_release();

    EXPECT_EQ(expected, output);
}

TEST_F(PlatformLinux, readOsReleaseFromFile)
{
    const auto& [input, expected_pair] = parse_os_release_ubuntu2104lts;
    auto expected = fmt::format("{}-{}", expected_pair.first, expected_pair.second);

    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    InSequence seq;
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, is_open(testing::A<const QFile&>())).WillOnce(Return(true));

    for (const auto& line : input)
        EXPECT_CALL(*mock_file_ops, read_line).WillOnce(Return(line)).RetiresOnSaturation();
    EXPECT_CALL(*mock_file_ops, read_line).WillOnce(Return(QString{}));

    auto output = multipass::platform::detail::read_os_release();

    EXPECT_EQ(expected, output);
}

TEST_F(PlatformLinux, hostVersionFromOs)
{
    const std::string expected =
        fmt::format("{}-{}", QSysInfo::productType(), QSysInfo::productVersion());

    auto output = multipass::platform::host_version();

    EXPECT_EQ(expected, output);
}

TEST_F(PlatformLinux, createAliasScriptWorksUnconfined)
{
    const mpt::TempDir tmp_dir;

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::AppLocalDataLocation))
        .WillOnce(Return(tmp_dir.path()));

    EXPECT_NO_THROW(
        MP_PLATFORM.create_alias_script("alias_name",
                                        mp::AliasDefinition{"instance", "command", "map"}));

    QFile checked_script(tmp_dir.path() + "/bin/alias_name");
    ASSERT_TRUE(checked_script.open(QFile::ReadOnly));

    EXPECT_EQ(checked_script.readLine().toStdString(), "#!/bin/sh\n");
    EXPECT_EQ(checked_script.readLine().toStdString(), "\n");
    EXPECT_THAT(checked_script.readLine().toStdString(), HasSubstr("alias_name -- \"${@}\"\n"));
    EXPECT_TRUE(checked_script.atEnd());

    auto script_permissions = checked_script.permissions();
    EXPECT_TRUE(script_permissions & QFileDevice::ExeOwner);
    EXPECT_TRUE(script_permissions & QFileDevice::ExeGroup);
    EXPECT_TRUE(script_permissions & QFileDevice::ExeOther);
}

TEST_F(PlatformLinux, createAliasScriptWorksConfined)
{
    const mpt::TempDir tmp_dir;

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::AppLocalDataLocation))
        .Times(0);

    qputenv("SNAP_NAME", QByteArray{"multipass"});
    qputenv("SNAP_USER_COMMON", tmp_dir.path().toUtf8());
    EXPECT_NO_THROW(
        MP_PLATFORM.create_alias_script("alias_name",
                                        mp::AliasDefinition{"instance", "command", "map"}));

    QFile checked_script(tmp_dir.path() + "/bin/alias_name");
    ASSERT_TRUE(checked_script.open(QFile::ReadOnly));

    EXPECT_EQ(checked_script.readLine().toStdString(), "#!/bin/sh\n");
    EXPECT_EQ(checked_script.readLine().toStdString(), "\n");
    EXPECT_EQ(checked_script.readLine().toStdString(),
              "exec /usr/bin/snap run multipass alias_name -- \"${@}\"\n");
    EXPECT_TRUE(checked_script.atEnd());

    auto script_permissions = checked_script.permissions();
    EXPECT_TRUE(script_permissions & QFileDevice::ExeOwner);
    EXPECT_TRUE(script_permissions & QFileDevice::ExeGroup);
    EXPECT_TRUE(script_permissions & QFileDevice::ExeOther);

    qunsetenv("SNAP_NAME");
    qunsetenv("SNAP_USER_COMMON");
}

TEST_F(PlatformLinux, createAliasScriptOverwrites)
{
    auto [mock_utils, guard1] = mpt::MockUtils::inject();
    auto [mock_file_ops, guard2] = mpt::MockFileOps::inject();
    auto [mock_platform, guard3] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_utils, make_file_with_content(_, _, true)).Times(1);
    EXPECT_CALL(*mock_file_ops, get_permissions(_))
        .WillOnce(Return(mp::fs::perms::owner_read | mp::fs::perms::owner_write));
    EXPECT_CALL(*mock_platform, set_permissions(_, _, _)).WillOnce(Return(true));

    // Calls the platform function directly since MP_PLATFORM is mocked.
    EXPECT_NO_THROW(MP_PLATFORM.Platform::create_alias_script(
        "alias_name",
        mp::AliasDefinition{"instance", "other_command", "map"}));
}

TEST_F(PlatformLinux, createAliasScriptThrowsIfCannotCreatePath)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.create_alias_script("alias_name",
                                        mp::AliasDefinition{"instance", "command", "map"}),
        std::runtime_error,
        mpt::match_what(HasSubstr("failed to create dir '")));
}

TEST_F(PlatformLinux, createAliasScriptThrowsIfCannotWriteScript)
{
    auto [mock_file_ops, guard] = mpt::MockFileOps::inject();

    EXPECT_CALL(*mock_file_ops, mkpath(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, open(_, _)).WillOnce(Return(true));
    EXPECT_CALL(*mock_file_ops, write(A<QFile&>(), _, _)).WillOnce(Return(747));

    MP_EXPECT_THROW_THAT(
        MP_PLATFORM.create_alias_script("alias_name",
                                        mp::AliasDefinition{"instance", "command", "map"}),
        std::runtime_error,
        mpt::match_what(HasSubstr("failed to write to file '")));
}

TEST_F(PlatformLinux, createAliasScriptThrowsIfCannotSetPermissions)
{
    auto [mock_utils, guard1] = mpt::MockUtils::inject();
    auto [mock_file_ops, guard2] = mpt::MockFileOps::inject();
    auto [mock_platform, guard3] = mpt::MockPlatform::inject();

    EXPECT_CALL(*mock_utils, make_file_with_content(_, _, true)).Times(1);
    EXPECT_CALL(*mock_file_ops, get_permissions(_))
        .WillOnce(Return(mp::fs::perms::owner_read | mp::fs::perms::owner_write));
    EXPECT_CALL(*mock_platform, set_permissions(_, _, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_PLATFORM.Platform::create_alias_script(
                             "alias_name",
                             mp::AliasDefinition{"instance", "command", "map"}),
                         std::runtime_error,
                         mpt::match_what(HasSubstr("cannot set permissions to alias script '")));
}

TEST_F(PlatformLinux, removeAliasScriptWorks)
{
    const mpt::TempDir tmp_dir;
    QFile script_file(tmp_dir.path() + "/bin/alias_name");

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::AppLocalDataLocation))
        .WillOnce(Return(tmp_dir.path()));

    MP_UTILS.make_file_with_content(script_file.fileName().toStdString(), "script content\n");

    EXPECT_NO_THROW(MP_PLATFORM.remove_alias_script("alias_name"));

    EXPECT_FALSE(script_file.exists());
}

TEST_F(PlatformLinux, removeAliasScriptThrowsIfCannotRemoveScript)
{
    const mpt::TempDir tmp_dir;
    QFile script_file(tmp_dir.path() + "/bin/alias_name");

    EXPECT_CALL(mpt::MockStandardPaths::mock_instance(),
                writableLocation(mp::StandardPaths::AppLocalDataLocation))
        .WillOnce(Return(tmp_dir.path()));

    MP_EXPECT_THROW_THAT(MP_PLATFORM.remove_alias_script("alias_name"),
                         std::runtime_error,
                         mpt::match_what(StrEq("No such file or directory")));
}

TEST_F(PlatformLinux, testSnapMultipassCertLocation)
{
    const auto unconfined_location = MP_PLATFORM.get_root_cert_path();

    mpt::SetEnvScope env{"SNAP_NAME", "multipass"};
    mpt::SetEnvScope env2("SNAP_COMMON", "common");

    const auto snap_location = MP_PLATFORM.get_root_cert_path();

    EXPECT_NE(snap_location, unconfined_location);
    EXPECT_EQ(snap_location.filename(), unconfined_location.filename());
}
} // namespace
