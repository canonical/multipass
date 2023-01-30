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

#include "test_with_mocked_bin_path.h"
#include "path.h"

#include <QDir>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

void mpt::TestWithMockedBinPath::SetUp()
{
    QByteArray new_path =
        QByteArray::fromStdString(mpt::mock_bin_path()) + QDir::listSeparator().toLatin1() + qgetenv("PATH");
    env = std::make_unique<SetEnvScope>("PATH", new_path);
}

void mpt::TestWithMockedBinPath::TearDown()
{
    env.reset();
}
