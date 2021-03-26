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
#include <multipass/constants.h>

#include <gmock/gmock.h>

#include "file_operations.h"
#include "mock_standard_paths.h"
#include "temp_dir.h"

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct AliasDictionary : public Test
{
    AliasDictionary()
    {
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(_))
            .WillRepeatedly(Return(temp_dir.path()));
    }

    std::string db_filename()
    {
        const auto file_name = QStringLiteral("%1_aliases.json").arg(mp::client_name);

        return temp_dir.filePath(file_name).toStdString();
    }

    void populate_db_file(const std::vector<std::pair<std::string, mp::AliasDefinition>>& aliases)
    {
        mp::AliasDict writer;

        auto db_qfilename = QString::fromStdString(db_filename());

        for (const auto& alias : aliases)
            writer.add_alias(alias.first, alias.second);
    }

    mpt::TempDir temp_dir;
};

TEST_F(AliasDictionary, works_with_empty_file)
{
    QFile db(QString::fromStdString(db_filename()));

    db.open(QIODevice::ReadWrite); // Create the database file.

    mp::AliasDict dict;

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_with_unexisting_file)
{
    mp::AliasDict dict;

    ASSERT_TRUE(dict.empty());
}

TEST_F(AliasDictionary, works_with_broken_file)
{
    mpt::make_file_with_content(QString::fromStdString(db_filename()), "broken file {]");

    mp::AliasDict dict;

    ASSERT_TRUE(dict.empty());
}

typedef std::vector<std::pair<std::string, mp::AliasDefinition>> AliasesVector;

struct WriteReadTeststuite : public AliasDictionary, public WithParamInterface<AliasesVector>
{
};

TEST_P(WriteReadTeststuite, writes_and_reads_files)
{
    auto aliases_vector = GetParam();

    populate_db_file(aliases_vector);

    mp::AliasDict reader;

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
