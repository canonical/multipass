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

#include "tests/fake_handle.h"
#include "tests/mock_environment_helpers.h"
#include "tests/mock_settings.h"
#include "tests/temp_dir.h"
#include "tests/test_with_mocked_bin_path.h"

#include <src/platform/backends/libvirt/libvirt_virtual_machine_factory.h>
#include <src/platform/backends/libvirt/libvirt_wrapper.h>
#include <src/platform/backends/lxd/lxd_virtual_machine_factory.h>
#include <src/platform/backends/qemu/qemu_virtual_machine_factory.h>
#include <src/platform/platform_linux_detail.h>

#include <multipass/constants.h>
#include <multipass/exceptions/autostart_setup_exception.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/platform.h>

#include <QDir>
#include <QFile>
#include <QString>

#include <scope_guard.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
const auto backend_path = QStringLiteral("/tmp");

void setup_driver_settings(const QString& driver)
{
    auto& expectation = EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(mp::driver_key)));
    if (!driver.isEmpty())
        expectation.WillRepeatedly(Return(driver));
}

// hold on to return until the change is to be discarded
auto temporarily_change_env(const char* var_name, QByteArray var_value)
{
    auto guard = sg::make_scope_guard([var_name, var_save = qgetenv(var_name)]() { qputenv(var_name, var_save); });
    qputenv(var_name, var_value);

    return guard;
}

template <typename Guards>
struct autostart_test_record
{
    autostart_test_record(const QDir& autostart_dir, const QString& autostart_filename,
                          const QString& autostart_contents, Guards&& guards)
        : autostart_dir{autostart_dir},
          autostart_filename{autostart_filename},
          autostart_contents{autostart_contents},
          guards{std::forward<Guards>(guards)} // this should always move, since scope guards are not copyable
    {
    }

    QDir autostart_dir;
    QString autostart_filename;
    QString autostart_contents;
    Guards guards;
};

struct autostart_test_setup_error : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};

auto setup_autostart_desktop_file_test()
{
    QDir test_dir{QDir::temp().filePath(QString{"%1_%2"}.arg(mp::client_name, "autostart_test"))};
    if (test_dir.exists() || QFile{test_dir.path()}.exists()) // avoid touching this at all if it already exists
        throw autostart_test_setup_error{fmt::format("Test dir or file already exists: {}", test_dir.path())}; /*
                           use exception to abort the test, since ASSERTS only work in void-returning functions */

    // Now mock filesystem tree and environment, reverting when done

    auto guard_fs = sg::make_scope_guard([test_dir]() mutable {
        test_dir.removeRecursively(); // succeeds if not there
    });

    QDir data_dir{test_dir.filePath("data")};
    QDir config_dir{test_dir.filePath("config")};
    auto guard_home = temporarily_change_env("HOME", "hide/me");
    auto guard_xdg_config = temporarily_change_env("XDG_CONFIG_HOME", config_dir.path().toLatin1());
    auto guard_xdg_data = temporarily_change_env("XDG_DATA_DIRS", data_dir.path().toLatin1());

    QDir mp_data_dir{data_dir.filePath(mp::client_name)};
    QDir autostart_dir{config_dir.filePath("autostart")};
    EXPECT_TRUE(mp_data_dir.mkpath("."));   // this is where the directories are actually created
    EXPECT_TRUE(autostart_dir.mkpath(".")); // idem

    const auto autostart_filename = mp::platform::autostart_test_data();
    const auto autostart_filepath = mp_data_dir.filePath(autostart_filename);
    const auto autostart_contents = "Exec=multipass.gui --autostarting\n";

    {
        QFile autostart_file{autostart_filepath};
        EXPECT_TRUE(autostart_file.open(QIODevice::WriteOnly)); // create desktop file to link against
        EXPECT_EQ(autostart_file.write(autostart_contents), qstrlen(autostart_contents));
    }

    auto guards = std::make_tuple(std::move(guard_home), std::move(guard_xdg_config), std::move(guard_xdg_data),
                                  std::move(guard_fs));
    return autostart_test_record{autostart_dir, autostart_filename, autostart_contents, std::move(guards)};
}

