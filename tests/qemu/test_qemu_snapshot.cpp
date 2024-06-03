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
#include "tests/mock_cloud_init_file_ops.h"
#include "tests/mock_logger.h"
#include "tests/mock_process_factory.h"
#include "tests/mock_snapshot.h"
#include "tests/mock_virtual_machine.h"
#include "tests/path.h"
#include "tests/stub_ssh_key_provider.h"

#include <multipass/process/process.h>
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
    using ArgsMatcher = Matcher<QStringList>;

    mp::QemuSnapshot quick_snapshot(const std::string& name = "asdf")
    {
        return mp::QemuSnapshot{name, "", "", nullptr, specs, vm, desc};
    }

    mp::QemuSnapshot loaded_snapshot()
    {
        return mp::QemuSnapshot{mpt::test_data_path_for("test_snapshot.json"), vm, desc};
    }

    template <typename T>
    static std::string derive_tag(T&& index)
    {
        return fmt::format("@s{}", std::forward<T>(index));
    }

    static void set_common_expectations_on(mpt::MockProcess* process)
    {
        EXPECT_EQ(process->program(), "qemu-img");
        EXPECT_CALL(*process, execute).WillOnce(Return(success));
    }

    static void set_tag_output(mpt::MockProcess* process, std::string tag)
    {
        EXPECT_CALL(*process, read_all_standard_output).WillOnce(Return(QByteArray::fromStdString(tag + ' ')));
    }

    mp::VirtualMachineDescription desc = [] {
        mp::VirtualMachineDescription ret{};
        ret.image.image_path = "raniunotuiroleh";
        return ret;
    }();

    mpt::StubSSHKeyProvider key_provider{};
    NiceMock<mpt::MockVirtualMachineT<mp::QemuVirtualMachine>> vm{"qemu-vm", key_provider};
    ArgsMatcher list_args_matcher = ElementsAre("snapshot", "-l", desc.image.image_path);
    const mpt::MockCloudInitFileOps::GuardedMock mock_cloud_init_file_ops_injection =
        mpt::MockCloudInitFileOps::inject<NiceMock>();

    inline static const auto success = mp::ProcessState{0, std::nullopt};
    inline static const auto failure = mp::ProcessState{1, std::nullopt};
    inline static const auto specs = [] {
        const auto cpus = 3;
        const auto mem_size = mp::MemorySize{"1.23G"};
        const auto disk_space = mp::MemorySize{"3.21M"};
        const std::vector<mp::NetworkInterface> extra_interfaces{{"eth15", "15:15:15:15:15:15", false}};
        const auto state = mp::VirtualMachine::State::off;
        const auto mounts =
            std::unordered_map<std::string, mp::VMMount>{{"asdf", {"fdsa", {}, {}, mp::VMMount::MountType::Classic}}};
        const auto metadata = [] {
            auto metadata = QJsonObject{};
            metadata["meta"] = "data";
            return metadata;
        }();

        return mp::VMSpecs{cpus, mem_size, disk_space, "mac", extra_interfaces, "", state, mounts, false, metadata};
    }();
};

TEST_F(TestQemuSnapshot, initializesBaseProperties)
{
    const auto name = "name";
    const auto comment = "comment";
    const auto instance_id = "vm2";

    const auto parent = std::make_shared<mpt::MockSnapshot>();

    auto desc = mp::VirtualMachineDescription{};
    auto vm = NiceMock<mpt::MockVirtualMachineT<mp::QemuVirtualMachine>>{"qemu-vm", key_provider};

    const auto snapshot = mp::QemuSnapshot{name, comment, instance_id, parent, specs, vm, desc};
    EXPECT_EQ(snapshot.get_name(), name);
    EXPECT_EQ(snapshot.get_comment(), comment);
    EXPECT_EQ(snapshot.get_parent(), parent);
    EXPECT_EQ(snapshot.get_num_cores(), specs.num_cores);
    EXPECT_EQ(snapshot.get_mem_size(), specs.mem_size);
    EXPECT_EQ(snapshot.get_disk_space(), specs.disk_space);
    EXPECT_EQ(snapshot.get_extra_interfaces(), specs.extra_interfaces);
    EXPECT_EQ(snapshot.get_state(), specs.state);
    EXPECT_EQ(snapshot.get_mounts(), specs.mounts);
    EXPECT_EQ(snapshot.get_metadata(), specs.metadata);
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
    EXPECT_EQ(snapshot.get_extra_interfaces(), std::vector<mp::NetworkInterface>{});
    EXPECT_EQ(snapshot.get_state(), mp::VirtualMachine::State::off);

    auto mount_matcher1 =
        Pair(Eq("guybrush"), Property(&mp::VMMount::get_mount_type, Eq(mp::VMMount::MountType::Classic)));
    auto mount_matcher2 =
        Pair(Eq("murray"), Property(&mp::VMMount::get_mount_type, Eq(mp::VMMount::MountType::Native)));
    EXPECT_THAT(snapshot.get_mounts(), UnorderedElementsAre(mount_matcher1, mount_matcher2));

    EXPECT_THAT(
        snapshot.get_metadata(),
        ResultOf([](const QJsonObject& metadata) { return metadata["arguments"].toArray(); }, Contains("-qmp")));
}

