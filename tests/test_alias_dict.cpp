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

typedef std::vector<std::pair<std::string, mp::AliasDefinition>> AliasesVector;

struct WriteReadTeststuite : public AliasDictionary, public WithParamInterface<AliasesVector>
{
};

void create_alias_file(const std::string& file_name,
                       const std::vector<std::pair<std::string, mp::AliasDefinition>>& aliases)
{
    mp::AliasDict writer(file_name);

    for (const auto& alias : aliases)
        writer.add_alias(alias.first, alias.second);
}

TEST_P(WriteReadTeststuite, writes_and_reads_files)
{
    auto aliases_vector = GetParam();

    mpt::TempDir temp_dir;
    std::string fake_db_file{(temp_dir.path() + "/fake_config_file").toStdString()};

    create_alias_file(fake_db_file, aliases_vector);

    mp::AliasDict reader(fake_db_file);

    for (const auto& alias : aliases_vector)
    {
        auto read_value = reader.get_alias(alias.first);
        ASSERT_TRUE(read_value);
        ASSERT_EQ((*read_value).instance, alias.second.instance);
        ASSERT_EQ((*read_value).command, alias.second.command);
        ASSERT_EQ((*read_value).arguments, alias.second.arguments);
    }

    ASSERT_EQ((size_t)std::distance(reader.cbegin(), reader.cend()), (size_t)aliases_vector.size());
}

INSTANTIATE_TEST_SUITE_P(
    AliasDictionary, WriteReadTeststuite,
    Values(AliasesVector{}, AliasesVector{{"w", {"fake", "w", {}}}}, AliasesVector{{"ipf", {"instance", "ip", {"a"}}}},
           AliasesVector{{"lsp", {"primary", "ls", {}}}, {"llp", {"primary", "ls", {"-l", "-a"}}}}));
} // namespace
