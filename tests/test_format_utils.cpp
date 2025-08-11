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

#include <multipass/cli/format_utils.h>
#include <multipass/rpc/multipass.grpc.pb.h>

namespace mp = multipass;
using namespace testing;

TEST(InstanceStatusString, runningStatusReturnsRunning)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::RUNNING);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Running"));
}

TEST(InstanceStatusString, stoppedStatusReturnsStopped)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::STOPPED);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Stopped"));
}

TEST(InstanceStatusString, deletedStatusReturnsDeleted)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::DELETED);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Deleted"));
}

TEST(InstanceStatusString, suspendingStatusReturnsSuspending)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::SUSPENDING);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Suspending"));
}

TEST(InstanceStatusString, suspendedStatusReturnsSuspended)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::SUSPENDED);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Suspended"));
}

TEST(InstanceStatusString, restartingStatusReturnsRestarting)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::RESTARTING);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Restarting"));
}

TEST(InstanceStatusString, bogusStatusReturnsUnknown)
{
    mp::InstanceStatus status;
    status.set_status(static_cast<mp::InstanceStatus_Status>(46));
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Unknown"));
}

TEST(InstanceStatusString, startingStatusReturnsStarting)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::STARTING);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Starting"));
}

TEST(InstanceStatusString, delayedShutdownStatusReturnsDelayedShutdown)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::DELAYED_SHUTDOWN);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Delayed Shutdown"));
}

TEST(AliasFilter, unwantedAliasesFilteredOut)
{
    auto reply = mp::FindReply();
    auto image = reply.add_images_info();

    image->add_aliases("ubuntu");
    image->add_aliases("default");
    image->add_aliases("devel");

    auto aliases = image->aliases();

    mp::format::filter_aliases(aliases);

    EXPECT_EQ(aliases.size(), 1);
    EXPECT_EQ(aliases[0], "devel");
}

TEST(AliasFilter, singleCharacterAliasesFilteredOut)
{
    auto reply = mp::FindReply();
    auto image = reply.add_images_info();

    image->add_aliases("a");
    image->add_aliases("b");
    image->add_aliases("devel");

    auto aliases = image->aliases();

    mp::format::filter_aliases(aliases);

    EXPECT_EQ(aliases.size(), 1);
    EXPECT_EQ(aliases[0], "devel");
}

TEST(AliasFilter, wantedAliasesNotFilteredOut)
{
    auto reply = mp::FindReply();
    auto image = reply.add_images_info();

    image->add_aliases("lts");
    image->add_aliases("devel");
    image->add_aliases("eoan");

    auto aliases = image->aliases();

    mp::format::filter_aliases(aliases);

    EXPECT_THAT(aliases.size(), Eq(3));
    EXPECT_THAT(aliases[0], Eq("lts"));
    EXPECT_THAT(aliases[1], Eq("devel"));
    EXPECT_THAT(aliases[2], Eq("eoan"));
}

TEST(AliasFilter, mixedAliasesFilteredOut)
{
    auto reply = mp::FindReply();
    auto image = reply.add_images_info();

    image->add_aliases("lts");
    image->add_aliases("d");
    image->add_aliases("eoan");
    image->add_aliases("ubuntu");

    auto aliases = image->aliases();

    mp::format::filter_aliases(aliases);

    EXPECT_THAT(aliases.size(), Eq(2));
    EXPECT_THAT(aliases[0], Eq("lts"));
    EXPECT_THAT(aliases[1], Eq("eoan"));
}

TEST(AliasFilter, atLeastOneAliasLeft)
{
    auto reply = mp::FindReply();
    auto image = reply.add_images_info();

    image->add_aliases("d");
    image->add_aliases("");

    auto aliases = image->aliases();

    mp::format::filter_aliases(aliases);

    EXPECT_EQ(aliases.size(), 1);
    EXPECT_EQ(aliases[0], "d");
}

TEST(StaticFormatFunctions, columnWidthOnEmptyInputWorks)
{
    std::vector<std::string> empty_vector;
    const auto get_width = [](const auto& str) -> int { return str.length(); };
    int min_w = 3;

    ASSERT_EQ(
        mp::format::column_width(empty_vector.begin(), empty_vector.end(), get_width, 0, min_w),
        mp::format::col_buffer);
}

TEST(StaticFormatFunctions, columnWidthOnWideInputWorks)
{
    const std::string wider_str = "wide string example";

    const auto str_vector = std::vector<std::string>{wider_str, "w2"};
    const auto get_width = [](const auto& str) -> int { return str.length(); };
    int min_w = 3;
    int space = 1;

    ASSERT_EQ(
        mp::format::column_width(str_vector.begin(), str_vector.end(), get_width, space, min_w),
        wider_str.length() + mp::format::col_buffer);
}

TEST(StaticFormatFunctions, columnWidthOnNarrowInputWorks)
{
    const auto str_vector = std::vector<std::string>{"n", "n2"};
    const auto get_width = [](const auto& str) -> int { return str.length(); };
    int min_w = 7;
    int space = 2;

    ASSERT_EQ(
        mp::format::column_width(str_vector.begin(), str_vector.end(), get_width, space, min_w),
        min_w);
}
