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
#if defined Q_PROCESSOR_X86 or defined Q_PROCESSOR_S390_X
    const QStringList default_arguments = {"-nographic", "-dump-vmstate", file_name};
#elif defined Q_PROCESSOR_ARM
    const QStringList default_arguments = {"-machine",
                                           "virt",
                                           "-nographic",
                                           "-dump-vmstate",
                                           file_name};
#endif
    EXPECT_EQ(spec.arguments(), default_arguments);
}
