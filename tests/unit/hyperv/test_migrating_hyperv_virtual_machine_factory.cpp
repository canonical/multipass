/*
 * Copyright (C) Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tests/unit/common.h"
#include "tests/unit/file_operations.h"
#include "tests/unit/hyperv_api/mock_hyperv_virtdisk_wrapper.h"
#include "tests/unit/stub_availability_zone_manager.h"
#include "tests/unit/stub_ssh_key_provider.h"
#include "tests/unit/stub_status_monitor.h"
#include "tests/unit/temp_dir.h"
#include "tests/unit/windows/powershell_test_helper.h"

#include <src/platform/backends/hyperv/hcs_ownership.h>
#include <src/platform/backends/hyperv/migrating_hyperv_virtual_machine_factory.h>

#include <multipass/constants.h>
#include <multipass/virtual_machine_description.h>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mhv = multipass::hyperv;
using namespace testing;

namespace
{
struct MigratingHyperVFactoryTest : Test
{
    mpt::TempDir data_dir;
    mpt::StubAvailabilityZoneManager zone_manager;
    mpt::StubSSHKeyProvider key_provider;
    mpt::StubVMStatusMonitor monitor;
    mpt::MockVirtDiskWrapper::GuardedMock virtdisk_injection =
        mpt::MockVirtDiskWrapper::inject<StrictMock>();
    mpt::MockVirtDiskWrapper& virtdisk = *virtdisk_injection.first;
    mhv::MigratingHyperVVirtualMachineFactory factory{data_dir.path(), zone_manager};

    mp::VirtualMachineDescription description()
    {
        const QDir instance_dir{factory.get_instance_directory("migration-test")};
        std::filesystem::create_directories(instance_dir.path().toStdWString());
        const auto image = instance_dir.filePath("base.vhdx");
        mpt::make_file_with_content(image, "disk");

        return {2,
                mp::MemorySize{"1G"},
                mp::MemorySize{"5G"},
                "migration-test",
                "zone1",
                "52:54:00:12:34:56",
                {},
                "ubuntu",
                {image.toStdString(), "", "", "", "", {}},
                instance_dir.filePath(mp::cloud_init_file_name)};
    }
};
} // namespace

TEST_F(MigratingHyperVFactoryTest, persistsHcsOwnershipWhenPreparingANewInstance)
{
    auto desc = description();
    EXPECT_CALL(virtdisk, resize_virtual_disk(desc.image.image_path, desc.disk_space.in_bytes()))
        .WillOnce(Return(mhv::OperationResult{0, L""}));

    factory.prepare_instance_image(desc.image, desc);

    const QDir instance_dir{factory.get_instance_directory(desc.vm_name)};
    const auto state = mhv::HCSOwnership::load(
        std::filesystem::path{instance_dir.path().toStdString()});
    ASSERT_TRUE(state);
    EXPECT_EQ(state->active_disk, desc.image.image_path);
    EXPECT_EQ(state->state_file_stem, desc.image.image_path);
}

TEST_F(MigratingHyperVFactoryTest, blocksMalformedMigrationMetadata)
{
    auto desc = description();
    const QDir instance_dir{factory.get_instance_directory(desc.vm_name)};
    mpt::make_file_with_content(instance_dir.filePath("hcs-ownership.json"), "{not-json");

    auto vm = factory.create_virtual_machine(desc, key_provider, monitor);

    EXPECT_EQ(vm->current_state(), mp::VirtualMachine::State::unavailable);
    EXPECT_THROW(vm->start(), std::runtime_error);
}

TEST_F(MigratingHyperVFactoryTest, blocksAmbiguousOwnership)
{
    mpt::PowerShellTestHelper powershell;
    powershell.setup_mocked_run_sequence({{"if (Get-VM", "false"}});
    auto desc = description();

    auto vm = factory.create_virtual_machine(desc, key_provider, monitor);

    EXPECT_EQ(vm->current_state(), mp::VirtualMachine::State::unavailable);
    EXPECT_THROW(vm->start(), std::runtime_error);
}
