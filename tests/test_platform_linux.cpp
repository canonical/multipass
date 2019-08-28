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
    QByteArray test_dir_path = "/tmp";
    QString expected_filename = "multipass-gui.conditional-autostart.desktop";
    QString expected_filepath = QDir{test_dir_path}.filePath(expected_filename);

    auto cleanup = sg::make_scope_guard([expected_filepath, config_home_save = qgetenv("XDG_CONFIG_HOME")]() {
        QFile::remove(expected_filepath);
        qputenv("XDG_CONFIG_HOME", config_home_save);
    });

    QFile::remove(expected_filepath);
    qputenv("XDG_CONFIG_HOME", test_dir_path);

    mp::platform::preliminary_gui_autostart_setup();

    QFile f{expected_filepath};
    ASSERT_TRUE(f.exists());
    ASSERT_TRUE(f.open(QIODevice::ReadOnly | QIODevice::Text));

    auto contents = QString{f.readAll()};
    EXPECT_TRUE(contents.contains("Exec=multipass.gui --autostarting"));
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
