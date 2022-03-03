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
#include "mock_client_rpc.h"
#include "mock_terminal.h"

#include <src/client/cli/cmd/remote_settings_handler.h>

#include <sstream>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
class RemoteSettingsTest : public Test
{
public:
    void SetUp() override
    {
        EXPECT_CALL(mock_term, cout).WillRepeatedly(ReturnRef(fake_cout));
        EXPECT_CALL(mock_term, cerr).WillRepeatedly(ReturnRef(fake_cerr));
    }

public:
    std::ostringstream fake_cout;
    std::ostringstream fake_cerr;
    mpt::MockTerminal mock_term;
    StrictMock<mpt::MockRpcStub> mock_stub{};
};

TEST_F(RemoteSettingsTest, savesProvidedKeyPrefix)
{
    constexpr auto prefix = "my.prefix";
    mp::RemoteSettingsHandler handler{prefix, mock_stub, &mock_term, 1};

    EXPECT_EQ(handler.get_key_prefix(), prefix);
}

TEST_F(RemoteSettingsTest, savesProvidedVerbosity)
{
    constexpr auto verbosity = 42;
    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, verbosity};

    EXPECT_EQ(handler.get_verbosity(), verbosity);
}

TEST_F(RemoteSettingsTest, honorsVerbosityInKeysRequest)
{
    constexpr auto verbosity = 5;
    EXPECT_CALL(mock_stub, keysRaw(_, Property(&mp::KeysRequest::verbosity_level, Eq(verbosity))))
        .WillOnce(ReturnNew<mpt::MockClientReader<mp::KeysReply>>());

    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, verbosity};
    handler.keys();
}

TEST_F(RemoteSettingsTest, keysEmptyByDefault)
{
    EXPECT_CALL(mock_stub, keysRaw).WillOnce(ReturnNew<mpt::MockClientReader<mp::KeysReply>>());
    mp::RemoteSettingsHandler handler{"prefix", mock_stub, &mock_term, 31};

    EXPECT_THAT(handler.keys(), IsEmpty());
}
} // namespace
