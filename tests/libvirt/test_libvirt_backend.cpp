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

#include "tests/fake_handle.h"
#include "tests/libvirt/mock_libvirt.h"
#include "tests/mock_ssh.h"
#include "tests/mock_status_monitor.h"
#include "tests/stub_process_factory.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_dir.h"
#include "tests/temp_file.h"

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
using namespace std::chrono_literals;

struct LibVirtBackend : public Test
{
    LibVirtBackend()
    {
        connect_close.returnValue(0);
        domain_xml_desc.returnValue(strdup("mac"));
        domain_create.returnValue(0);
        domain_shutdown.returnValue(0);
        domain_free.returnValue(0);
        network_free.returnValue(0);
        leases.returnValue(0);
        network_active.returnValue(1);
        network_by_name.returnValue(mpt::fake_handle<virNetworkPtr>());
        bridge_name.returnValue(strdup("mpvirt0"));
        last_error.returnValue(&error);
    }
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used
                                                      "pied-piper-valley",
                                                      "",
                                                      "",
                                                      {dummy_image.name(), "", "", "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name()};
    mpt::TempDir data_dir;
    virError error{0, 0, nullptr, VIR_ERR_WARNING, nullptr, nullptr, nullptr, nullptr, nullptr, 0, 0, nullptr};

    decltype(MOCK(virConnectClose)) connect_close{MOCK(virConnectClose)};
    decltype(MOCK(virDomainGetXMLDesc)) domain_xml_desc{MOCK(virDomainGetXMLDesc)};
    decltype(MOCK(virDomainCreate)) domain_create{MOCK(virDomainCreate)};
    decltype(MOCK(virDomainShutdown)) domain_shutdown{MOCK(virDomainShutdown)};
    decltype(MOCK(virDomainFree)) domain_free{MOCK(virDomainFree)};
    decltype(MOCK(virNetworkFree)) network_free{MOCK(virNetworkFree)};
    decltype(MOCK(virNetworkGetDHCPLeases)) leases{MOCK(virNetworkGetDHCPLeases)};
    decltype(MOCK(virNetworkIsActive)) network_active{MOCK(virNetworkIsActive)};
    decltype(MOCK(virNetworkLookupByName)) network_by_name{MOCK(virNetworkLookupByName)};
    decltype(MOCK(virNetworkGetBridgeName)) bridge_name{MOCK(virNetworkGetBridgeName)};
    decltype(MOCK(virGetLastError)) last_error{MOCK(virGetLastError)};
};

TEST_F(LibVirtBackend, health_check_failed_connection_throws)
{
    REPLACE(virConnectOpen, [](auto...) { return nullptr; });
    mp::LibVirtVirtualMachineFactory backend(data_dir.path());

    EXPECT_THROW(backend.hypervisor_health_check(), std::runtime_error);
}

TEST_F(LibVirtBackend, creates_in_off_state)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virDomainLookupByName, [](auto...) { return mpt::fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_SHUTOFF;
        return 0;
    });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    mpt::StubVMStatusMonitor stub_monitor;
    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(LibVirtBackend, creates_in_suspended_state_with_managed_save)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virDomainLookupByName, [](auto...) { return mpt::fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_SHUTOFF;
        return 0;
    });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 1; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    mpt::StubVMStatusMonitor stub_monitor;
    auto machine = backend.create_virtual_machine(default_description, stub_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::suspended));
}

TEST_F(LibVirtBackend, machine_sends_monitoring_events)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virDomainLookupByName, [](auto...) { return mpt::fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_SHUTOFF;
        return 0;
    });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    REPLACE(virNetworkGetDHCPLeases, [](auto, auto, auto leases, auto) {
        virNetworkDHCPLeasePtr* leases_ret;
        leases_ret = (virNetworkDHCPLeasePtr*)calloc(1, sizeof(virNetworkDHCPLeasePtr));
        leases_ret[0] = (virNetworkDHCPLeasePtr)calloc(1, sizeof(virNetworkDHCPLease));
        leases_ret[0]->ipaddr = strdup("0.0.0.0");
        *leases = leases_ret;

        return 1;
    });

    REPLACE(ssh_connect, [](auto...) { return SSH_OK; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    });

    machine->wait_until_ssh_up(2min);

    EXPECT_CALL(mock_monitor, on_shutdown());
    machine->shutdown();

    EXPECT_CALL(mock_monitor, on_suspend());
    machine->suspend();
}

