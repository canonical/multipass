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

#include <multipass/rpc/multipass.pb.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace multipass::test
{

::testing::Matcher<SnapshotsReply> Snapshots(const auto& inner_matcher)
{
    return ::testing::Property(&SnapshotsReply::snapshot_list,
                               ::testing::Property(&SnapshotsList::snapshots, inner_matcher));
}

::testing::Matcher<ListVMSnapshot> SnapshotInstance(const auto& inner_matcher)
{
    return ::testing::Property(&ListVMSnapshot::name, inner_matcher);
}

::testing::Matcher<ListVMSnapshot> SnapshotName(const auto& inner_matcher)
{
    return ::testing::Property(
        &ListVMSnapshot::fundamentals,
        ::testing::Property(&SnapshotFundamentals::snapshot_name, inner_matcher));
}

::testing::Matcher<ListVMSnapshot> SnapshotParent(const auto& inner_matcher)
{
    return ::testing::Property(&ListVMSnapshot::fundamentals,
                               ::testing::Property(&SnapshotFundamentals::parent, inner_matcher));
}

::testing::Matcher<ListVMSnapshot> SnapshotComment(const auto& inner_matcher)
{
    return ::testing::Property(&ListVMSnapshot::fundamentals,
                               ::testing::Property(&SnapshotFundamentals::comment, inner_matcher));
}

} // namespace multipass::test
