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
#include "tests/unit/mock_virtual_machine.h"
#include "tests/unit/stub_availability_zone.h"

#include <src/platform/backends/hyperv/migrating_hyperv_virtual_machine.h>

#include <chrono>
#include <future>

namespace mpt = multipass::test;
namespace mhv = multipass::hyperv;
using namespace testing;

namespace
{
class MockHyperVMigrator : public mhv::HyperVMigrator
{
public:
    MOCK_METHOD(bool, migrate, (multipass::VirtualMachine& legacy_vm), (override));
    MOCK_METHOD(multipass::VirtualMachine::UPtr, make_target, (), (override));
};
} // namespace

TEST(MigratingHyperVVirtualMachine, keepsRunningLegacyVm)
{
    mpt::StubAvailabilityZone zone;
    auto legacy = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    auto* legacy_ptr = legacy.get();
    legacy->state = multipass::VirtualMachine::State::running;
    ON_CALL(*legacy, current_state())
        .WillByDefault(Return(multipass::VirtualMachine::State::running));
    ON_CALL(*legacy, get_zone()).WillByDefault(ReturnRef(zone));
    EXPECT_CALL(*legacy, start());

    auto migrator = std::make_unique<StrictMock<MockHyperVMigrator>>();
    auto vm = mhv::MigratingHyperVVirtualMachine{std::move(legacy), std::move(migrator)};

    vm.start();

    EXPECT_EQ(vm.state, legacy_ptr->state);
}

TEST(MigratingHyperVVirtualMachine, fallsBackToLegacyWhenMigrationIsSkipped)
{
    mpt::StubAvailabilityZone zone;
    auto legacy = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    auto* legacy_ptr = legacy.get();
    legacy->state = multipass::VirtualMachine::State::stopped;
    ON_CALL(*legacy, current_state())
        .WillByDefault(Return(multipass::VirtualMachine::State::stopped));
    ON_CALL(*legacy, get_zone()).WillByDefault(ReturnRef(zone));
    EXPECT_CALL(*legacy, start());

    auto migrator = std::make_unique<StrictMock<MockHyperVMigrator>>();
    EXPECT_CALL(*migrator, migrate(Ref(*legacy_ptr))).WillOnce(Return(false));

    auto vm = mhv::MigratingHyperVVirtualMachine{std::move(legacy), std::move(migrator)};
    vm.start();
}

TEST(MigratingHyperVVirtualMachine, switchesToHcsBeforeStarting)
{
    mpt::StubAvailabilityZone zone;
    auto legacy = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    auto* legacy_ptr = legacy.get();
    legacy->state = multipass::VirtualMachine::State::stopped;
    ON_CALL(*legacy, current_state())
        .WillByDefault(Return(multipass::VirtualMachine::State::stopped));
    ON_CALL(*legacy, get_zone()).WillByDefault(ReturnRef(zone));
    EXPECT_CALL(*legacy, start()).Times(0);

    auto target = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    auto* target_ptr = target.get();
    target->state = multipass::VirtualMachine::State::off;
    EXPECT_CALL(*target, start());

    auto migrator = std::make_unique<StrictMock<MockHyperVMigrator>>();
    EXPECT_CALL(*migrator, migrate(Ref(*legacy_ptr))).WillOnce(Return(true));
    EXPECT_CALL(*migrator, make_target()).WillOnce(Return(ByMove(std::move(target))));

    auto vm = mhv::MigratingHyperVVirtualMachine{std::move(legacy), std::move(migrator)};
    vm.start();

    EXPECT_EQ(vm.state, target_ptr->state);
}

TEST(MigratingHyperVVirtualMachine, retriesTargetConstructionAfterCommit)
{
    mpt::StubAvailabilityZone zone;
    auto legacy = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    auto* legacy_ptr = legacy.get();
    legacy->state = multipass::VirtualMachine::State::stopped;
    ON_CALL(*legacy, current_state())
        .WillByDefault(Return(multipass::VirtualMachine::State::stopped));
    ON_CALL(*legacy, get_zone()).WillByDefault(ReturnRef(zone));

    auto target = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    EXPECT_CALL(*target, start());

    auto migrator = std::make_unique<StrictMock<MockHyperVMigrator>>();
    EXPECT_CALL(*migrator, migrate(Ref(*legacy_ptr))).WillOnce(Return(true));
    EXPECT_CALL(*migrator, make_target())
        .WillOnce(Throw(std::runtime_error{"target construction failed"}))
        .WillOnce(Return(ByMove(std::move(target))));

    auto vm = mhv::MigratingHyperVVirtualMachine{std::move(legacy), std::move(migrator)};

    EXPECT_THROW(vm.start(), std::runtime_error);
    EXPECT_EQ(vm.current_state(), multipass::VirtualMachine::State::unknown);
    EXPECT_NO_THROW(vm.start());
}

TEST(MigratingHyperVVirtualMachine, keepsDelegateAliveForConcurrentCalls)
{
    using namespace std::chrono_literals;

    mpt::StubAvailabilityZone zone;
    auto legacy = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    auto* legacy_ptr = legacy.get();
    legacy->state = multipass::VirtualMachine::State::stopped;
    ON_CALL(*legacy, current_state())
        .WillByDefault(Return(multipass::VirtualMachine::State::stopped));
    ON_CALL(*legacy, get_zone()).WillByDefault(ReturnRef(zone));

    auto target = std::make_unique<NiceMock<mpt::MockVirtualMachine>>();
    target->state = multipass::VirtualMachine::State::off;
    ON_CALL(*target, current_state()).WillByDefault(Return(multipass::VirtualMachine::State::off));
    EXPECT_CALL(*target, start());

    std::promise<void> migration_entered;
    std::promise<void> release_migration;
    auto release = release_migration.get_future().share();
    auto migrator = std::make_unique<StrictMock<MockHyperVMigrator>>();
    EXPECT_CALL(*migrator, migrate(Ref(*legacy_ptr)))
        .WillOnce([&](auto&) {
            migration_entered.set_value();
            release.wait();
            return true;
        });
    EXPECT_CALL(*migrator, make_target()).WillOnce(Return(ByMove(std::move(target))));

    auto vm = mhv::MigratingHyperVVirtualMachine{std::move(legacy), std::move(migrator)};
    auto start = std::async(std::launch::async, [&] { vm.start(); });
    migration_entered.get_future().wait();
    auto state = std::async(std::launch::async, [&] { return vm.current_state(); });

    EXPECT_EQ(state.wait_for(50ms), std::future_status::ready);
    EXPECT_EQ(state.get(), multipass::VirtualMachine::State::stopped);
    release_migration.set_value();
    EXPECT_NO_THROW(start.get());
}