TEST_F(LibVirtBackend, machine_persists_and_sets_state_on_start)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virDomainLookupByName, [](auto...) { return mpt::fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_SHUTOFF;
        return 0;
    });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->start();

    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    });

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::starting));
}

TEST_F(LibVirtBackend, machine_persists_and_sets_state_on_shutdown)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virDomainLookupByName, [](auto...) { return mpt::fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->shutdown();

    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_SHUTOFF;
        return 0;
    });
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(LibVirtBackend, machine_persists_and_sets_state_on_suspend)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virDomainLookupByName, [](auto...) { return mpt::fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->suspend();
    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_SHUTOFF;
        return 0;
    });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 1; });

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::suspended));
}

TEST_F(LibVirtBackend, start_with_broken_libvirt_connection_throws)
{
    REPLACE(virConnectOpen, [](auto...) { return nullptr; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_THROW(machine->start(), std::runtime_error);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::unknown));
}

TEST_F(LibVirtBackend, shutdown_with_broken_libvirt_connection_throws)
{
    REPLACE(virConnectOpen, [](auto...) { return nullptr; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_THROW(machine->shutdown(), std::runtime_error);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::unknown));
}

TEST_F(LibVirtBackend, suspend_with_broken_libvirt_connection_throws)
{
    REPLACE(virConnectOpen, [](auto...) { return nullptr; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_THROW(machine->suspend(), std::runtime_error);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::unknown));
}

TEST_F(LibVirtBackend, current_state_with_broken_libvirt_unknown)
{
    REPLACE(virConnectOpen, [](auto...) { return nullptr; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::unknown));
}

TEST_F(LibVirtBackend, current_state_delayed_shutdown_domain_running)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virDomainLookupByName, [](auto...) { return mpt::fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);
    machine->state = mp::VirtualMachine::State::delayed_shutdown;

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::delayed_shutdown));
}

TEST_F(LibVirtBackend, current_state_delayed_shutdown_domain_off)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virDomainLookupByName, [](auto...) { return mpt::fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_SHUTOFF;
        return 0;
    });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);
    machine->state = mp::VirtualMachine::State::delayed_shutdown;

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(LibVirtBackend, current_state_off_domain_starts_running)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virDomainLookupByName, [](auto...) { return mpt::fake_handle<virDomainPtr>(); });
    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_SHUTOFF;
        return 0;
    });
    REPLACE(virDomainManagedSave, [](auto...) { return 0; });
    REPLACE(virDomainHasManagedSaveImage, [](auto...) { return 0; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, mock_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));

    REPLACE(virDomainGetState, [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    });

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::running));
}

TEST_F(LibVirtBackend, returns_version_string)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return mpt::fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });

    REPLACE(virConnectGetVersion, [](virConnectPtr, unsigned long* hvVer) {
        *hvVer = 1002003;
        return 0;
    });
    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};

    EXPECT_EQ(backend.get_backend_version_string(), "libvirt-1.2.3");
}

TEST_F(LibVirtBackend, returns_version_string_when_error)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return mpt::fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });

    REPLACE(virConnectGetVersion, [](auto...) { return -1; });
    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};

    EXPECT_EQ(backend.get_backend_version_string(), "libvirt-unknown");
}

TEST_F(LibVirtBackend, returns_version_string_when_lacking_capabilities)
{
    REPLACE(virConnectOpen, [](auto...) { return mpt::fake_handle<virConnectPtr>(); });
    REPLACE(virNetworkLookupByName, [](auto...) { return mpt::fake_handle<virNetworkPtr>(); });
    REPLACE(virNetworkGetBridgeName, [](auto...) {
        std::string bridge_name{"mpvirt0"};
        return strdup(bridge_name.c_str());
    });
    REPLACE(virNetworkIsActive, [](auto...) { return 1; });

    REPLACE(virConnectGetVersion, [](virConnectPtr, unsigned long* hvVer) {
        *hvVer = 0;
        return 0;
    });
    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};

    EXPECT_EQ(backend.get_backend_version_string(), "libvirt-unknown");
}

TEST_F(LibVirtBackend, returns_version_string_when_failed_connecting)
{
    REPLACE(virConnectOpen, [](auto...) { return nullptr; });
    auto m = MOCK(virConnectGetVersion);

    mp::LibVirtVirtualMachineFactory backend{data_dir.path()};

    EXPECT_EQ(backend.get_backend_version_string(), "libvirt-unknown");
    m.expectCalled(0);
}
