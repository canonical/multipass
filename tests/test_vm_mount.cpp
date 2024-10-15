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
#include "mock_file_ops.h"

#include <multipass/vm_mount.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct TestVMMount : public Test
{
    static const mp::VMMount a_mount;
};

struct TestUnequalVMMountsTestSuite : public Test, public WithParamInterface<std::pair<mp::VMMount, mp::VMMount>>
{
};

const mp::VMMount TestVMMount::a_mount{"asdf", {{1, 2}, {2, 4}}, {{8, 4}, {6, 3}}, mp::VMMount::MountType::Classic};

TEST_P(TestUnequalVMMountsTestSuite, CompareMountsUnequal)
{
    auto [mount_a, mount_b] = GetParam();

    // Force use of overloaded operator== and operator!=
    EXPECT_TRUE(mount_a != mount_b);
    EXPECT_FALSE(mount_a == mount_b);
}

INSTANTIATE_TEST_SUITE_P(
    VMMounts,
    TestUnequalVMMountsTestSuite,
    Values(std::make_pair(TestVMMount::a_mount,
                          mp::VMMount(TestVMMount::a_mount.get_source_path(),
                                      TestVMMount::a_mount.get_gid_mappings(),
                                      TestVMMount::a_mount.get_uid_mappings(),
                                      mp::VMMount::MountType::Native)),
           std::make_pair(TestVMMount::a_mount,
                          mp::VMMount("fdsa",
                                      TestVMMount::a_mount.get_gid_mappings(),
                                      TestVMMount::a_mount.get_uid_mappings(),
                                      TestVMMount::a_mount.get_mount_type())),
           std::make_pair(TestVMMount::a_mount,
                          mp::VMMount(TestVMMount::a_mount.get_source_path(),
                                      mp::id_mappings{{1, 2}, {2, 4}, {10, 5}},
                                      TestVMMount::a_mount.get_uid_mappings(),
                                      TestVMMount::a_mount.get_mount_type())),
           std::make_pair(TestVMMount::a_mount,
                          mp::VMMount(TestVMMount::a_mount.get_source_path(),
                                      TestVMMount::a_mount.get_gid_mappings(),
                                      mp::id_mappings({TestVMMount::a_mount.get_uid_mappings()[0]}),
                                      TestVMMount::a_mount.get_mount_type()))));

TEST_F(TestVMMount, comparesEqual)
{
    auto b = a_mount;
    EXPECT_EQ(a_mount, b);
    EXPECT_EQ(b, a_mount);
    EXPECT_NE(&b, &a_mount);
}

TEST_F(TestVMMount, serializeAndDeserializeToAndFromJson)
{
    auto jsonObj = TestVMMount::a_mount.serialize();

    EXPECT_EQ(jsonObj["source_path"].toString().toStdString(), TestVMMount::a_mount.get_source_path());
    EXPECT_EQ(jsonObj["mount_type"], static_cast<int>(TestVMMount::a_mount.get_mount_type()));

    auto b_mount = mp::VMMount{jsonObj};
    EXPECT_EQ(TestVMMount::a_mount, b_mount);
}

TEST_F(TestVMMount, duplicateUidsThrowsWithDuplicateHostID)
{
    MP_EXPECT_THROW_THAT(mp::VMMount("src",
                                     mp::id_mappings{{1000, 1000}},
                                     mp::id_mappings{{1000, 1000}, {1000, 1001}},
                                     mp::VMMount::MountType::Classic),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Mount cannot apply mapping with duplicate ids:"),
                                               HasSubstr("uids: 1000: "),
                                               HasSubstr("1000:1001"),
                                               HasSubstr("1000:1000"))));
}

TEST_F(TestVMMount, duplicateUidsThrowsWithDuplicateTargetID)
{
    MP_EXPECT_THROW_THAT(mp::VMMount("src",
                                     mp::id_mappings{{1000, 1000}},
                                     mp::id_mappings{{1002, 1001}, {1000, 1001}},
                                     mp::VMMount::MountType::Classic),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Mount cannot apply mapping with duplicate ids:"),
                                               HasSubstr("uids: 1001: "),
                                               HasSubstr("1002:1001"),
                                               HasSubstr("1000:1001"))));
}

TEST_F(TestVMMount, duplicateGidsThrowsWithDuplicateHostID)
{
    MP_EXPECT_THROW_THAT(mp::VMMount("src",
                                     mp::id_mappings{{1000, 1000}, {1000, 1001}},
                                     mp::id_mappings{{1000, 1000}},
                                     mp::VMMount::MountType::Classic),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Mount cannot apply mapping with duplicate ids:"),
                                               HasSubstr("gids: 1000: "),
                                               HasSubstr("1000:1001"),
                                               HasSubstr("1000:1000"))));
}

TEST_F(TestVMMount, duplicateGidsThrowsWithDuplicateTargetID)
{
    MP_EXPECT_THROW_THAT(mp::VMMount("src",
                                     mp::id_mappings{{1002, 1001}, {1000, 1001}},
                                     mp::id_mappings{{1000, 1000}},
                                     mp::VMMount::MountType::Classic),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Mount cannot apply mapping with duplicate ids:"),
                                               HasSubstr("gids: 1001: "),
                                               HasSubstr("1002:1001"),
                                               HasSubstr("1000:1001"))));
}

TEST_F(TestVMMount, sourcePathResolved)
{
    const auto source_path = mp::fs::path{"/tmp/./src/main/../../src"};
    const auto dest_path = mp::fs::absolute(source_path);

    const auto [mock_file_ops, _] = mpt::MockFileOps::inject();
    EXPECT_CALL(*mock_file_ops, weakly_canonical).WillOnce(Return(dest_path));

    const mp::VMMount mount{source_path.string(), {}, {}, mp::VMMount::MountType::Classic};

    EXPECT_EQ(mount.get_source_path(), dest_path.string());
}

} // namespace
