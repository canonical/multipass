/*
 * Copyright (C) 2018 Canonical, Ltd.
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

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

void mpt::TestWithMockedBinPath::SetUp()
{
#ifdef WIN32
    auto separator = ";";
#else
    auto separator = ":";
#endif

    QByteArray new_path = QByteArray::fromStdString(mpt::mock_bin_path()) + separator + qgetenv("PATH");
    env = std::make_unique<SetEnvScope>("PATH", new_path);
}

void mpt::TestWithMockedBinPath::TearDown()
{
    env.reset();
}