void check_autostart_file(const QDir& autostart_dir, const QString& autostart_filename,
                          const QString& autostart_contents)
{
    QFile f{autostart_dir.filePath(autostart_filename)};
    ASSERT_TRUE(f.exists());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));

    auto actual_contents = QString{f.readAll()};
    EXPECT_EQ(actual_contents, autostart_contents);
}

struct PlatformLinux : public mpt::TestWithMockedBinPath
{
    template <typename VMFactoryType>
    void aux_test_driver_factory(const QString& driver = QStringLiteral(""))
    {
        setup_driver_settings(driver);

        decltype(mp::platform::vm_backend("")) factory_ptr;
        EXPECT_NO_THROW(factory_ptr = mp::platform::vm_backend(backend_path););

        EXPECT_TRUE(dynamic_cast<VMFactoryType*>(factory_ptr.get()));
    }

    void with_minimally_mocked_libvirt(std::function<void()> test_contents)
    {
        mp::LibvirtWrapper libvirt_wrapper{""};
        test_contents();
    }

    mpt::UnsetEnvScope unset_env_scope{mp::driver_env_var};
    mpt::SetEnvScope disable_apparmor{"DISABLE_APPARMOR", "1"};
};

TEST_F(PlatformLinux, test_interpretation_of_winterm_setting_not_supported)
{
    for (const auto x : {"no", "matter", "what"})
        EXPECT_THROW(mp::platform::interpret_setting(mp::winterm_key, x), mp::InvalidSettingsException);
}

TEST_F(PlatformLinux, test_interpretation_of_unknown_settings_not_supported)
{
    for (const auto k : {"unimaginable", "katxama", "katxatxa"})
        for (const auto v : {"no", "matter", "what"})
            EXPECT_THROW(mp::platform::interpret_setting(k, v), mp::InvalidSettingsException);
}

TEST_F(PlatformLinux, test_empty_sync_winterm_profiles)
{
    EXPECT_NO_THROW(mp::platform::sync_winterm_profiles());
}

TEST_F(PlatformLinux, test_autostart_desktop_file_properly_placed)
{
    try
    {
        const auto& [autostart_dir, autostart_filename, autostart_contents, guards] =
            setup_autostart_desktop_file_test();

        ASSERT_FALSE(HasFailure()) << "autostart test setup failed";

        mp::platform::setup_gui_autostart_prerequisites();
        check_autostart_file(autostart_dir, autostart_filename, autostart_contents);
    }
    catch (autostart_test_setup_error& e)
    {
        FAIL() << e.what();
    }
}

TEST_F(PlatformLinux, test_autostart_setup_replaces_wrong_link)
{
    try
    {
        const auto& [autostart_dir, autostart_filename, autostart_contents, guards] =
            setup_autostart_desktop_file_test();

        ASSERT_FALSE(HasFailure()) << "autostart test setup failed";

        const auto bad_filename = autostart_dir.filePath("wrong_file");
        QFile bad_file{bad_filename};

        EXPECT_TRUE(bad_file.open(QIODevice::WriteOnly)); // create desktop file to link against
        bad_file.write("bad contents");
        bad_file.close();

        bad_file.link(autostart_dir.filePath(autostart_filename)); // link to a bad file

        mp::platform::setup_gui_autostart_prerequisites();
        check_autostart_file(autostart_dir, autostart_filename, autostart_contents);
    }
    catch (autostart_test_setup_error& e)
    {
        FAIL() << e.what();
    }
}

TEST_F(PlatformLinux, test_autostart_setup_replaces_broken_link)
{
    try
    {
        const auto& [autostart_dir, autostart_filename, autostart_contents, guards] =
            setup_autostart_desktop_file_test();

        ASSERT_FALSE(HasFailure()) << "autostart test setup failed";

        const auto bad_filename = autostart_dir.filePath("absent_file");
        QFile bad_file{bad_filename};
        ASSERT_FALSE(bad_file.exists());
        bad_file.link(autostart_dir.filePath(autostart_filename)); // link to absent file
        ASSERT_FALSE(bad_file.exists());

        mp::platform::setup_gui_autostart_prerequisites();
        check_autostart_file(autostart_dir, autostart_filename, autostart_contents);
    }
    catch (autostart_test_setup_error& e)
    {
        FAIL() << e.what();
    }
}

