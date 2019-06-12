/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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

#include <src/platform/backends/libvirt/libvirt_virtual_machine_factory.h>

#include "mock_libvirt.h"
#include "mock_status_monitor.h"
#include "stub_process_factory.h"
#include "stub_ssh_key_provider.h"
#include "stub_status_monitor.h"
#include "temp_dir.h"
#include "temp_file.h"

#include <multipass/memory_size.h>
#include <multipass/platform.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <cstdlib>

#include <gmock/gmock.h>

#include <multipass/format.h>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

namespace
{
template <typename T>
auto fake_handle()
{
    return reinterpret_cast<T>(0xDEADBEEF);
}
} // namespace

struct LibVirtBackend : public Test
{
    LibVirtBackend()
    {
        connect_close.returnValue(0);
        domain_free.returnValue(0);
        network_free.returnValue(0);
        leases.returnValue(0);
    }
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mpt::StubProcessFactory process_factory;
    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used
                                                      "pied-piper-valley",
                                                      "",
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name()};
    mpt::TempDir data_dir;

    decltype(MOCK(virConnectClose)) connect_close{MOCK(virConnectClose)};
    decltype(MOCK(virDomainFree)) domain_free{MOCK(virDomainFree)};
    decltype(MOCK(virNetworkFree)) network_free{MOCK(virNetworkFree)};
    decltype(MOCK(virNetworkGetDHCPLeases)) leases{MOCK(virNetworkGetDHCPLeases)};
};

TEST_F(LibVirtBackend, failed_connection_throws)
{
    REPLACE(virConnectOpen, [](auto...) { return nullptr; });
    EXPECT_THROW(mp::LibVirtVirtualMachineFactory backend(&process_factory, data_dir.path()), std::runtime_error);
}

TEST_F(LibVirtBackend, creates_in_off_state)
{
    REPLACE(virConnectOpen, [](auto...) { return fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });
    REPLACE(virDomainLookupByName, [](auto...) { return fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto...) { return VIR_DOMAIN_NOSTATE; });
    REPLACE(virDomainGetXMLDesc, [](auto...) {
        std::string domain_desc{"mac"};
        return strdup(domain_desc.c_str());
    });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{&process_factory, data_dir.path()};
    mpt::StubVMStatusMonitor stub_monitor;
    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(LibVirtBackend, creates_in_suspended_state_with_managed_save)
{
    REPLACE(virConnectOpen, [](auto...) { return fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });
    REPLACE(virDomainLookupByName, [](auto...) { return fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto...) { return VIR_DOMAIN_NOSTATE; });
    REPLACE(virDomainGetXMLDesc, [](auto...) {
        std::string domain_desc{"mac"};
        return strdup(domain_desc.c_str());
    });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 1; });

    mp::LibVirtVirtualMachineFactory backend{&process_factory, data_dir.path()};
    mpt::StubVMStatusMonitor stub_monitor;
    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::suspended));
}

TEST_F(LibVirtBackend, machine_sends_monitoring_events)
{
    REPLACE(virConnectOpen, [](auto...) { return fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });
    REPLACE(virDomainLookupByName, [](auto...) { return fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto...) { return VIR_DOMAIN_NOSTATE; });
    REPLACE(virDomainGetXMLDesc, [](auto...) {
        std::string domain_desc{"mac"};
        return strdup(domain_desc.c_str());
    });
    REPLACE(virDomainCreate, [](auto...) { return 0; });
    REPLACE(virDomainShutdown, [](auto...) { return 0; });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{&process_factory, data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    machine->state = mp::VirtualMachine::State::running;
    EXPECT_CALL(mock_monitor, on_shutdown());
    machine->shutdown();

    machine->state = mp::VirtualMachine::State::running;
    EXPECT_CALL(mock_monitor, on_suspend());
    machine->suspend();
}

TEST_F(LibVirtBackend, machine_persists_and_sets_state_on_start)
{
    REPLACE(virConnectOpen, [](auto...) { return fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });
    REPLACE(virDomainLookupByName, [](auto...) { return fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto...) { return VIR_DOMAIN_NOSTATE; });
    REPLACE(virDomainGetXMLDesc, [](auto...) {
        std::string domain_desc{"mac"};
        return strdup(domain_desc.c_str());
    });
    REPLACE(virDomainCreate, [](auto...) { return 0; });
    REPLACE(virDomainShutdown, [](auto...) { return 0; });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{&process_factory, data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->start();

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::starting));
}

TEST_F(LibVirtBackend, machine_persists_and_sets_state_on_shutdown)
{
    REPLACE(virConnectOpen, [](auto...) { return fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });
    REPLACE(virDomainLookupByName, [](auto...) { return fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto...) { return VIR_DOMAIN_NOSTATE; });
    REPLACE(virDomainGetXMLDesc, [](auto...) {
        std::string domain_desc{"mac"};
        return strdup(domain_desc.c_str());
    });
    REPLACE(virDomainCreate, [](auto...) { return 0; });
    REPLACE(virDomainShutdown, [](auto...) { return 0; });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{&process_factory, data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    machine->state = mp::VirtualMachine::State::running;
    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->shutdown();

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(LibVirtBackend, machine_persists_and_sets_state_on_suspend)
{
    REPLACE(virConnectOpen, [](auto...) { return fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });
    REPLACE(virDomainLookupByName, [](auto...) { return fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto...) { return VIR_DOMAIN_NOSTATE; });
    REPLACE(virDomainGetXMLDesc, [](auto...) {
        std::string domain_desc{"mac"};
        return strdup(domain_desc.c_str());
    });
    REPLACE(virDomainCreate, [](auto...) { return 0; });
    REPLACE(virDomainShutdown, [](auto...) { return 0; });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{&process_factory, data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    machine->state = mp::VirtualMachine::State::running;
    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->suspend();

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::suspended));
}

TEST_F(LibVirtBackend, machine_unknown_state_properly_shuts_down)
{
    REPLACE(virConnectOpen, [](auto...) { return fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });
    REPLACE(virDomainLookupByName, [](auto...) { return fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto...) { return VIR_DOMAIN_NOSTATE; });
    REPLACE(virDomainGetXMLDesc, [](auto...) {
        std::string domain_desc{"mac"};
        return strdup(domain_desc.c_str());
    });
    REPLACE(virDomainCreate, [](auto...) { return 0; });
    REPLACE(virDomainShutdown, [](auto...) { return 0; });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{&process_factory, data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    machine->state = mp::VirtualMachine::State::unknown;
    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->shutdown();

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}
