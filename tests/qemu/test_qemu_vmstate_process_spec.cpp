/*
 * Copyright (C) 2020-2021 Canonical, Ltd.
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

#include <src/platform/backends/qemu/qemu_vmstate_process_spec.h>

#include <QStringList>

namespace mp = multipass;
using namespace testing;

struct TestQemuVmStateProcessSpec : public Test
{
    QString file_name{"foo"};
    const QString host_arch{"x86_64"};
};

TEST_F(TestQemuVmStateProcessSpec, default_arguments_correct)
{
    mp::QemuVmStateProcessSpec spec{file_name, host_arch};

    EXPECT_EQ(spec.arguments(), QStringList({"-nographic", "-dump-vmstate", file_name}));
}
