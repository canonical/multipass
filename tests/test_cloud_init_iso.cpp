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

#include <multipass/cloud_init_iso.h>

#include <QTemporaryDir>
#include <gmock/gmock.h>

namespace mp = multipass;
using namespace testing;

struct CloudInitIso : public testing::Test
{
    CloudInitIso()
    {
        if (!dir.isValid())
            throw std::runtime_error("test failed to create temp directory");
    }
    QTemporaryDir dir;
};

TEST_F(CloudInitIso, creates_iso_file)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");

    auto file_path = dir.filePath("test.iso");
    iso.write_to(file_path);

    QFile file{file_path};
    EXPECT_TRUE(file.exists());
    EXPECT_THAT(file.size(), Ge(0));
}
