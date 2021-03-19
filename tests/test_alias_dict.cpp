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

#include <multipass/cli/alias_dict.h>

#include <gmock/gmock.h>

#include "file_operations.h"
#include "temp_dir.h"
#include "temp_file.h"

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct AliasDictionary : public Test
{
};

TEST_F(AliasDictionary, works_with_empty_file)
{
    mpt::TempFile fake_db;
    mp::AliasDict dict(fake_db.name().toStdString());

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_with_unexisting_file)
{
    auto unexisting_file = mpt::TempDir().path().toStdString() + "/unexisting_file";
    mp::AliasDict dict(unexisting_file);

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_with_broken_file)
{
    mpt::TempDir temp_dir;
    auto broken_db(temp_dir.path() + "/broken_config_file");

    mpt::make_file_with_content(broken_db, "broken file {]");

    mp::AliasDict dict(broken_db.toStdString());

    ASSERT_TRUE(dict.empty());
}
} // namespace
