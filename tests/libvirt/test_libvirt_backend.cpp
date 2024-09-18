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

#include "tests/common.h"
#include "tests/fake_handle.h"
#include "tests/mock_backend_utils.h"
#include "tests/mock_logger.h"
#include "tests/mock_ssh.h"
#include "tests/mock_status_monitor.h"
#include "tests/stub_ssh_key_provider.h"
#include "tests/stub_status_monitor.h"
#include "tests/temp_dir.h"
#include "tests/temp_file.h"

#include <src/platform/backends/libvirt/libvirt_virtual_machine_factory.h>

#include <multipass/auto_join_thread.h>
#include <multipass/exceptions/start_exception.h>
#include <multipass/memory_size.h>
#include <multipass/network_interface_info.h>
#include <multipass/virtual_machine.h>
#include <multipass/virtual_machine_description.h>

#include <cstdlib>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;
using namespace std::chrono_literals;

struct LibVirtBackend : public Test
{
    mpt::TempFile dummy_image;
    mpt::TempFile dummy_cloud_init_iso;
    mp::VirtualMachineDescription default_description{2,
                                                      mp::MemorySize{"3M"},
                                                      mp::MemorySize{}, // not used
                                                      "pied-piper-valley",
                                                      "",
                                                      {},
                                                      "ubuntu",
                                                      {dummy_image.name(), "", "", "", "", {}},
                                                      dummy_cloud_init_iso.name(),
                                                      {},
                                                      {},
                                                      {},
                                                      {}};
    mpt::TempDir data_dir;
    mpt::StubSSHKeyProvider key_provider;

    // This indicates that LibvirtWrapper should open the test executable
    std::string fake_libvirt_path{""};

    mpt::MockLogger::Scope logger_scope{mpt::MockLogger::inject()};

    mpt::MockBackend::GuardedMock backend_attr{mpt::MockBackend::inject<NiceMock>()};
    mpt::MockBackend* mock_backend = backend_attr.first;
};

TEST_F(LibVirtBackend, libvirt_wrapper_missing_libvirt_throws)
{
    EXPECT_THROW(mp::LibvirtWrapper{"missing_libvirt"}, mp::LibvirtOpenException);
}

TEST_F(LibVirtBackend, libvirt_wrapper_missing_symbol_throws)
{
    // Need to set LD_LIBRARY_PATH to this .so for the multipass_tests executable
    EXPECT_THROW(mp::LibvirtWrapper{"libbroken_libvirt.so"}, mp::LibvirtSymbolAddressException);
}

TEST_F(LibVirtBackend, health_check_good_does_not_throw)
{
    EXPECT_CALL(*mock_backend, check_for_kvm_support()).WillOnce(Return());
    EXPECT_CALL(*mock_backend, check_if_kvm_is_in_use()).WillOnce(Return());

    mp::LibVirtVirtualMachineFactory backend(data_dir.path(), fake_libvirt_path);

    EXPECT_NO_THROW(backend.hypervisor_health_check());
}

TEST_F(LibVirtBackend, health_check_failed_connection_throws)
{
    const std::string error_msg{"Not working"};

    EXPECT_CALL(*mock_backend, check_for_kvm_support()).WillOnce(Return());
    EXPECT_CALL(*mock_backend, check_if_kvm_is_in_use()).WillOnce(Return());

    auto virGetLastErrorMessage = [&error_msg] { return error_msg.c_str(); };
    static auto static_virGetLastErrorMessage = virGetLastErrorMessage;

    mp::LibVirtVirtualMachineFactory backend(data_dir.path(), fake_libvirt_path);
    backend.libvirt_wrapper->virConnectOpen = [](auto...) -> virConnectPtr { return nullptr; };
    backend.libvirt_wrapper->virGetLastErrorMessage = [] { return static_virGetLastErrorMessage(); };

    MP_EXPECT_THROW_THAT(
        backend.hypervisor_health_check(), std::runtime_error,
        mpt::match_what(StrEq(fmt::format(
            "Cannot connect to libvirtd: {}\nPlease ensure libvirt is installed and running.", error_msg))));
}

TEST_F(LibVirtBackend, creates_in_off_state)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    mpt::StubVMStatusMonitor stub_monitor;

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(LibVirtBackend, creates_in_suspended_state_with_managed_save)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virDomainHasManagedSaveImage = [](auto...) { return 1; };

    mpt::StubVMStatusMonitor stub_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::suspended));
}

