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

#include "libvirt_wrapper.h"

#include <dlfcn.h>

namespace mp = multipass;

namespace
{
auto open_libvirt_handle(const std::string& filename)
{
    // If filename is empty, it's for testing, ie, opening the test executable itself
    auto handle = dlopen(filename.empty() ? nullptr : filename.c_str(), RTLD_NOW | RTLD_GLOBAL);

    if (!handle)
        throw mp::LibvirtOpenException(dlerror());

    return handle;
}

auto get_symbol_address_for(const std::string& symbol, void* handle)
{
    auto address = dlsym(handle, symbol.c_str());

    if (!address)
    {
        std::string error{dlerror()};
        dlclose(handle);

        throw mp::LibvirtSymbolAddressException(symbol, error.c_str());
    }

    return address;
}
} // namespace

mp::LibvirtWrapper::LibvirtWrapper(const std::string& filename)
    : handle{open_libvirt_handle(filename)},
      virConnectOpen{reinterpret_cast<virConnectOpen_t>(get_symbol_address_for("virConnectOpen", handle))},
      virConnectClose{reinterpret_cast<virConnectClose_t>(get_symbol_address_for("virConnectClose", handle))},
      virConnectGetCapabilities{
          reinterpret_cast<virConnectGetCapabilities_t>(get_symbol_address_for("virConnectGetCapabilities", handle))},
      virConnectGetVersion{
          reinterpret_cast<virConnectGetVersion_t>(get_symbol_address_for("virConnectGetVersion", handle))},
      virNetworkLookupByName{
          reinterpret_cast<virNetworkLookupByName_t>(get_symbol_address_for("virNetworkLookupByName", handle))},
      virNetworkCreateXML{
          reinterpret_cast<virNetworkCreateXML_t>(get_symbol_address_for("virNetworkCreateXML", handle))},
      virNetworkDestroy{reinterpret_cast<virNetworkDestroy_t>(get_symbol_address_for("virNetworkDestroy", handle))},
      virNetworkFree{reinterpret_cast<virNetworkFree_t>(get_symbol_address_for("virNetworkFree", handle))},
      virNetworkGetBridgeName{
          reinterpret_cast<virNetworkGetBridgeName_t>(get_symbol_address_for("virNetworkGetBridgeName", handle))},
      virNetworkIsActive{reinterpret_cast<virNetworkIsActive_t>(get_symbol_address_for("virNetworkIsActive", handle))},
      virNetworkCreate{reinterpret_cast<virNetworkCreate_t>(get_symbol_address_for("virNetworkCreate", handle))},
      virNetworkGetDHCPLeases{
          reinterpret_cast<virNetworkGetDHCPLeases_t>(get_symbol_address_for("virNetworkGetDHCPLeases", handle))},
      virNetworkDHCPLeaseFree{
          reinterpret_cast<virNetworkDHCPLeaseFree_t>(get_symbol_address_for("virNetworkDHCPLeaseFree", handle))},
      virDomainUndefine{reinterpret_cast<virDomainUndefine_t>(get_symbol_address_for("virDomainUndefine", handle))},
      virDomainLookupByName{
          reinterpret_cast<virDomainLookupByName_t>(get_symbol_address_for("virDomainLookupByName", handle))},
      virDomainGetXMLDesc{
          reinterpret_cast<virDomainGetXMLDesc_t>(get_symbol_address_for("virDomainGetXMLDesc", handle))},
      virDomainDestroy{reinterpret_cast<virDomainDestroy_t>(get_symbol_address_for("virDomainDestroy", handle))},
      virDomainFree{reinterpret_cast<virDomainFree_t>(get_symbol_address_for("virDomainFree", handle))},
      virDomainDefineXML{reinterpret_cast<virDomainDefineXML_t>(get_symbol_address_for("virDomainDefineXML", handle))},
      virDomainGetState{reinterpret_cast<virDomainGetState_t>(get_symbol_address_for("virDomainGetState", handle))},
      virDomainCreate{reinterpret_cast<virDomainCreate_t>(get_symbol_address_for("virDomainCreate", handle))},
      virDomainShutdown{reinterpret_cast<virDomainShutdown_t>(get_symbol_address_for("virDomainShutdown", handle))},
      virDomainManagedSave{
          reinterpret_cast<virDomainManagedSave_t>(get_symbol_address_for("virDomainManagedSave", handle))},
      virDomainHasManagedSaveImage{reinterpret_cast<virDomainHasManagedSaveImage_t>(
          get_symbol_address_for("virDomainHasManagedSaveImage", handle))},
      virDomainSetVcpusFlags{
          reinterpret_cast<virDomainSetVcpusFlags_t>(get_symbol_address_for("virDomainSetVcpusFlags", handle))},
      virDomainSetMemoryFlags{
          reinterpret_cast<virDomainSetMemoryFlags_t>(get_symbol_address_for("virDomainSetMemoryFlags", handle))},
      virGetLastErrorMessage{
          reinterpret_cast<virGetLastErrorMessage_t>(get_symbol_address_for("virGetLastErrorMessage", handle))}
{
}

mp::LibvirtWrapper::~LibvirtWrapper()
{
    dlclose(handle);
}
