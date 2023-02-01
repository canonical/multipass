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
#include "mock_standard_paths.h"

#include <QCoreApplication>

namespace mp = multipass;

// Normally one would just use libgtest_main but our static library dependencies
// also define main... DAMN THEM!
int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("multipass_tests");

    ::testing::InitGoogleTest(&argc, argv);
    mp::test::MockStandardPaths::mockit();

    return RUN_ALL_TESTS();
}
