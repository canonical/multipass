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

#include <src/platform/backends/qemu/qemu_snapshot.h>

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
};
} // namespace
