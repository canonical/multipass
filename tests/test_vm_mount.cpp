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

#include <multipass/vm_mount.h>

namespace mp = multipass;
using namespace testing;

namespace
{
struct TestVMMount : public Test
{
    mp::VMMount a_mount{"asdf", {{1, 2}, {2, 4}}, {{8, 4}, {6, 3}}, multipass::VMMount::MountType::Classic};
};

TEST_F(TestVMMount, comparesEqual)
{
    auto b = a_mount;
    EXPECT_EQ(a_mount, b);
    EXPECT_EQ(b, a_mount);

    b.mount_type = multipass::VMMount::MountType::Native;
    EXPECT_FALSE(a_mount == b);
    EXPECT_FALSE(a_mount == mp::VMMount{});
}

TEST_F(TestVMMount, comparesUnequal)
{
    mp::VMMount b{a_mount};
    mp::VMMount c{a_mount};
    mp::VMMount d{a_mount};
    mp::VMMount e{a_mount};
    mp::VMMount f{a_mount};

    c.source_path = "fdsa";
    d.uid_mappings.emplace_back(10, 5);
    e.gid_mappings.pop_back();
    f.mount_type = multipass::VMMount::MountType::Native;

    EXPECT_FALSE(a_mount != b); // Force use of operator!=
    for (const auto* other : {&c, &d, &e, &f})
    {
        EXPECT_NE(a_mount, *other);
        EXPECT_NE(*other, a_mount);
    }
}
} // namespace
