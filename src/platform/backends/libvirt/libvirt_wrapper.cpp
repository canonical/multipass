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

#include "libvirt_wrapper.h"

#include <dlfcn.h>

namespace mp = multipass;

namespace
{
auto open_libvirt_handle()
{
    auto handle = dlopen("libvirt.so", RTLD_NOW | RTLD_GLOBAL);

    return handle;
}
} // namespace

mp::LibvirtWrapper::LibvirtWrapper()
    : handle{open_libvirt_handle()},
      virConnectOpen{reinterpret_cast<virConnectPtr (*)(const char*)>(dlsym(handle, "virConnectOpen"))},
      virConnectClose{reinterpret_cast<int (*)(virConnectPtr)>(dlsym(handle, "virConnectClose"))},
      virConnectGetCapabilities{reinterpret_cast<char* (*)(virConnectPtr)>(dlsym(handle, "virConnectGetCapabilities"))},
      virConnectGetVersion{
          reinterpret_cast<int (*)(virConnectPtr, unsigned long*)>(dlsym(handle, "virConnectGetVersion"))},
      virNetworkLookupByName{
          reinterpret_cast<virNetworkPtr (*)(virConnectPtr, const char*)>(dlsym(handle, "virNetworkLookupByName"))},
      virNetworkCreateXML{
          reinterpret_cast<virNetworkPtr (*)(virConnectPtr, const char*)>(dlsym(handle, "virNetworkCreateXML"))},
      virNetworkDestroy{reinterpret_cast<int (*)(virNetworkPtr)>(dlsym(handle, "virNetworkDestroy"))},
      virNetworkFree{reinterpret_cast<int (*)(virNetworkPtr)>(dlsym(handle, "virNetworkFree"))},
      virNetworkGetBridgeName{reinterpret_cast<char* (*)(virNetworkPtr)>(dlsym(handle, "virNetworkGetBridgeName"))},
      virNetworkIsActive{reinterpret_cast<int (*)(virNetworkPtr)>(dlsym(handle, "virNetworkIsActive"))},
      virNetworkCreate{reinterpret_cast<int (*)(virNetworkPtr)>(dlsym(handle, "virNetworkCreate"))},
      virNetworkGetDHCPLeases{
          reinterpret_cast<int (*)(virNetworkPtr, const char*, virNetworkDHCPLeasePtr**, unsigned int)>(
              dlsym(handle, "virNetworkGetDHCPLeases"))},
      virNetworkDHCPLeaseFree{
          reinterpret_cast<void (*)(virNetworkDHCPLeasePtr)>(dlsym(handle, "virNetworkDHCPLeaseFree"))},
      virDomainUndefine{reinterpret_cast<int (*)(virDomainPtr)>(dlsym(handle, "virDomainUndefine"))},
      virDomainLookupByName{
          reinterpret_cast<virDomainPtr (*)(virConnectPtr, const char*)>(dlsym(handle, "virDomainLookupByName"))},
      virDomainGetXMLDesc{
          reinterpret_cast<char* (*)(virDomainPtr, unsigned int)>(dlsym(handle, "virDomainGetXMLDesc"))},
      virDomainDestroy{reinterpret_cast<int (*)(virDomainPtr)>(dlsym(handle, "virDomainDestroy"))},
      virDomainFree{reinterpret_cast<int (*)(virDomainPtr)>(dlsym(handle, "virDomainFree"))},
      virDomainDefineXML{
          reinterpret_cast<virDomainPtr (*)(virConnectPtr, const char*)>(dlsym(handle, "virDomainDefineXML"))},
      virDomainGetState{
          reinterpret_cast<int (*)(virDomainPtr, int*, int*, unsigned int)>(dlsym(handle, "virDomainGetState"))},
      virDomainCreate{reinterpret_cast<int (*)(virDomainPtr)>(dlsym(handle, "virDomainCreate"))},
      virDomainShutdown{reinterpret_cast<int (*)(virDomainPtr)>(dlsym(handle, "virDomainShutdown"))},
      virDomainManagedSave{
          reinterpret_cast<int (*)(virDomainPtr, unsigned int)>(dlsym(handle, "virDomainManagedSave"))},
      virDomainHasManagedSaveImage{
          reinterpret_cast<int (*)(virDomainPtr, unsigned int)>(dlsym(handle, "virDomainHasManagedSaveImage"))},
      virGetLastErrorMessage{reinterpret_cast<const char* (*)(void)>(dlsym(handle, "virGetLastErrorMessage"))}
{
}

mp::LibvirtWrapper::~LibvirtWrapper()
{
    dlclose(handle);
}
