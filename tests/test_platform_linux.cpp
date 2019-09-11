/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "fake_handle.h"
#include "mock_environment_helpers.h"
#include "mock_libvirt.h"
#include "mock_settings.h"
#include "test_with_mocked_bin_path.h"

#include "src/platform/backends/libvirt/libvirt_virtual_machine_factory.h"
#include "src/platform/backends/qemu/qemu_virtual_machine_factory.h"

#include <multipass/constants.h>
#include <multipass/platform.h>

#include <QDir>
#include <QFile>
#include <QString>

#include <scope_guard.hpp>

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

auto temporarily_change_env(const char* var_name, QByteArray var_value)
{
    auto guard = sg::make_scope_guard([var_name, var_save = qgetenv(var_name)]() { qputenv(var_name, var_save); });
    qputenv(var_name, var_value);

    return guard;
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
        REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
        REPLACE(virNetworkLookupByName, [](auto...) { return mpt::fake_handle<virNetworkPtr>(); });
        REPLACE(virNetworkIsActive, [](auto...) { return 1; });
        REPLACE(virNetworkFree, [](auto...) { return 0; });
        REPLACE(virConnectClose, [](auto...) { return 0; });
        REPLACE(virNetworkGetBridgeName, [](auto...) { return strdup("where's that confounded bridge?"); });

        test_contents();
    }

    mpt::UnsetEnvScope unset_env_scope{mp::driver_env_var};
    mpt::SetEnvScope disable_apparmor{"DISABLE_APPARMOR", "1"};
};

TEST_F(PlatformLinux, test_autostart_desktop_file_properly_placed)
{
    // Test setup
    // Mock filesystem tree and environment

    QDir test_dir{QDir::temp().filePath(QString{"%1_%2"}.arg(mp::client_name, "autostart_test"))};
    ASSERT_FALSE(test_dir.exists());

    QDir data_dir{test_dir.filePath("data")};
    QDir config_dir{test_dir.filePath("config")};
    const auto guard_xdg_config = temporarily_change_env("XDG_CONFIG_HOME", config_dir.path().toLatin1());
    const auto guard_xdg_data = temporarily_change_env("XDG_DATA_DIRS", data_dir.path().toLatin1());

    const auto cleanup = sg::make_scope_guard([&test_dir]() {
        test_dir.removeRecursively(); // succeeds if not there
    });

    QDir mp_data_dir{data_dir.filePath(mp::client_name)};
    QDir autostart_dir{config_dir.filePath("autostart")};
    ASSERT_TRUE(mp_data_dir.mkpath("."));
    ASSERT_TRUE(autostart_dir.mkpath("."));

    const auto desktop_filename = mp::platform::autostart_test_data();
    const auto desktop_filepath = mp_data_dir.filePath(desktop_filename);
    const auto autostart_contents = "Exec=multipass.gui --autostarting\n";

    {
        QFile desktop_file{desktop_filepath};
        ASSERT_TRUE(desktop_file.open(QIODevice::WriteOnly)); // create desktop file to link against
        EXPECT_EQ(desktop_file.write(autostart_contents), qstrlen(autostart_contents));
    }

    // Test setup done
    // Actual tests follows

    mp::platform::setup_gui_autostart_prerequisites();
    const auto expected_autostart_path = autostart_dir.filePath(desktop_filename);

    QFile f{expected_autostart_path};
    ASSERT_TRUE(f.exists());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));

    auto actual_contents = QString{f.readAll()};
    EXPECT_EQ(actual_contents, autostart_contents);
}

TEST_F(PlatformLinux, test_autostart_setup_fails_on_absent_desktop_target)
{
    const auto guard_xdg = temporarily_change_env("XDG_DATA_DIRS", "/dadgad/bad/dir");
    const auto guard_home = temporarily_change_env("HOME", "dadgbd/bad/too");

    EXPECT_THROW(mp::platform::setup_gui_autostart_prerequisites(), std::runtime_error);
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
} // namespace
