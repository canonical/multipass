/*
 * Copyright (C) 2019-2020 Canonical, Ltd.
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

#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/snap_utils.h>

#include <QFile>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#include "tests/mock_environment_helpers.h"

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mu = multipass::utils;

using namespace testing;

TEST(SnapUtils, test_is_confined_when_snap_dir_set)
{
    mpt::SetEnvScope env("SNAP", "/tmp");

    EXPECT_NO_THROW(mu::snap_dir());
}

TEST(SnapUtils, test_is_not_confined_when_snap_dir_not_set)
{
    mpt::UnsetEnvScope env("SNAP");

    EXPECT_THROW(mu::snap_dir(), mp::SnapEnvironmentException);
}

TEST(SnapUtils, test_snap_dir_read_ok)
{
    QTemporaryDir snap_dir;
    mpt::SetEnvScope env("SNAP", snap_dir.path().toUtf8());

    EXPECT_EQ(snap_dir.path(), mu::snap_dir());
}

TEST(SnapUtils, test_snap_dir_resolves_links)
{
    QTemporaryDir snap_dir, link_dir;
    link_dir.remove();
    QFile::link(snap_dir.path(), link_dir.path());
    mpt::SetEnvScope env("SNAP", link_dir.path().toUtf8());

    EXPECT_EQ(snap_dir.path(), mu::snap_dir());
}

TEST(SnapUtils, test_snap_common_dir_read_ok)
{
    QTemporaryDir snap_dir;
    mpt::SetEnvScope env("SNAP_COMMON", snap_dir.path().toUtf8());

    EXPECT_EQ(snap_dir.path(), mu::snap_common_dir());
}

TEST(SnapUtils, test_snap_common_dir_throws_if_not_set)
{
    mpt::UnsetEnvScope env("SNAP_COMMON");

    EXPECT_THROW(mu::snap_common_dir(), mp::SnapEnvironmentException);
}

TEST(SnapUtils, test_snap_common_resolves_links)
{
    QTemporaryDir common_dir, link_dir;
    link_dir.remove();
    QFile::link(common_dir.path(), link_dir.path());
    mpt::SetEnvScope env("SNAP_COMMON", link_dir.path().toUtf8());

    EXPECT_EQ(common_dir.path(), mu::snap_common_dir());
}
