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

#ifndef MULTIPASS_LIBVIRT_WRAPPER_H
#define MULTIPASS_LIBVIRT_WRAPPER_H

#include <stdexcept>
#include <string>

#include <multipass/format.h>

#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

namespace multipass
{
class BaseLibvirtException : public std::runtime_error
{
public:
    BaseLibvirtException(const std::string& error_message) : runtime_error(error_message)
    {
    }
};

class LibvirtOpenException : public BaseLibvirtException
{
public:
    LibvirtOpenException(const char* error_message)
        : BaseLibvirtException(fmt::format("Failed to open the libvirt object: {}", error_message))
    {
    }
};

class LibvirtSymbolAddressException : public BaseLibvirtException
{
public:
    LibvirtSymbolAddressException(const std::string& symbol, const char* error_message)
        : BaseLibvirtException(fmt::format("Failed to load symbol \"{}\": {}", symbol, error_message))
    {
    }
};

class LibvirtWrapper
{
private:
    typedef virConnectPtr (*virConnectOpen_t)(const char* name);
    typedef int (*virConnectClose_t)(virConnectPtr conn);
    typedef char* (*virConnectGetCapabilities_t)(virConnectPtr conn);
    typedef int (*virConnectGetVersion_t)(virConnectPtr conn, unsigned long* hvVer);
    typedef virNetworkPtr (*virNetworkLookupByName_t)(virConnectPtr conn, const char* name);
    typedef virNetworkPtr (*virNetworkCreateXML_t)(virConnectPtr conn, const char* xmlDesc);
    typedef int (*virNetworkDestroy_t)(virNetworkPtr network);
    typedef int (*virNetworkFree_t)(virNetworkPtr network);
    typedef char* (*virNetworkGetBridgeName_t)(virNetworkPtr network);
    typedef int (*virNetworkIsActive_t)(virNetworkPtr network);
    typedef int (*virNetworkCreate_t)(virNetworkPtr network);
    typedef int (*virNetworkGetDHCPLeases_t)(virNetworkPtr network, const char* mac, virNetworkDHCPLeasePtr** leases,
                                             unsigned int flags);
    typedef void (*virNetworkDHCPLeaseFree_t)(virNetworkDHCPLeasePtr lease);
    typedef int (*virDomainUndefine_t)(virDomainPtr domain);
    typedef virDomainPtr (*virDomainLookupByName_t)(virConnectPtr conn, const char* name);
    typedef char* (*virDomainGetXMLDesc_t)(virDomainPtr domain, unsigned int flags);
    typedef int (*virDomainDestroy_t)(virDomainPtr domain);
    typedef int (*virDomainFree_t)(virDomainPtr domain);
    typedef virDomainPtr (*virDomainDefineXML_t)(virConnectPtr conn, const char* xml);
    typedef int (*virDomainGetState_t)(virDomainPtr domain, int* state, int* reason, unsigned int flags);
    typedef int (*virDomainCreate_t)(virDomainPtr domain);
    typedef int (*virDomainShutdown_t)(virDomainPtr domain);
    typedef int (*virDomainManagedSave_t)(virDomainPtr domain, unsigned int flags);
    typedef int (*virDomainHasManagedSaveImage_t)(virDomainPtr domain, unsigned int flags);
    typedef int (*virDomainSetVcpusFlags_t)(virDomainPtr domain, unsigned int nvcpus, unsigned int flags);
    typedef int (*virDomainSetMemoryFlags_t)(virDomainPtr domain, unsigned long memory, unsigned int flags);
    typedef const char* (*virGetLastErrorMessage_t)();

    void* handle{nullptr};

public:
    using UPtr = std::unique_ptr<LibvirtWrapper>;

    LibvirtWrapper(const std::string& filename = "libvirt.so.0");
    ~LibvirtWrapper();

    virConnectOpen_t virConnectOpen;
    virConnectClose_t virConnectClose;
    virConnectGetCapabilities_t virConnectGetCapabilities;
    virConnectGetVersion_t virConnectGetVersion;
    virNetworkLookupByName_t virNetworkLookupByName;
    virNetworkCreateXML_t virNetworkCreateXML;
    virNetworkDestroy_t virNetworkDestroy;
    virNetworkFree_t virNetworkFree;
    virNetworkGetBridgeName_t virNetworkGetBridgeName;
    virNetworkIsActive_t virNetworkIsActive;
    virNetworkCreate_t virNetworkCreate;
    virNetworkGetDHCPLeases_t virNetworkGetDHCPLeases;
    virNetworkDHCPLeaseFree_t virNetworkDHCPLeaseFree;
    virDomainUndefine_t virDomainUndefine;
    virDomainLookupByName_t virDomainLookupByName;
    virDomainGetXMLDesc_t virDomainGetXMLDesc;
    virDomainDestroy_t virDomainDestroy;
    virDomainFree_t virDomainFree;
    virDomainDefineXML_t virDomainDefineXML;
    virDomainGetState_t virDomainGetState;
    virDomainCreate_t virDomainCreate;
    virDomainShutdown_t virDomainShutdown;
    virDomainManagedSave_t virDomainManagedSave;
    virDomainHasManagedSaveImage_t virDomainHasManagedSaveImage;
    virDomainSetVcpusFlags_t virDomainSetVcpusFlags;
    virDomainSetMemoryFlags_t virDomainSetMemoryFlags;
    virGetLastErrorMessage_t virGetLastErrorMessage;
};
} // namespace multipass

#endif // MULTIPASS_LIBVIRT_WRAPPER_H