TEST_F(LibVirtBackend, machine_sends_monitoring_events)
{
    REPLACE(ssh_connect, [](auto...) { return SSH_OK; });
    REPLACE(ssh_userauth_publickey, [](auto...) { return SSH_AUTH_SUCCESS; });

    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virNetworkGetDHCPLeases = [](auto, auto, auto leases, auto) {
        virNetworkDHCPLeasePtr* leases_ret;
        leases_ret = (virNetworkDHCPLeasePtr*)calloc(1, sizeof(virNetworkDHCPLeasePtr));
        leases_ret[0] = (virNetworkDHCPLeasePtr)calloc(1, sizeof(virNetworkDHCPLease));
        leases_ret[0]->ipaddr = strdup("0.0.0.0");
        *leases = leases_ret;

        return 1;
    };

    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_CALL(mock_monitor, on_resume());
    machine->start();

    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    };

    machine->wait_until_ssh_up(2min);

    EXPECT_CALL(mock_monitor, on_shutdown());
    machine->shutdown();

    EXPECT_CALL(mock_monitor, on_suspend());
    machine->suspend();
}

TEST_F(LibVirtBackend, machine_persists_and_sets_state_on_start)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->start();

    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    };

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::starting));
}

TEST_F(LibVirtBackend, machine_persists_and_sets_state_on_shutdown)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    };

    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->shutdown();

    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_SHUTOFF;
        return 0;
    };
    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(LibVirtBackend, machine_persists_and_sets_state_on_suspend)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    };

    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_CALL(mock_monitor, persist_state_for(_, _));
    machine->suspend();

    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_SHUTOFF;
        return 0;
    };
    backend.libvirt_wrapper->virDomainHasManagedSaveImage = [](auto...) { return 1; };

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::suspended));
}

TEST_F(LibVirtBackend, start_with_broken_libvirt_connection_throws)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virConnectOpen = [](auto...) -> virConnectPtr { return nullptr; };

    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_THROW(machine->start(), std::runtime_error);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::unknown));
}

TEST_F(LibVirtBackend, shutdown_with_broken_libvirt_connection_throws)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virConnectOpen = [](auto...) -> virConnectPtr { return nullptr; };

    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_THROW(machine->shutdown(), std::runtime_error);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::unknown));
}

TEST_F(LibVirtBackend, suspend_with_broken_libvirt_connection_throws)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virConnectOpen = [](auto...) -> virConnectPtr { return nullptr; };

    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_THROW(machine->suspend(), std::runtime_error);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::unknown));
}

TEST_F(LibVirtBackend, current_state_with_broken_libvirt_unknown)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virConnectOpen = [](auto...) -> virConnectPtr { return nullptr; };

    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::unknown));
}

TEST_F(LibVirtBackend, current_state_delayed_shutdown_domain_running)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    };

    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);
    machine->state = mp::VirtualMachine::State::delayed_shutdown;

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::delayed_shutdown));
}

TEST_F(LibVirtBackend, current_state_delayed_shutdown_domain_off)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);
    machine->state = mp::VirtualMachine::State::delayed_shutdown;

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));
}

TEST_F(LibVirtBackend, current_state_off_domain_starts_running)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));

    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    };

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::running));
}

TEST_F(LibVirtBackend, returns_version_string)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virConnectGetVersion = [](virConnectPtr, unsigned long* hvVer) {
        *hvVer = 1002003;
        return 0;
    };

    EXPECT_EQ(backend.get_backend_version_string(), "libvirt-1.2.3");
}

TEST_F(LibVirtBackend, returns_version_string_when_error)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virConnectGetVersion = [](auto...) { return -1; };

    EXPECT_EQ(backend.get_backend_version_string(), "libvirt-unknown");
}

TEST_F(LibVirtBackend, returns_version_string_when_lacking_capabilities)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};

    EXPECT_EQ(backend.get_backend_version_string(), "libvirt-unknown");
}

TEST_F(LibVirtBackend, returns_version_string_when_failed_connecting)
{
    auto called{0};
    auto virConnectGetVersion = [&called](auto...) {
        ++called;
        return 0;
    };

    // Need this to "fake out" not being able to assign lambda's that capture directly
    // to a pointer to a function.
    static auto static_virConnectGetVersion = virConnectGetVersion;

    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    backend.libvirt_wrapper->virConnectOpen = [](auto...) -> virConnectPtr { return nullptr; };
    backend.libvirt_wrapper->virConnectGetVersion = [](virConnectPtr conn, long unsigned int* hwVer) {
        return static_virConnectGetVersion(conn, hwVer);
    };

    EXPECT_EQ(backend.get_backend_version_string(), "libvirt-unknown");
    EXPECT_EQ(called, 0);
}

