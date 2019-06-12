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

#include "mock_libvirt.h"
#include "mock_settings.h"

#include "src/platform/backends/libvirt/libvirt_virtual_machine_factory.h"
#include "src/platform/backends/qemu/qemu_virtual_machine_factory.h"

#include <multipass/constants.h>
#include <multipass/platform.h>

#include <QString>

#include <gtest/gtest.h>

#include <stdexcept>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
const auto backend_path = QStringLiteral("/tmp");

template <typename T>
auto fake_handle()
{
    return reinterpret_cast<T>(0xDEADBEEF);
}

void setup_driver_settings(const QString& driver)
{
    auto& expectation = EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(mp::driver_key)));
    if (!driver.isEmpty())
        expectation.WillRepeatedly(Return(driver));
}

template <typename VMFactoryType>
void aux_test_driver_factory(const QString& driver = QStringLiteral(""))
{
    setup_driver_settings(driver);

    decltype(mp::platform::vm_backend("")) factory_ptr;
    EXPECT_NO_THROW(factory_ptr = mp::platform::vm_backend(backend_path););

    EXPECT_TRUE(dynamic_cast<VMFactoryType*>(factory_ptr.get()));
}

TEST(PlatformLinux, test_default_qemu_driver_produces_correct_factory)
{
    aux_test_driver_factory<mp::QemuVirtualMachineFactory>();
}

TEST(PlatformLinux, test_explicit_qemu_driver_produces_correct_factory)
{
    aux_test_driver_factory<mp::QemuVirtualMachineFactory>("qemu");
}

TEST(PlatformLinux, test_libvirt_driver_produces_correct_factory)
{
    REPLACE(virConnectOpen, [](auto...) { return fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });
    REPLACE(virNetworkFree, [](auto...) { return 0; });
    REPLACE(virConnectClose, [](auto...) { return 0; });

    std::string bridge_name{"where's that confounded bridge?"};
    REPLACE(virNetworkGetBridgeName, [&bridge_name](auto...) {
        return &bridge_name.front(); // hackish... replace with bridge_name.data() in C++17
    });

    aux_test_driver_factory<mp::LibVirtVirtualMachineFactory>("libvirt");
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
