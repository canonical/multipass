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
 */

#include <client/cli/cmd/autocomplete_from_rpc_provider.h>
#include <tests/unit/mock_client_rpc.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <list>
#include <ranges>
#include <set>
#include <string_view>

namespace multipass::test
{

using namespace ::testing;

TEST(AutoCompleteFromRpcProvider, provideInstances)
{
    auto r1 = ListReply{};
    r1.mutable_instance_list()->add_instances()->set_name("i1");

    auto r2 = ListReply{};
    r2.mutable_instance_list()->add_instances()->set_name("i2");
    r2.mutable_instance_list()->add_instances()->set_name("i3");

    auto rpcMock = NiceMock<MockRpcStub>{};
    EXPECT_CALL(rpcMock, listRaw(_))
        .WillOnce(SuccessfullyReply<ListRequest>(r1))
        .WillOnce(FailWithStatus<ListRequest, ListReply>(grpc::StatusCode::UNKNOWN))
        .WillOnce(SuccessfullyReply<ListRequest>(r2));

    auto provider = cmd::AutoCompleteFromRpcProvider{&rpcMock};
    EXPECT_THAT(provider.provide(cmd::make_instances_request()), ElementsAre("i1"));
    EXPECT_THAT(provider.provide(cmd::make_instances_request()), ElementsAre());
    EXPECT_THAT(provider.provide(cmd::make_instances_request()), ElementsAre("i2", "i3"));
}

TEST(AutoCompleteFromRpcProvider, provideSnapshots)
{
    auto r1 = ListReply{};
    auto* ss1 = r1.mutable_snapshot_list()->add_snapshots();
    ss1->set_name("i1");
    ss1->mutable_fundamentals()->set_snapshot_name("ss1");
    auto* ss2 = r1.mutable_snapshot_list()->add_snapshots();
    ss2->set_name("i2");
    ss2->mutable_fundamentals()->set_snapshot_name("ss2");
    auto* ss3 = r1.mutable_snapshot_list()->add_snapshots();
    ss3->set_name("i3");
    ss3->mutable_fundamentals()->set_snapshot_name("ss3");

    auto r2 = ListReply{};
    auto* ss4 = r2.mutable_snapshot_list()->add_snapshots();
    ss4->set_name("i4");
    ss4->mutable_fundamentals()->set_snapshot_name("ss4");

    auto rpcMock = NiceMock<MockRpcStub>{};
    EXPECT_CALL(rpcMock, listRaw(_))
        .WillOnce(SuccessfullyReply<ListRequest>(r1))
        .WillOnce(FailWithStatus<ListRequest, ListReply>(grpc::StatusCode::UNKNOWN))
        .WillOnce(SuccessfullyReply<ListRequest>(r2));

    auto provider = cmd::AutoCompleteFromRpcProvider{&rpcMock};
    EXPECT_THAT(provider.provide(cmd::make_snapshots_request()),
                ElementsAre("i1.ss1", "i2.ss2", "i3.ss3"));
    EXPECT_THAT(provider.provide(cmd::make_snapshots_request()), ElementsAre());
    EXPECT_THAT(provider.provide(cmd::make_snapshots_request()), ElementsAre("i4.ss4"));
}

} // namespace multipass::test
