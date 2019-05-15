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

TEST(InstanceStatusString, running_status_returns_RUNNING)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::RUNNING);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Running"));
}

TEST(InstanceStatusString, stopped_status_returns_STOPPED)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::STOPPED);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Stopped"));
}

TEST(InstanceStatusString, deleted_status_returns_DELETED)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::DELETED);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Deleted"));
}

TEST(InstanceStatusString, suspending_status_returns_SUSPENDING)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::SUSPENDING);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Suspending"));
}

TEST(InstanceStatusString, suspended_status_returns_SUSPENDED)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::SUSPENDED);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Suspended"));
}

TEST(InstanceStatusString, running_status_returns_RESTARTING)
{
    mp::InstanceStatus status;
    status.set_status(mp::InstanceStatus::RESTARTING);
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Restarting"));
}

TEST(InstanceStatusString, bogus_status_returns_UNKNOWN)
{
    mp::InstanceStatus status;
    status.set_status(static_cast<mp::InstanceStatus_Status>(10));
    auto status_string = mp::format::status_string_for(status);

    EXPECT_THAT(status_string, Eq("Unknown"));
}
