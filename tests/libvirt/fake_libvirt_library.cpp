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

#include "tests/fake_handle.h"

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

#include <stdlib.h>
#include <string.h>

namespace mpt = multipass::test;

/*
 * These are default fake libvirt functions for testing.  They return the most
 * common values needed for testing.
 * See test_libvirt_backend.cpp for examples on how to override these functions
 * using LibvirtWrapper.
 */

virConnectPtr virConnectOpen(const char* name)
{
    return mpt::fake_handle<virConnectPtr>();
}

int virConnectClose(virConnectPtr conn)
{
    return 0;
}

char* virConnectGetCapabilities(virConnectPtr /*conn*/)
{
    return strdup("");
}

int virConnectGetVersion(virConnectPtr /*conn*/, unsigned long* hvVer)
{
    *hvVer = 0;
    return 0;
}

int virDomainCreate(virDomainPtr /*domain*/)
{
    return 0;
}

virDomainPtr virDomainDefineXML(virConnectPtr /*conn*/, const char* /*xml*/)
{
    return mpt::fake_handle<virDomainPtr>();
}

int virDomainDestroy(virDomainPtr /*domain*/)
{
    return 0;
}

int virDomainFree(virDomainPtr /*domain*/)
{
    return 0;
}

int virDomainGetState(virDomainPtr /*domain*/, int* state, int* /*reason*/, unsigned int /*flags*/)
{
    *state = VIR_DOMAIN_SHUTOFF;

    return 0;
}

char* virDomainGetXMLDesc(virDomainPtr /*domain*/, unsigned int /*flags*/)
{
    return strdup("mac");
}

int virDomainHasManagedSaveImage(virDomainPtr /*domain*/, unsigned int /*flags*/)
{
    return 0;
}

virDomainPtr virDomainLookupByName(virConnectPtr /*conn*/, const char* /*name*/)
{
    return mpt::fake_handle<virDomainPtr>();
}

int virDomainManagedSave(virDomainPtr /*domain*/, unsigned int /*flags*/)
{
    return 0;
}

int virDomainShutdown(virDomainPtr /*domain*/)
{
    return 0;
}

int virDomainUndefine(virDomainPtr /*domain*/)
{
    return 0;
}

int virNetworkCreate(virNetworkPtr /*network*/)
{
    return 0;
}

virNetworkPtr virNetworkCreateXML(virConnectPtr /*conn*/, const char* /*xmlDesc*/)
{
    return mpt::fake_handle<virNetworkPtr>();
}

int virNetworkDestroy(virNetworkPtr /*network*/)
{
    return 0;
}

void virNetworkDHCPLeaseFree(virNetworkDHCPLeasePtr lease)
{
    free(lease);
}

int virNetworkFree(virNetworkPtr /*network*/)
{
    return 0;
}

char* virNetworkGetBridgeName(virNetworkPtr /*network*/)
{
    return strdup("mpvirt0");
}

int virNetworkGetDHCPLeases(virNetworkPtr /*network*/, const char* /*mac*/, virNetworkDHCPLeasePtr** /*leases*/,
                            unsigned int /*flags*/)
{
    return 0;
}

int virNetworkIsActive(virNetworkPtr /*network*/)
{
    return 1;
}

virNetworkPtr virNetworkLookupByName(virConnectPtr /*conn*/, const char* /*name*/)
{
    return mpt::fake_handle<virNetworkPtr>();
}

const char* virGetLastErrorMessage()
{
    static char fake_error[64] = "";
    return fake_error;
}

int virDomainSetVcpusFlags(virDomainPtr /*domain*/, unsigned int /*nvcpus*/, unsigned int /*flags*/)
{
    return 1;
}

int virDomainSetMemoryFlags(virDomainPtr /*domain*/, unsigned long /*memory*/, unsigned int /*flags*/)
{
    return 1;
}