TEST_F(LibVirtBackend, ssh_hostname_returns_expected_value)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};

    const std::string expected_ip{"10.10.0.34"};
    auto virNetworkGetDHCPLeases = [&expected_ip](auto, auto, auto leases, auto) {
        virNetworkDHCPLeasePtr* leases_ret;
        leases_ret = (virNetworkDHCPLeasePtr*)calloc(1, sizeof(virNetworkDHCPLeasePtr));
        leases_ret[0] = (virNetworkDHCPLeasePtr)calloc(1, sizeof(virNetworkDHCPLease));
        leases_ret[0]->ipaddr = strdup(expected_ip.c_str());
        *leases = leases_ret;

        return 1;
    };

    static auto static_virNetworkGetDHCPLeases = virNetworkGetDHCPLeases;

    backend.libvirt_wrapper->virNetworkGetDHCPLeases = [](virNetworkPtr network, const char* mac,
                                                          virNetworkDHCPLeasePtr** leases, unsigned int flags) {
        return static_virNetworkGetDHCPLeases(network, mac, leases, flags);
    };

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);
    machine->start();

    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    };

    EXPECT_EQ(machine->VirtualMachine::ssh_hostname(), expected_ip);
}

TEST_F(LibVirtBackend, ssh_hostname_timeout_throws_and_sets_unknown_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};

    backend.libvirt_wrapper->virNetworkGetDHCPLeases = [](auto...) { return 0; };

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);
    machine->start();

    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    };

    EXPECT_THROW(machine->ssh_hostname(std::chrono::milliseconds(1)), std::runtime_error);
    EXPECT_EQ(machine->state, mp::VirtualMachine::State::unknown);
}

TEST_F(LibVirtBackend, shutdown_while_starting_throws_and_sets_correct_state)
{
    mpt::StubVMStatusMonitor stub_monitor;
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};

    auto destroy_called{false};
    auto virDomainDestroy = [&backend, &destroy_called](auto...) {
        destroy_called = true;

        backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
            *state = VIR_DOMAIN_SHUTOFF;
            return 0;
        };

        return 0;
    };

    static auto static_virDomainDestroy = virDomainDestroy;

    backend.libvirt_wrapper->virDomainDestroy = [](virDomainPtr domain) { return static_virDomainDestroy(domain); };

    auto machine = backend.create_virtual_machine(default_description, key_provider, stub_monitor);

    machine->start();

    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    };

    ASSERT_EQ(machine->state, mp::VirtualMachine::State::starting);

    mp::AutoJoinThread thread = [&machine] { machine->shutdown(mp::VirtualMachine::ShutdownPolicy::Poweroff); };

    using namespace std::chrono_literals;
    while (!destroy_called)
        std::this_thread::sleep_for(1ms);

    MP_EXPECT_THROW_THAT(machine->ensure_vm_is_running(), mp::StartException,
                         mpt::match_what(StrEq("Instance failed to start")));

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::off);
}

TEST_F(LibVirtBackend, machineInOffStateLogsAndIgnoresShutdown)
{
    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    EXPECT_THAT(machine->current_state(), Eq(mp::VirtualMachine::State::off));

    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_SHUTOFF;
        return 0;
    };

    logger_scope.mock_logger->screen_logs(mpl::Level::info);
    logger_scope.mock_logger->expect_log(mpl::Level::info, "Ignoring shutdown since instance is already stopped.");

    machine->shutdown();

    EXPECT_EQ(machine->current_state(), mp::VirtualMachine::State::off);
}

TEST_F(LibVirtBackend, machineNoForceCannotShutdownLogsAndThrows)
{
    const std::string error_msg{"Not working"};

    mp::LibVirtVirtualMachineFactory backend{data_dir.path(), fake_libvirt_path};
    NiceMock<mpt::MockVMStatusMonitor> mock_monitor;
    auto machine = backend.create_virtual_machine(default_description, key_provider, mock_monitor);

    backend.libvirt_wrapper->virDomainGetState = [](auto, auto state, auto, auto) {
        *state = VIR_DOMAIN_RUNNING;
        return 0;
    };

    backend.libvirt_wrapper->virDomainShutdown = [](auto) { return -1; };

    auto virGetLastErrorMessage = [&error_msg] { return error_msg.c_str(); };
    static auto static_virGetLastErrorMessage = virGetLastErrorMessage;
    backend.libvirt_wrapper->virGetLastErrorMessage = [] { return static_virGetLastErrorMessage(); };

    logger_scope.mock_logger->screen_logs(mpl::Level::warning);
    logger_scope.mock_logger->expect_log(mpl::Level::warning, error_msg);

    MP_EXPECT_THROW_THAT(machine->shutdown(),
                         std::runtime_error,
                         mpt::match_what(AllOf(HasSubstr("pied-piper-valley"), HasSubstr(error_msg))));
}

TEST_F(LibVirtBackend, lists_no_networks)
{
    mp::LibVirtVirtualMachineFactory backend(data_dir.path(), fake_libvirt_path);

    EXPECT_THROW(backend.networks(), mp::NotImplementedOnThisBackendException);
}
