/*
 * Copyright (C) 2020-2021 Canonical, Ltd.
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

#ifndef MULTIPASS_MOCK_NETWORK_ACCESS_MANAGER_H
#define MULTIPASS_MOCK_NETWORK_ACCESS_MANAGER_H

#include <multipass/network_access_manager.h>

#include "mock_local_socket_reply.h"

#include <gmock/gmock.h>

using namespace testing;

namespace multipass
{
namespace test
{
struct MockNetworkAccessManager : public NetworkAccessManager
{
    MockNetworkAccessManager()
    {
        ON_CALL(*this, createRequest).WillByDefault(Return(new MockLocalSocketReply{""}));
    }

    MOCK_METHOD3(createRequest, QNetworkReply*(Operation, const QNetworkRequest&, QIODevice*));
};
} // namespace test
} // namespace multipass

#endif // MULTIPASS_MOCK_NETWORK_ACCESS_MANAGER_H
