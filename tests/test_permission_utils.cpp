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

#include "mock_file_ops.h"
#include "mock_permission_utils.h"
#include "mock_platform.h"
#include "mock_recursive_dir_iterator.h"

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
struct TestPermissionUtils : public Test
{
    const mpt::MockFileOps::GuardedMock guarded_mock_file_ops = mpt::MockFileOps::inject<StrictMock>();
    mpt::MockFileOps& mock_file_ops = *guarded_mock_file_ops.first;
    const mpt::MockPlatform::GuardedMock guarded_mock_platform = mpt::MockPlatform::inject<StrictMock>();
    mpt::MockPlatform& mock_platform = *guarded_mock_platform.first;

    const mp::fs::path test_path{"test_path"};
    static constexpr std::filesystem::perms test_permissions{std::filesystem::perms::owner_read};
    static constexpr std::filesystem::perms restricted_permissions{std::filesystem::perms::owner_all};
};

struct TestPermissionUtilsNoFile : public TestPermissionUtils
{
    TestPermissionUtilsNoFile()
    {
        EXPECT_CALL(mock_file_ops, exists(test_path, _)).WillOnce(Return(false));
    }
};

TEST_F(TestPermissionUtilsNoFile, set_permissions_throws_when_file_non_existant)
{
    MP_EXPECT_THROW_THAT(MP_PERMISSIONS.set_permissions(test_path, test_permissions),
                         std::runtime_error,
                         mpt::match_what(HasSubstr(test_path.string())));
}

TEST_F(TestPermissionUtilsNoFile, take_ownership_throws_when_file_non_existant)
{
    MP_EXPECT_THROW_THAT(MP_PERMISSIONS.take_ownership(test_path),
                         std::runtime_error,
                         mpt::match_what(HasSubstr(test_path.string())));
}

TEST_F(TestPermissionUtilsNoFile, restrict_permissions_throws_when_file_non_existant)
{
    MP_EXPECT_THROW_THAT(MP_PERMISSIONS.restrict_permissions(test_path),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("nonexistent file"), HasSubstr(test_path.string()))));
}

struct TestPermissionUtilsFile : public TestPermissionUtils
{
    TestPermissionUtilsFile()
    {
        EXPECT_CALL(mock_file_ops, exists(test_path, _)).WillOnce(Return(true));
        EXPECT_CALL(mock_file_ops, is_directory(test_path, _)).WillRepeatedly(Return(false));
    }
};

TEST_F(TestPermissionUtilsFile, set_permissions_sets_permissions)
{
    EXPECT_CALL(mock_platform, set_permissions(test_path, test_permissions, false)).WillOnce(Return(true));

    MP_PERMISSIONS.set_permissions(test_path, test_permissions);
}

