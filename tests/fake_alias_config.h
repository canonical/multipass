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

#ifndef MULTIPASS_FAKE_ALIAS_CONFIG_H
#define MULTIPASS_FAKE_ALIAS_CONFIG_H

#include <multipass/cli/alias_dict.h>
#include <multipass/constants.h>

#include "common.h"
#include "mock_standard_paths.h"
#include "stub_terminal.h"
#include "temp_dir.h"

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
struct FakeAliasConfig
{
    FakeAliasConfig()
    {
        EXPECT_CALL(mpt::MockStandardPaths::mock_instance(), writableLocation(_))
            .WillRepeatedly(Return(fake_alias_dir.path()));
    }

    std::string db_filename()
    {

        const auto file_name = QStringLiteral("%1/%1_aliases.json").arg(mp::client_name);

        return fake_alias_dir.filePath(file_name).toStdString();
    }

    void populate_db_file(const std::vector<std::pair<std::string, mp::AliasDefinition>>& aliases)
    {
        static std::stringstream trash_stream;
        mpt::StubTerminal term(trash_stream, trash_stream, trash_stream);
        mp::AliasDict writer(&term);

        for (const auto& alias : aliases)
            writer.add_alias(alias.first, alias.second);
    }

    mpt::TempDir fake_alias_dir;
};
} // namespace
#endif // MULTIPASS_FAKE_ALIAS_CONFIG_H
