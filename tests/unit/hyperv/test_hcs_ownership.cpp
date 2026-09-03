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
#include "tests/unit/temp_dir.h"

#include <hyperv_api/hcs_ownership.h>

#include <multipass/file_ops.h>
#include <multipass/json_utils.h>

namespace mp = multipass;
namespace mpt = multipass::test;
namespace mhv = multipass::hyperv;
using namespace testing;

TEST(HCSOwnership, roundTrips)
{
    mpt::TempDir instance_dir;
    const mhv::HCSOwnership expected{
        .active_disk = instance_dir.filePath("active.avhdx").toStdString(),
        .state_file_stem = instance_dir.filePath("hcs-state").toStdString(),
    };

    expected.persist(instance_dir.path().toStdString());
    const auto actual = mhv::HCSOwnership::load(instance_dir.path().toStdString());

    ASSERT_TRUE(actual);
    EXPECT_EQ(actual->active_disk, expected.active_disk);
    EXPECT_EQ(actual->state_file_stem, expected.state_file_stem);
}

TEST(HCSOwnership, rejectsPathsOutsideInstanceDirectory)
{
    mpt::TempDir instance_dir;
    mpt::TempDir other_dir;
    const mhv::HCSOwnership ownership{
        .active_disk = other_dir.filePath("active.avhdx").toStdString(),
        .state_file_stem = instance_dir.filePath("hcs-state").toStdString(),
    };
    ownership.persist(instance_dir.path().toStdString());

    EXPECT_THROW((void)mhv::HCSOwnership::load(instance_dir.path().toStdString()),
                 std::runtime_error);
}
