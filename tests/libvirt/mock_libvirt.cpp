/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "mock_libvirt.h"

extern "C"
{
    IMPL_MOCK_DEFAULT(1, virConnectOpen);
    IMPL_MOCK_DEFAULT(1, virConnectClose);
    IMPL_MOCK_DEFAULT(1, virConnectGetCapabilities);
    IMPL_MOCK_DEFAULT(2, virConnectGetVersion);
    IMPL_MOCK_DEFAULT(2, virNetworkLookupByName);
    IMPL_MOCK_DEFAULT(1, virNetworkFree);
    IMPL_MOCK_DEFAULT(2, virNetworkCreateXML);
    IMPL_MOCK_DEFAULT(1, virNetworkGetBridgeName);
    IMPL_MOCK_DEFAULT(1, virNetworkIsActive);
    IMPL_MOCK_DEFAULT(1, virNetworkCreate);
    IMPL_MOCK_DEFAULT(4, virNetworkGetDHCPLeases);
    IMPL_MOCK_DEFAULT(1, virNetworkDHCPLeaseFree);
    IMPL_MOCK_DEFAULT(1, virDomainUndefine);
    IMPL_MOCK_DEFAULT(2, virDomainLookupByName);
    IMPL_MOCK_DEFAULT(2, virDomainGetXMLDesc);
    IMPL_MOCK_DEFAULT(1, virDomainFree);
    IMPL_MOCK_DEFAULT(2, virDomainDefineXML);
    IMPL_MOCK_DEFAULT(4, virDomainGetState);
    IMPL_MOCK_DEFAULT(1, virDomainCreate);
    IMPL_MOCK_DEFAULT(1, virDomainShutdown);
    IMPL_MOCK_DEFAULT(2, virDomainManagedSave);
    IMPL_MOCK_DEFAULT(2, virDomainHasManagedSaveImage);
    IMPL_MOCK_DEFAULT(0, virGetLastError);
}
