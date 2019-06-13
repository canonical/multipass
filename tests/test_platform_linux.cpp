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
#include "mock_libvirt.h"
#include "mock_settings.h"

#include "src/platform/backends/libvirt/libvirt_virtual_machine_factory.h"
#include "src/platform/backends/qemu/qemu_virtual_machine_factory.h"

#include <multipass/constants.h>
#include <multipass/optional.h>
#include <multipass/platform.h>

#include <QProcessEnvironment>
#include <QString>
#include <QtGlobal>

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

struct PlatformLinux : public Test
{
    void SetUp() override
    {
        if (QProcessEnvironment::systemEnvironment().contains(mp::driver_env_var))
        {
            env_driver_backup = qgetenv(mp::driver_env_var);
            qunsetenv(mp::driver_env_var);
        }
    }

    void TearDown() override
    {
        if (env_driver_backup)
            qputenv(mp::driver_env_var, *env_driver_backup);
        else
            qunsetenv(mp::driver_env_var);
    }

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

        std::string bridge_name{"where's that confounded bridge?"};
        REPLACE(virNetworkGetBridgeName, [&bridge_name](auto...) {
            return &bridge_name.front(); // hackish... replace with bridge_name.data() in C++17
        });

        test_contents();
    }

    mp::optional<QByteArray> env_driver_backup = mp::nullopt;
};

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

TEST_F(PlatformLinux, test_qemu_in_env_var_takes_precedence_over_settings)
{
    qputenv(mp::driver_env_var, "QEMU");
    aux_test_driver_factory<mp::QemuVirtualMachineFactory>("libvirt");
}

TEST_F(PlatformLinux, test_libvirt_in_env_var_takes_precedence_over_settings)
{
    qputenv(mp::driver_env_var, "LIBVIRT");
    auto test = [this] { aux_test_driver_factory<mp::LibVirtVirtualMachineFactory>("qemu"); };
    with_minimally_mocked_libvirt(test);
}

struct TestUnsupportedDrivers : public TestWithParam<QString>
{
};

TEST_P(TestUnsupportedDrivers, test_unsupported_driver)
{
    setup_driver_settings(GetParam());
    EXPECT_THROW(mp::platform::vm_backend(backend_path), std::runtime_error);
}

INSTANTIATE_TEST_SUITE_P(PlatformLinux, TestUnsupportedDrivers,
                         Values(QStringLiteral("hyperkit"), QStringLiteral("hyper-v"), QStringLiteral("other")));
} // namespace