TEST_F(TestPermissionUtilsFile, set_permissions_throws_on_failure)
{
    EXPECT_CALL(mock_platform, set_permissions(test_path, test_permissions, _)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_PERMISSIONS.set_permissions(test_path, test_permissions),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Cannot set permissions"), HasSubstr(test_path.string()))));
}

TEST_F(TestPermissionUtilsFile, take_ownership_takes_ownership)
{
    EXPECT_CALL(mock_platform, take_ownership(test_path)).WillOnce(Return(true));

    MP_PERMISSIONS.take_ownership(test_path);
}

TEST_F(TestPermissionUtilsFile, take_ownership_throws_on_failure)
{
    EXPECT_CALL(mock_platform, take_ownership(test_path)).WillOnce(Return(false));

    MP_EXPECT_THROW_THAT(MP_PERMISSIONS.take_ownership(test_path),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Cannot set owner"), HasSubstr(test_path.string()))));
}

TEST_F(TestPermissionUtilsFile, restrict_permissions_restricts_permissions)
{
    EXPECT_CALL(mock_platform, take_ownership(test_path)).WillOnce(Return(true));
    EXPECT_CALL(mock_platform, set_permissions(test_path, restricted_permissions, false)).WillOnce(Return(true));

    MP_PERMISSIONS.restrict_permissions(test_path);
}

struct TestPermissionUtilsDir : public TestPermissionUtils
{
    TestPermissionUtilsDir()
    {
        EXPECT_CALL(mock_file_ops, exists(test_path, _)).WillRepeatedly(Return(true));
        EXPECT_CALL(mock_file_ops, is_directory(test_path, _)).WillOnce(Return(true));

        EXPECT_CALL(entry1, path).WillRepeatedly(ReturnRef(path1));
        EXPECT_CALL(entry2, path).WillRepeatedly(ReturnRef(path2));

        auto iter = std::make_unique<mpt::MockRecursiveDirIterator>();
        auto iter_p = iter.get();

        EXPECT_CALL(*iter_p, hasNext).WillOnce(Return(true)).WillOnce(Return(true)).WillRepeatedly(Return(false));
        EXPECT_CALL(*iter_p, next).WillOnce(ReturnRef(entry1)).WillOnce(ReturnRef(entry2));
        EXPECT_CALL(mock_file_ops, recursive_dir_iterator(test_path, _)).WillOnce(Return(std::move(iter)));
    }

    const mp::fs::path path1{"Hello.txt"};
    mpt::MockDirectoryEntry entry1;

    const mp::fs::path path2{"World.txt"};
    mpt::MockDirectoryEntry entry2;
};

TEST_F(TestPermissionUtilsDir, set_permissions_iterates_dir)
{
    EXPECT_CALL(mock_platform, set_permissions(test_path, test_permissions, false)).WillOnce(Return(true));
    EXPECT_CALL(mock_platform, set_permissions(path1, test_permissions, true)).WillOnce(Return(true));
    EXPECT_CALL(mock_platform, set_permissions(path2, test_permissions, true)).WillOnce(Return(true));

    MP_PERMISSIONS.set_permissions(test_path, test_permissions);
}

TEST_F(TestPermissionUtilsDir, take_ownership_iterates_dir)
{
    EXPECT_CALL(mock_platform, take_ownership(test_path)).WillOnce(Return(true));
    EXPECT_CALL(mock_platform, take_ownership(path1)).WillOnce(Return(true));
    EXPECT_CALL(mock_platform, take_ownership(path2)).WillOnce(Return(true));

    MP_PERMISSIONS.take_ownership(test_path);
}

TEST_F(TestPermissionUtilsDir, restrict_permissions_iterates_dir)
{
    EXPECT_CALL(mock_platform, take_ownership(test_path)).WillOnce(Return(true));
    EXPECT_CALL(mock_platform, take_ownership(path1)).WillOnce(Return(true));
    EXPECT_CALL(mock_platform, take_ownership(path2)).WillOnce(Return(true));

    EXPECT_CALL(mock_platform, set_permissions(test_path, restricted_permissions, false)).WillOnce(Return(true));
    EXPECT_CALL(mock_platform, set_permissions(path1, restricted_permissions, true)).WillOnce(Return(true));
    EXPECT_CALL(mock_platform, set_permissions(path2, restricted_permissions, true)).WillOnce(Return(true));

    MP_PERMISSIONS.restrict_permissions(test_path);
}

struct TestPermissionUtilsBadDir : public TestPermissionUtils
{
    TestPermissionUtilsBadDir()
    {
        EXPECT_CALL(mock_file_ops, exists(test_path, _)).WillRepeatedly(Return(true));
        EXPECT_CALL(mock_file_ops, is_directory(test_path, _)).WillOnce(Return(true));

        EXPECT_CALL(mock_file_ops, recursive_dir_iterator(test_path, _)).WillOnce(Return(nullptr));
    }
};

TEST_F(TestPermissionUtilsBadDir, set_permissions_throws_on_broken_iterator)
{
    EXPECT_CALL(mock_platform, set_permissions(test_path, test_permissions, false)).WillOnce(Return(true));

    MP_EXPECT_THROW_THAT(MP_PERMISSIONS.set_permissions(test_path, test_permissions),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Cannot iterate"), HasSubstr(test_path.string()))));
}

TEST_F(TestPermissionUtilsBadDir, take_ownership_throws_on_broken_iterator)
{
    EXPECT_CALL(mock_platform, take_ownership(test_path)).WillOnce(Return(true));

    MP_EXPECT_THROW_THAT(MP_PERMISSIONS.take_ownership(test_path),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Cannot iterate"), HasSubstr(test_path.string()))));
}

TEST_F(TestPermissionUtilsBadDir, restrict_permissions_throws_on_broken_iterator)
{
    EXPECT_CALL(mock_platform, set_permissions(test_path, restricted_permissions, false)).WillOnce(Return(true));
    EXPECT_CALL(mock_platform, take_ownership(test_path)).WillOnce(Return(true));

    MP_EXPECT_THROW_THAT(MP_PERMISSIONS.restrict_permissions(test_path),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("Cannot iterate"), HasSubstr(test_path.string()))));
}

} // namespace
