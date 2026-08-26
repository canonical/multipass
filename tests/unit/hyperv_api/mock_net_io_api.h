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

#include <hyperv_api/net_io_api.h>

#include "tests/unit/mock_singleton_helpers.h"

namespace multipass::test
{

class MockNetIOAPI : public hyperv::NetIOAPI
{
public:
    using NetIOAPI::NetIOAPI;

    MOCK_METHOD(void, InitializeIpInterfaceEntry, (PMIB_IPINTERFACE_ROW Row), (const override));
    MOCK_METHOD(DWORD, SetIpInterfaceEntry, (PMIB_IPINTERFACE_ROW Row), (const override));
    MOCK_METHOD(DWORD,
                ConvertInterfaceAliasToLuid,
                (const WCHAR* InterfaceName, NET_LUID* InterfaceLuid),
                (const override));

    MP_MOCK_SINGLETON_BOILERPLATE(MockNetIOAPI, NetIOAPI);
};
} // namespace multipass::test
