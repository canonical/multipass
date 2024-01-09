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

#include "common.h"
#include "mock_snapshot.h"
#include "mock_virtual_machine.h"
#include "path.h"

#include <multipass/virtual_machine_description.h>
#include <multipass/vm_specs.h>
#include <src/platform/backends/qemu/qemu_snapshot.h>

#include <QJsonArray>
#include <QJsonObject>

#include <memory>
#include <unordered_map>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{

struct PublicQemuSnapshot : public mp::QemuSnapshot
{
    // clang-format off
    // (keeping original declaration order)
    using mp::QemuSnapshot::QemuSnapshot;
    using mp::QemuSnapshot::capture_impl;
    using mp::QemuSnapshot::erase_impl;
    using mp::QemuSnapshot::apply_impl;
    // clang-format on
};

struct TestQemuSnapshot : public Test
{
    mp::VirtualMachineDescription desc{};
    NiceMock<mpt::MockVirtualMachineT<mp::QemuVirtualMachine>> vm{"qemu-vm"};
};

TEST_F(TestQemuSnapshot, initializesBaseProperties)
{
    const auto name = "name";
    const auto comment = "comment";
    const auto parent = std::make_shared<mpt::MockSnapshot>();

    const auto cpus = 3;
    const auto mem_size = mp::MemorySize{"1.23G"};
    const auto disk_space = mp::MemorySize{"3.21M"};
    const auto state = mp::VirtualMachine::State::off;
    const auto mounts =
        std::unordered_map<std::string, mp::VMMount>{{"asdf", {"fdsa", {}, {}, mp::VMMount::MountType::Classic}}};
    const auto metadata = [] {
        auto metadata = QJsonObject{};
        metadata["meta"] = "data";
        return metadata;
    }();
    const auto specs = mp::VMSpecs{cpus, mem_size, disk_space, "mac", {}, "", state, mounts, false, metadata, {}};

    auto desc = mp::VirtualMachineDescription{};
    auto vm = NiceMock<mpt::MockVirtualMachineT<mp::QemuVirtualMachine>>{"qemu-vm"};

    const auto snapshot = mp::QemuSnapshot{name, comment, parent, specs, vm, desc};
    EXPECT_EQ(snapshot.get_name(), name);
    EXPECT_EQ(snapshot.get_comment(), comment);
    EXPECT_EQ(snapshot.get_parent(), parent);
    EXPECT_EQ(snapshot.get_num_cores(), cpus);
    EXPECT_EQ(snapshot.get_mem_size(), mem_size);
    EXPECT_EQ(snapshot.get_disk_space(), disk_space);
    EXPECT_EQ(snapshot.get_state(), state);
    EXPECT_EQ(snapshot.get_mounts(), mounts);
    EXPECT_EQ(snapshot.get_metadata(), metadata);
}

TEST_F(TestQemuSnapshot, initializesBasePropertiesFromJson)
{
    const auto parent = std::make_shared<mpt::MockSnapshot>();
    EXPECT_CALL(vm, get_snapshot(2)).WillOnce(Return(parent));

    const mp::QemuSnapshot snapshot{mpt::test_data_path_for("test_snapshot.json"), vm, desc};
    EXPECT_EQ(snapshot.get_name(), "snapshot3");
    EXPECT_EQ(snapshot.get_comment(), "A comment");
    EXPECT_EQ(snapshot.get_parent(), parent);
    EXPECT_EQ(snapshot.get_num_cores(), 1);
    EXPECT_EQ(snapshot.get_mem_size(), mp::MemorySize{"1G"});
    EXPECT_EQ(snapshot.get_disk_space(), mp::MemorySize{"5G"});
    EXPECT_EQ(snapshot.get_state(), mp::VirtualMachine::State::off);

    auto mount_matcher1 = Pair(Eq("guybrush"), Field(&mp::VMMount::mount_type, mp::VMMount::MountType::Classic));
    auto mount_matcher2 = Pair(Eq("murray"), Field(&mp::VMMount::mount_type, mp::VMMount::MountType::Native));
    EXPECT_THAT(snapshot.get_mounts(), UnorderedElementsAre(mount_matcher1, mount_matcher2));

    EXPECT_THAT(
        snapshot.get_metadata(),
        ResultOf([](const QJsonObject& metadata) { return metadata["arguments"].toArray(); }, Contains("-qmp")));
}

} // namespace
