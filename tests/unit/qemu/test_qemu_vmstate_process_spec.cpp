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

#include "tests/unit/common.h"

#include <src/platform/backends/qemu/qemu_vmstate_process_spec.h>

#include <QStringList>

namespace mp = multipass;
using namespace testing;

struct TestQemuVmStateProcessSpec : public Test
{
    QString file_name{"foo"};
};

TEST_F(TestQemuVmStateProcessSpec, defaultArgumentsCorrect)
{
    mp::QemuVmStateProcessSpec spec{file_name};
    QStringList default_arguments;

#if defined Q_PROCESSOR_S390
    default_arguments << "-machine"
                      << "s390-ccw-virtio";
#elif defined Q_PROCESSOR_ARM
    default_arguments << "-machine"
                      << "virt";
#endif

    default_arguments << "-nographic"
                      << "-dump-vmstate" << file_name;

    EXPECT_EQ(spec.arguments(), default_arguments);
}
