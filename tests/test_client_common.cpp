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

#include "common.h"
#include "mock_settings.h"

#include <multipass/cli/client_common.h>
#include <multipass/persistent_settings_handler.h>

namespace mp = multipass;
namespace mpt = mp::test;
using namespace testing;

namespace
{
struct TestClientRegisteredSettingsHandlers : public Test
{
    mpt::MockSettings& mock_settings = mpt::MockSettings::mock_instance();
};

TEST_F(TestClientRegisteredSettingsHandlers, registers_persistent_handler_for_client_settings)
{
    std::unique_ptr<mp::SettingsHandler> handler = nullptr;
    EXPECT_CALL(mock_settings, register_handler(NotNull())) // TODO@ricab will need to distinguish types (need #2282)
        .WillOnce([&handler](auto&& arg) { handler = std::move(arg); });

    mp::client::register_settings_handlers();

    ASSERT_THAT(dynamic_cast<mp::PersistentSettingsHandler*>(handler.get()), NotNull()); // TODO@ricab move into expect

    // TODO@ricab test get keys
}
} // namespace
