/*
 * Copyright (C) 2022 Canonical, Ltd.
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
#include "mock_rpc_stub.h"
#include "mock_terminal.h"

#include <src/client/cli/cmd/remote_settings_handler.h>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

TEST(RemoteSettingsTest, savesProvidedKeyPrefix)
{
    constexpr auto prefix = "my.prefix";
    auto mock_stub = StrictMock<mpt::MockRpcStub>{};
    auto mock_term = mpt::MockTerminal{};

    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 1};
    EXPECT_EQ(handler.get_key_prefix(), prefix);
}

TEST(RemoteSettingsTest, savesProvidedVerbosity)
{
    constexpr auto verbosity = 42;
    auto mock_stub = StrictMock<mpt::MockRpcStub>{};
    auto mock_term = mpt::MockTerminal{};

    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, verbosity};
    EXPECT_EQ(handler.get_verbosity(), verbosity);
}