TEST_F(PlatformLinux, test_autostart_setup_leaves_non_link_file_alone)
{
    try
    {
        const auto& [autostart_dir, autostart_filename, autostart_contents, guards] =
            setup_autostart_desktop_file_test();

        ASSERT_FALSE(HasFailure()) << "autostart test setup failed";

        const auto replacement_contents = "replacement contents";

        {
            QFile replacement_file{autostart_dir.filePath(autostart_filename)};
            ASSERT_FALSE(replacement_file.exists());
            ASSERT_TRUE(replacement_file.open(QIODevice::WriteOnly)); // create desktop file to link against
            ASSERT_EQ(replacement_file.write(replacement_contents), qstrlen(replacement_contents));
        }

        mp::platform::setup_gui_autostart_prerequisites();
        check_autostart_file(autostart_dir, autostart_filename, replacement_contents);
    }
    catch (autostart_test_setup_error& e)
    {
        FAIL() << e.what();
    }
}

TEST_F(PlatformLinux, test_autostart_setup_fails_on_absent_desktop_target)
{
    const auto guard_xdg = temporarily_change_env("XDG_DATA_DIRS", "/dadgad/bad/dir");
    const auto guard_home = temporarily_change_env("HOME", "dadgbd/bad/too");

    EXPECT_THROW(mp::platform::setup_gui_autostart_prerequisites(), mp::AutostartSetupException);
}

TEST_F(PlatformLinux, test_default_qemu_driver_produces_correct_factory)
{
    aux_test_driver_factory<mp::QemuVirtualMachineFactory>();
}

TEST_F(PlatformLinux, test_explicit_qemu_driver_produces_correct_factory)
{
    aux_test_driver_factory<mp::QemuVirtualMachineFactory>("qemu");
}

TEST_F(PlatformLinux, test_libvirt_driver_produces_correct_factory)
{
    auto test = [this] { aux_test_driver_factory<mp::LibVirtVirtualMachineFactory>("libvirt"); };
    with_minimally_mocked_libvirt(test);
}

TEST_F(PlatformLinux, test_lxd_driver_produces_correct_factory)
{
    aux_test_driver_factory<mp::LXDVirtualMachineFactory>("lxd");
}

TEST_F(PlatformLinux, test_qemu_in_env_var_is_ignored)
{
    mpt::SetEnvScope env(mp::driver_env_var, "QEMU");
    auto test = [this] { aux_test_driver_factory<mp::LibVirtVirtualMachineFactory>("libvirt"); };
    with_minimally_mocked_libvirt(test);
}

TEST_F(PlatformLinux, test_libvirt_in_env_var_is_ignored)
{
    mpt::SetEnvScope env(mp::driver_env_var, "LIBVIRT");
    aux_test_driver_factory<mp::QemuVirtualMachineFactory>("qemu");
}

TEST_F(PlatformLinux, test_is_remote_supported_returns_true)
{
    EXPECT_TRUE(mp::platform::is_remote_supported("release"));
    EXPECT_TRUE(mp::platform::is_remote_supported("daily"));
    EXPECT_TRUE(mp::platform::is_remote_supported(""));
    EXPECT_TRUE(mp::platform::is_remote_supported("snapcraft"));
    EXPECT_TRUE(mp::platform::is_remote_supported("appliance"));
}

TEST_F(PlatformLinux, test_is_remote_supported_lxd)
{
    setup_driver_settings("lxd");

    EXPECT_TRUE(mp::platform::is_remote_supported("release"));
    EXPECT_TRUE(mp::platform::is_remote_supported("daily"));
    EXPECT_TRUE(mp::platform::is_remote_supported(""));
    EXPECT_TRUE(mp::platform::is_remote_supported("appliance"));
    EXPECT_FALSE(mp::platform::is_remote_supported("snapcraft"));
}

TEST_F(PlatformLinux, test_snap_returns_expected_default_address)
{
    const QByteArray base_dir{"/tmp"};
    const QByteArray snap_name{"multipass"};

    mpt::SetEnvScope env("SNAP_COMMON", base_dir);
    mpt::SetEnvScope env2("SNAP_NAME", snap_name);

    EXPECT_EQ(mp::platform::default_server_address(), fmt::format("unix:{}/multipass_socket", base_dir.toStdString()));
}

