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
#include "tests/mock_environment_helpers.h"

#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/snap_utils.h>

#include <QFile>
#include <QTemporaryDir>

#include <utility>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mpu = multipass::utils;

using namespace testing;

namespace
{
const QByteArray snap_name{"multipass"};
} // namespace

TEST(Snap, recognizes_in_snap_when_snap_name_is_multipass)
{
    mpt::SetEnvScope env{"SNAP_NAME", "multipass"};
    EXPECT_TRUE(mpu::in_multipass_snap());
}

TEST(Snap, recognizes_not_in_snap_when_snap_name_is_empty)
{
    mpt::UnsetEnvScope env{"SNAP_NAME"};
    EXPECT_FALSE(mpu::in_multipass_snap());
}

TEST(Snap, recognizes_not_in_snap_when_snap_name_is_otherwise)
{
    mpt::SetEnvScope env{"SNAP_NAME", "otherwise"};
    EXPECT_FALSE(mpu::in_multipass_snap());
}

struct SnapDirs : public TestWithParam<std::pair<const char*, std::function<QByteArray()>>>
{
};

TEST_P(SnapDirs, test_snap_dir_no_throw_if_set)
{
    const auto& [var, getter] = GetParam();
    mpt::SetEnvScope env(var, "/tmp");
    mpt::SetEnvScope env2("SNAP_NAME", snap_name);

    EXPECT_NO_THROW(getter());
}

TEST_P(SnapDirs, test_snap_dir_throws_if_not_set)
{
    const auto& [var, getter] = GetParam();
    mpt::UnsetEnvScope env(var);
    mpt::SetEnvScope env2("SNAP_NAME", snap_name);

    EXPECT_THROW(getter(), mp::SnapEnvironmentException);
}

TEST_P(SnapDirs, test_snap_dir_throws_when_snap_name_not_set)
{
    const auto& [var, getter] = GetParam();
    QTemporaryDir snap_dir;
    mpt::SetEnvScope env(var, snap_dir.path().toUtf8());
    mpt::UnsetEnvScope env2("SNAP_NAME");

    EXPECT_THROW(getter(), mp::SnapEnvironmentException);
}

TEST_P(SnapDirs, test_snap_dir_throws_when_snap_name_not_multipass)
{
    const auto& [var, getter] = GetParam();
    QByteArray other_name{"foo"};
    QTemporaryDir snap_dir;
    mpt::SetEnvScope env(var, snap_dir.path().toUtf8());
    mpt::SetEnvScope env2("SNAP_NAME", other_name);

    EXPECT_THROW(getter(), mp::SnapEnvironmentException);
}

TEST_P(SnapDirs, test_snap_dir_read_ok)
{
    const auto& [var, getter] = GetParam();
    QTemporaryDir snap_dir;
    mpt::SetEnvScope env(var, snap_dir.path().toUtf8());
    mpt::SetEnvScope env2("SNAP_NAME", snap_name);

    EXPECT_EQ(snap_dir.path(), getter());
}

TEST_P(SnapDirs, test_snap_dir_resolves_links)
{
    const auto& [var, getter] = GetParam();
    QTemporaryDir snap_dir, link_dir;
    link_dir.remove();
    QFile::link(snap_dir.path(), link_dir.path());
    mpt::SetEnvScope env(var, link_dir.path().toUtf8());
    mpt::SetEnvScope env2("SNAP_NAME", snap_name);

    EXPECT_EQ(snap_dir.path(), getter());
}

INSTANTIATE_TEST_SUITE_P(SnapUtils,
                         SnapDirs,
                         testing::Values(std::make_pair("SNAP", &mpu::snap_dir),
                                         std::make_pair("SNAP_COMMON", &mpu::snap_common_dir),
                                         std::make_pair("SNAP_REAL_HOME", &mpu::snap_real_home_dir)));
