/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#ifndef MULTIPASS_LIBVIRT_WRAPPER_H
#define MULTIPASS_LIBVIRT_WRAPPER_H

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

namespace multipass
{
class LibvirtWrapper
{
public:
    LibvirtWrapper();
    ~LibvirtWrapper();

private:
    void* handle;

public:
    virConnectPtr (*virConnectOpen)(const char* name);
    int (*virConnectClose)(virConnectPtr conn);
    char* (*virConnectGetCapabilities)(virConnectPtr conn);
    int (*virConnectGetVersion)(virConnectPtr conn, unsigned long* hvVer);
    virNetworkPtr (*virNetworkLookupByName)(virConnectPtr conn, const char* name);
    virNetworkPtr (*virNetworkCreateXML)(virConnectPtr conn, const char* xmlDesc);
    int (*virNetworkDestroy)(virNetworkPtr network);
    int (*virNetworkFree)(virNetworkPtr network);
    char* (*virNetworkGetBridgeName)(virNetworkPtr network);
    int (*virNetworkIsActive)(virNetworkPtr network);
    int (*virNetworkCreate)(virNetworkPtr network);
    int (*virNetworkGetDHCPLeases)(virNetworkPtr network, const char* mac, virNetworkDHCPLeasePtr** leases,
                                   unsigned int flags);
    void (*virNetworkDHCPLeaseFree)(virNetworkDHCPLeasePtr lease);
    int (*virDomainUndefine)(virDomainPtr domain);
    virDomainPtr (*virDomainLookupByName)(virConnectPtr conn, const char* name);
    char* (*virDomainGetXMLDesc)(virDomainPtr domain, unsigned int flags);
    int (*virDomainDestroy)(virDomainPtr domain);
    int (*virDomainFree)(virDomainPtr domain);
    virDomainPtr (*virDomainDefineXML)(virConnectPtr conn, const char* xml);
    int (*virDomainGetState)(virDomainPtr domain, int* state, int* reason, unsigned int flags);
    int (*virDomainCreate)(virDomainPtr domain);
    int (*virDomainShutdown)(virDomainPtr domain);
    int (*virDomainManagedSave)(virDomainPtr domain, unsigned int flags);
    int (*virDomainHasManagedSaveImage)(virDomainPtr domain, unsigned int flags);
    const char* (*virGetLastErrorMessage)(void);
};
} // namespace multipass

#endif // MULTIPASS_LIBVIRT_WRAPPER_H