TEST_F(PlatformLinux, test_not_snap_returns_expected_default_address)
{
    const QByteArray snap_name{"multipass"};

    mpt::UnsetEnvScope unset_env("SNAP_COMMON");
    mpt::SetEnvScope env2("SNAP_NAME", snap_name);

    EXPECT_EQ(mp::platform::default_server_address(), fmt::format("unix:/run/multipass_socket"));
}

TEST_F(PlatformLinux, test_is_alias_supported_returns_true)
{
    EXPECT_TRUE(mp::platform::is_alias_supported("focal", "release"));
}

struct TestUnsupportedDrivers : public TestWithParam<QString>
{
};

TEST_P(TestUnsupportedDrivers, test_unsupported_driver)
{
    ASSERT_FALSE(mp::platform::is_backend_supported(GetParam()));

    setup_driver_settings(GetParam());
    EXPECT_THROW(mp::platform::vm_backend(backend_path), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(PlatformLinux, TestUnsupportedDrivers,
                         Values(QStringLiteral("hyperkit"), QStringLiteral("hyper-v"), QStringLiteral("other")));

TEST_F(PlatformLinux, retrieves_networks_from_the_system)
{
    const mpt::TempDir tmp_dir;
    const auto fake_nets = {"eth0", "foo", "kkkkk"};

    QDir fake_sys_class_net{tmp_dir.path()};
    for (const auto& net : fake_nets)
        ASSERT_TRUE(fake_sys_class_net.mkpath(net));

    auto net_map = mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path());
    ASSERT_EQ(net_map.size(), fake_nets.size());

    auto expect_it = std::cbegin(fake_nets);
    for (const auto& [key, val] : net_map)
    {
        ASSERT_NE(expect_it, std::cend(fake_nets));
        EXPECT_EQ(key, *expect_it++);
        EXPECT_EQ(val.id, key);
    }
}

TEST_F(PlatformLinux, retrieves_empty_bridges)
{
    const mpt::TempDir tmp_dir;
    const auto fake_bridge = "somebridge";

    QDir fake_sys_class_net{tmp_dir.path()};
    ASSERT_TRUE(fake_sys_class_net.mkpath(QString{fake_bridge} + "/bridge"));

    auto net_map = mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path());

    using value_type = decltype(net_map)::value_type;
    using Net = mp::NetworkInterfaceInfo;
    EXPECT_THAT(net_map, ElementsAre(AllOf(Field(&value_type::first, fake_bridge),
                                           Field(&value_type::second,
                                                 AllOf(Field(&Net::id, fake_bridge), Field(&Net::type, "bridge"),
                                                       Field(&Net::description, HasSubstr("Empty network bridge")))))));
}

struct BridgeMemberTest : public PlatformLinux, WithParamInterface<std::vector<std::string>>
{
};

TEST_P(BridgeMemberTest, retrieves_bridges_with_members)
{
    const mpt::TempDir tmp_dir;
    const auto fake_bridge = "aeiou";

    QDir fake_sys_class_net{tmp_dir.path()};
    QDir interface_dir{fake_sys_class_net.filePath(fake_bridge)};
    QDir members_dir{interface_dir.filePath("brif")};

    ASSERT_TRUE(interface_dir.mkpath("bridge"));
    ASSERT_TRUE(members_dir.mkpath("."));

    std::vector<Matcher<std::string>> substrs_expectations{};
    for (const auto& member : GetParam())
    {
        ASSERT_TRUE(members_dir.mkpath(QString::fromStdString(member)));
        substrs_expectations.push_back(HasSubstr(member));
    }

    auto net_map = mp::platform::detail::get_network_interfaces_from(fake_sys_class_net.path());

    using value_type = decltype(net_map)::value_type;
    using Net = mp::NetworkInterfaceInfo;

    EXPECT_THAT(net_map, ElementsAre(AllOf(Field(&value_type::first, fake_bridge),
                                           Field(&value_type::second,
                                                 AllOf(Field(&Net::id, fake_bridge), Field(&Net::type, "bridge"),
                                                       Field(&Net::description, AllOfArray(substrs_expectations)))))));
}

INSTANTIATE_TEST_SUITE_P(PlatformLinux, BridgeMemberTest,
                         Values(std::vector<std::string>{"en0"}, std::vector<std::string>{"en0, en1"},
                                std::vector<std::string>{"asdf", "ggi", "a1", "fu"}));

} // namespace
