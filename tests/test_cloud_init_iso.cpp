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
#include "temp_dir.h"

#include <multipass/cloud_init_iso.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct CloudInitIso : public testing::Test
{
    CloudInitIso()
    {
        iso_path = QDir{temp_dir.path()}.filePath("test.iso");
    }
    mpt::TempDir temp_dir;
    QString iso_path;
};

TEST_F(CloudInitIso, creates_iso_file)
{
    mp::CloudInitIso iso;
    iso.add_file("test", "test data");
    iso.write_to(iso_path);

    QFile file{iso_path};
    EXPECT_TRUE(file.exists());
    EXPECT_THAT(file.size(), Ge(0));
}

TEST_F(CloudInitIso, reads_non_exist_iso_file_throw)
{
    mp::CloudInitIso iso;
    MP_EXPECT_THROW_THAT(
        iso.read_from(std::filesystem::path{"non_existing_path"}),
        std::runtime_error,
        mpt::match_what(StrEq("The cloud-init-config.iso file does not exist or is not a regular file. ")));
}

TEST_F(CloudInitIso, reads_iso_file)
{
    mp::CloudInitIso orignal_iso;
    orignal_iso.add_file("meta-data", "test_data1\ntest_data2\n");
    orignal_iso.add_file("test3", "test data3");
    orignal_iso.add_file("test2", "test data2");
    orignal_iso.add_file("test4", "test data4");
    orignal_iso.write_to(iso_path);

    mp::CloudInitIso new_iso;
    new_iso.read_from(iso_path.toStdString());
    EXPECT_EQ(orignal_iso, new_iso);
}
