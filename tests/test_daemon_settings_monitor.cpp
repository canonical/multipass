/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include "mock_settings.h"

#include <multipass/constants.h>
#include <src/daemon/daemon_settings_monitor.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QFileSystemWatcher>
#include <QTemporaryFile>
#include <QTimer>

#include <iostream>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{

struct SettingsMonitor : public Test
{
    void SetUp()
    {
        fake_settings_file.open();
        EXPECT_CALL(mock_settings, get_daemon_settings_file_path).WillOnce(Return(fake_settings_file.fileName()));
    }

    void setup_file_change_trigger()
    {
        // Trigger a file change as soon as QCoreApplication starts
        QTimer::singleShot(0, [this] {
            fake_settings_file.write("blah");
            fake_settings_file.close();
        });
        // Exit if the one above doesn't
        QTimer::singleShot(1, []() { QCoreApplication::exit(1); });
    }

    QTemporaryFile fake_settings_file;
    mpt::MockSettings& mock_settings = mpt::MockSettings::mock_instance(); /* although this is shared, expectations are
                                                                              reset at the end of each test */
};

TEST_F(SettingsMonitor, exits_on_driver_changed)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key))).WillOnce(Return("other"));
    setup_file_change_trigger();

    mp::DaemonSettingsMonitor monitor{"this"};

    auto ret = QCoreApplication::exec();

    EXPECT_EQ(ret, 42) << "Settings monitor did not quit.";
}

TEST_F(SettingsMonitor, does_not_exit_on_driver_stable)
{
    EXPECT_CALL(mock_settings, get(Eq(mp::driver_key))).WillOnce(Return("this"));
    setup_file_change_trigger();

    mp::DaemonSettingsMonitor monitor{"this"};

    auto ret = QCoreApplication::exec();

    EXPECT_NE(ret, 42) << "Settings monitor quit when it shouldn't have.";
}
} // namespace
