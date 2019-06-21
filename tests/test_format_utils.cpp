/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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

#include <multipass/cli/format_utils.h>
#include <multipass/rpc/multipass.grpc.pb.h>

#include <gmock/gmock.h>

namespace mp = multipass;
using namespace testing;

TEST(InstanceStateString, RUNNING_state_returns_Running)
{
    mp::InstanceStatus status;
    status.set_state(mp::InstanceState::RUNNING);
    auto state_string = mp::format::state_string_for(status.state());

    EXPECT_THAT(state_string, Eq("Running"));
}

TEST(InstanceStateString, STOPPED_state_returns_Stopped)
{
    mp::InstanceStatus status;
    status.set_state(mp::InstanceState::STOPPED);
    auto state_string = mp::format::state_string_for(status.state());

    EXPECT_THAT(state_string, Eq("Stopped"));
}

TEST(InstanceStateString, DELETED_state_returns_Deleted)
{
    mp::InstanceStatus status;
    status.set_state(mp::InstanceState::DELETED);
    auto state_string = mp::format::state_string_for(status.state());

    EXPECT_THAT(state_string, Eq("Deleted"));
}

TEST(InstanceStateString, SUSPENDING_status_returns_Suspending)
{
    mp::InstanceStatus status;
    status.set_state(mp::InstanceState::SUSPENDING);
    auto state_string = mp::format::state_string_for(status.state());

    EXPECT_THAT(state_string, Eq("Suspending"));
}

TEST(InstanceStateString, SUSPENDED_state_returns_Suspended)
{
    mp::InstanceStatus status;
    status.set_state(mp::InstanceState::SUSPENDED);
    auto state_string = mp::format::state_string_for(status.state());

    EXPECT_THAT(state_string, Eq("Suspended"));
}

TEST(InstanceStateString, RESTARTING_state_returns_Restarting)
{
    mp::InstanceStatus status;
    status.set_state(mp::InstanceState::RESTARTING);
    auto state_string = mp::format::state_string_for(status.state());

    EXPECT_THAT(state_string, Eq("Restarting"));
}

TEST(InstanceStateString, UNKNOWN_state_returns_Unknown)
{
    mp::InstanceStatus status;
    status.set_state(mp::InstanceState::UNKNOWN);
    auto state_string = mp::format::state_string_for(status.state());

    EXPECT_THAT(state_string, Eq("Unknown"));
}

TEST(InstanceStateString, STARTING_state_returns_Starting)
{
    mp::InstanceStatus status;
    status.set_state(mp::InstanceState::STARTING);
    auto state_string = mp::format::state_string_for(status.state());

    EXPECT_THAT(state_string, Eq("Starting"));
}

TEST(InstanceStateString, DELAYED_SHUTDOWN_state_returns_Delayed_Shutdown)
{
    mp::InstanceStatus status;
    status.set_state(mp::InstanceState::DELAYED_SHUTDOWN);
    auto state_string = mp::format::state_string_for(status.state());

    EXPECT_THAT(state_string, Eq("Delayed Shutdown"));
}

TEST(AliasFilter, unwanted_aliases_filtered_out)
{
    auto reply = mp::FindReply();
    auto image = reply.add_images_info();

    auto alias = image->add_aliases_info();
    alias->set_alias("ubuntu");
    alias = image->add_aliases_info();
    alias->set_alias("default");
    alias = image->add_aliases_info();
    alias->set_alias("devel");

    auto aliases = image->aliases_info();

    mp::format::filter_aliases(aliases);

    EXPECT_EQ(aliases.size(), 1);
    EXPECT_EQ(aliases[0].alias(), "devel");
}

TEST(AliasFilter, single_character_aliases_filtered_out)
{
    auto reply = mp::FindReply();
    auto image = reply.add_images_info();

    auto alias = image->add_aliases_info();
    alias->set_alias("a");
    alias = image->add_aliases_info();
    alias->set_alias("b");
    alias = image->add_aliases_info();
    alias->set_alias("devel");

    auto aliases = image->aliases_info();

    mp::format::filter_aliases(aliases);

    EXPECT_EQ(aliases.size(), 1);
    EXPECT_EQ(aliases[0].alias(), "devel");
}

TEST(AliasFilter, wanted_aliases_not_filtered_out)
{
    auto reply = mp::FindReply();
    auto image = reply.add_images_info();

    auto alias = image->add_aliases_info();
    alias->set_alias("lts");
    alias = image->add_aliases_info();
    alias->set_alias("devel");
    alias = image->add_aliases_info();
    alias->set_alias("eoan");

    auto aliases = image->aliases_info();

    mp::format::filter_aliases(aliases);

    EXPECT_THAT(aliases.size(), Eq(3));
    EXPECT_THAT(aliases[0].alias(), Eq("lts"));
    EXPECT_THAT(aliases[1].alias(), Eq("devel"));
    EXPECT_THAT(aliases[2].alias(), Eq("eoan"));
}

TEST(AliasFilter, mixed_aliases_filtered_out)
{
    auto reply = mp::FindReply();
    auto image = reply.add_images_info();

    auto alias = image->add_aliases_info();
    alias->set_alias("lts");
    alias = image->add_aliases_info();
    alias->set_alias("d");
    alias = image->add_aliases_info();
    alias->set_alias("eoan");
    alias = image->add_aliases_info();
    alias->set_alias("ubuntu");

    auto aliases = image->aliases_info();

    mp::format::filter_aliases(aliases);

    EXPECT_THAT(aliases.size(), Eq(2));
    EXPECT_THAT(aliases[0].alias(), Eq("lts"));
    EXPECT_THAT(aliases[1].alias(), Eq("eoan"));
}

TEST(AliasFilter, at_least_one_alias_left)
{
    auto reply = mp::FindReply();
    auto image = reply.add_images_info();

    auto alias = image->add_aliases_info();
    alias->set_alias("d");
    auto aliases = image->aliases_info();

    mp::format::filter_aliases(aliases);

    EXPECT_EQ(aliases.size(), 1);
    EXPECT_EQ(aliases[0].alias(), "d");
}
