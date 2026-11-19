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

#pragma once

#include <multipass/cli/alias_dict.h>
#include <multipass/constants.h>
#include <multipass/json_utils.h>

#include "common.h"
#include "file_operations.h"
#include "mock_standard_paths.h"
#include "stub_terminal.h"
#include "temp_dir.h"

#include <map>

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

    // Contexts other than the default one can only come from files written by older versions.
    static boost::json::value make_legacy_json(
        const std::map<std::string, std::vector<std::pair<std::string, mp::AliasDefinition>>>&
            contexts,
        const std::string& active_context = mp::default_context_name)
    {
        boost::json::object json_contexts;
        for (const auto& [context, aliases] : contexts)
            json_contexts[context] =
                boost::json::value_from(mp::AliasContext{aliases.begin(), aliases.end()});

        return {{"active-context", active_context}, {"contexts", std::move(json_contexts)}};
    }

    void populate_db_file(const std::vector<std::pair<std::string, mp::AliasDefinition>>& aliases,
                          const std::string& context = mp::default_context_name)
    {
        if (context != mp::default_context_name)
        {
            mpt::make_file_with_content(
                QString::fromStdString(db_filename()),
                mp::pretty_print(make_legacy_json({{context, aliases}}, context)));
            return;
        }

        static std::stringstream trash_stream;
        mpt::StubTerminal term(trash_stream, trash_stream, trash_stream);
        auto writer = mp::AliasDict(&term, db_filename());

        for (const auto& alias : aliases)
            writer.add_alias(alias.first, alias.second);
    }

    mpt::TempDir fake_alias_dir;
};
} // namespace
