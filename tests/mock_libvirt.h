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

#ifndef MULTIPASS_MOCK_LIBVIRT_H
#define MULTIPASS_MOCK_LIBVIRT_H

#include <premock.hpp>

#include <libvirt/libvirt.h>

DECL_MOCK(virConnectOpen);
DECL_MOCK(virConnectClose);
DECL_MOCK(virConnectGetCapabilities);
DECL_MOCK(virConnectGetVersion);
DECL_MOCK(virNetworkLookupByName);
DECL_MOCK(virNetworkFree);
DECL_MOCK(virNetworkCreateXML);
DECL_MOCK(virNetworkGetBridgeName);
DECL_MOCK(virNetworkIsActive);
DECL_MOCK(virNetworkCreate);
DECL_MOCK(virNetworkGetDHCPLeases);
DECL_MOCK(virNetworkDHCPLeaseFree);
DECL_MOCK(virDomainUndefine);
DECL_MOCK(virDomainLookupByName);
DECL_MOCK(virDomainGetXMLDesc);
DECL_MOCK(virDomainFree);
DECL_MOCK(virDomainDefineXML);
DECL_MOCK(virDomainGetState);
DECL_MOCK(virDomainCreate);
DECL_MOCK(virDomainShutdown);
DECL_MOCK(virDomainManagedSave);
DECL_MOCK(virDomainHasManagedSaveImage);

#endif // MULTIPASS_MOCK_LIBVIRT_H
