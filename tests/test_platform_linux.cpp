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

#include "mock_settings.h"

#include "src/platform/backends/libvirt/libvirt_virtual_machine_factory.h"
#include "src/platform/backends/qemu/qemu_virtual_machine_factory.h"

#include <multipass/constants.h>
#include <multipass/platform.h>

#include <QString>

#include <gtest/gtest.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
void aux_test_qemu_driver_factory()
{
    decltype(mp::platform::vm_backend("")) factory_ptr;
    EXPECT_NO_THROW(factory_ptr = mp::platform::vm_backend(QStringLiteral("/tmp")););

    EXPECT_TRUE(dynamic_cast<mp::QemuVirtualMachineFactory*>(factory_ptr.get()));
}

TEST(PlatformLinux, test_default_qemu_driver_produces_qemu_factory)
{
    EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(mp::driver_key)));
    aux_test_qemu_driver_factory();
}

TEST(PlatformLinux, test_explicit_qemu_driver_produces_qemu_factory)
{
    EXPECT_CALL(mpt::MockSettings::mock_instance(), get(Eq(mp::driver_key))).WillRepeatedly(Return("qemu"));
    aux_test_qemu_driver_factory();
}
} // namespace