TEST_F(TestQemuSnapshot, capturesSnapshot)
{
    auto snapshot_index = 3;
    auto snapshot_tag = derive_tag(snapshot_index);
    EXPECT_CALL(vm, get_snapshot_count).WillOnce(Return(snapshot_index - 1));

    auto proc_count = 0;

    ArgsMatcher capture_args_matcher{
        ElementsAre("snapshot", "-c", QString::fromStdString(snapshot_tag), desc.image.image_path)};

    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    mock_factory_scope->register_callback([&](mpt::MockProcess* process) {
        ASSERT_LE(++proc_count, 2);

        set_common_expectations_on(process);

        const auto& args_matcher = proc_count == 1 ? list_args_matcher : capture_args_matcher;
        EXPECT_THAT(process->arguments(), args_matcher);
    });

    quick_snapshot().capture();
    EXPECT_EQ(proc_count, 2);
}

TEST_F(TestQemuSnapshot, captureThrowsOnRepeatedTag)
{
    auto snapshot_index = 22;
    auto snapshot_tag = derive_tag(snapshot_index);
    EXPECT_CALL(vm, get_snapshot_count).WillOnce(Return(snapshot_index - 1));

    auto proc_count = 0;
    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    mock_factory_scope->register_callback([&](mpt::MockProcess* process) {
        ASSERT_EQ(++proc_count, 1);

        set_common_expectations_on(process);

        EXPECT_THAT(process->arguments(), list_args_matcher);
        set_tag_output(process, snapshot_tag);
    });

    MP_EXPECT_THROW_THAT(quick_snapshot("whatever").capture(),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("already exists"),
                                               HasSubstr(snapshot_tag),
                                               HasSubstr(desc.image.image_path.toStdString()))));
}

TEST_F(TestQemuSnapshot, erasesSnapshot)
{
    auto snapshot = loaded_snapshot();
    auto proc_count = 0;

    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    mock_factory_scope->register_callback([&](mpt::MockProcess* process) {
        ASSERT_LE(++proc_count, 2);

        set_common_expectations_on(process);

        auto tag = derive_tag(snapshot.get_index());
        if (proc_count == 1)
        {
            EXPECT_THAT(process->arguments(), list_args_matcher);
            set_tag_output(process, tag);
        }
        else
        {
            EXPECT_THAT(process->arguments(),
                        ElementsAre("snapshot", "-d", QString::fromStdString(tag), desc.image.image_path));
        }
    });

    snapshot.erase();
    EXPECT_EQ(proc_count, 2);
}

TEST_F(TestQemuSnapshot, eraseLogsOnMissingTag)
{
    auto snapshot = loaded_snapshot();
    auto proc_count = 0;

    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    mock_factory_scope->register_callback([&](mpt::MockProcess* process) {
        ASSERT_EQ(++proc_count, 1);

        set_common_expectations_on(process);
        EXPECT_THAT(process->arguments(), list_args_matcher);
        set_tag_output(process, "some-tag-other-than-the-one-we-are-looking-for");
    });

    auto expected_log_level = mpl::Level::warning;
    auto logger_scope = mpt::MockLogger::inject(expected_log_level);
    logger_scope.mock_logger->expect_log(expected_log_level, "Could not find");

    snapshot.erase();
}

TEST_F(TestQemuSnapshot, appliesSnapshot)
{
    auto snapshot = loaded_snapshot();
    auto proc_count = 0;

    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    mock_factory_scope->register_callback([&](mpt::MockProcess* process) {
        ASSERT_EQ(++proc_count, 1);

        set_common_expectations_on(process);
        EXPECT_THAT(process->arguments(),
                    ElementsAre("snapshot",
                                "-a",
                                QString::fromStdString(derive_tag(snapshot.get_index())),
                                desc.image.image_path));
    });

    desc.num_cores = 8598;
    desc.mem_size = mp::MemorySize{"49"};
    desc.disk_space = mp::MemorySize{"328"};
    desc.extra_interfaces = std::vector<mp::NetworkInterface>{{"eth16", "16:16:16:16:16:16", true}};

    snapshot.apply();

    EXPECT_EQ(desc.num_cores, snapshot.get_num_cores());
    EXPECT_EQ(desc.mem_size, snapshot.get_mem_size());
    EXPECT_EQ(desc.disk_space, snapshot.get_disk_space());
    EXPECT_EQ(desc.extra_interfaces, snapshot.get_extra_interfaces());
}

TEST_F(TestQemuSnapshot, keepsDescOnFailure)
{
    auto snapshot = loaded_snapshot();
    auto proc_count = 0;

    auto mock_factory_scope = mpt::MockProcessFactory::Inject();
    mock_factory_scope->register_callback([&](mpt::MockProcess* process) {
        ASSERT_EQ(++proc_count, 1);
        EXPECT_CALL(*process, execute).WillOnce(Return(failure));
    });

    desc.num_cores = 123;
    desc.mem_size = mp::MemorySize{"321"};
    desc.disk_space = mp::MemorySize{"56K"};
    desc.extra_interfaces = std::vector<mp::NetworkInterface>{{"eth17", "17:17:17:17:17:17", true}};

    const auto orig_desc = desc;
    MP_EXPECT_THROW_THAT(snapshot.apply(), std::runtime_error, mpt::match_what(HasSubstr("qemu-img failed")));

    EXPECT_EQ(orig_desc.num_cores, desc.num_cores);
    EXPECT_EQ(orig_desc.mem_size, desc.mem_size);
    EXPECT_EQ(orig_desc.disk_space, desc.disk_space);
    EXPECT_EQ(orig_desc.extra_interfaces, desc.extra_interfaces);
}

} // namespace
