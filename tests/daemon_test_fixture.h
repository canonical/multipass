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

#ifndef MULTIPASS_DAEMON_TEST_FIXTURE_H
#define MULTIPASS_DAEMON_TEST_FIXTURE_H

// This include must go first because it comes from premock.
#include "mock_ssh_test_fixture.h"

#include "mock_virtual_machine_factory.h"
#include "temp_dir.h"

#include <src/daemon/daemon.h>
#include <src/daemon/daemon_config.h>

#include <multipass/rpc/multipass.grpc.pb.h>

#include <QEventLoop>
#include <QThread>

#include <memory>
#include <optional>
#include <unordered_map>

using namespace testing;
namespace mp = multipass;

namespace multipass
{
namespace test
{
struct DaemonTestFixture : public ::Test
{
    DaemonTestFixture();

    void SetUp() override;

    MockVirtualMachineFactory* use_a_mock_vm_factory();

    void send_command(const std::vector<std::string>& command, std::ostream& cout = trash_stream,
                      std::ostream& cerr = trash_stream, std::istream& cin = trash_stream);

    void send_commands(std::vector<std::vector<std::string>> commands, std::ostream& cout = trash_stream,
                       std::ostream& cerr = trash_stream, std::istream& cin = trash_stream);

    int total_lines_of_output(std::stringstream& output);

    std::string fake_json_contents(const std::string& default_mac,
                                   const std::vector<mp::NetworkInterface>& extra_ifaces,
                                   const std::unordered_map<std::string, mp::VMMount>& mounts = {});

    std::pair<std::unique_ptr<TempDir>, QString> // unique_ptr bypasses missing move ctor
    plant_instance_json(const std::string& contents);

    template <typename R>
    bool is_ready(std::future<R> const& f);

    /**
     * Helper function to call one of the <em>daemon slots</em> that ultimately handle RPC requests
     *  (e.g. @c mp::Daemon::get). It takes care of promise/future boilerplate. This will generally be given a
     *  @c mpt::MockServerReaderWriter, which can be used to verify replies.
     * @tparam DaemonSlotPtr The method pointer type for the provided slot. Example:
     *  @code
     *  void (mp::Daemon::*)(const mp::GetRequest *, grpc::ServerReaderWriterInterface<mp::GetReply> *,
     *                       std::promise<grpc::Status> *)>
     *  @endcode
     * @tparam Request The request type that the provided slot expects. Example: @c mp::GetRequest
     * @tparam Server The concrete @c grpc::ServerWriterInterface type that the provided slot expects. The template
     * needs to be instantiated with the correct reply type. Example: <tt> grpc::ServerWriterInterface\<mp\::GetReply\>
     * </tt>
     * @param daemon The daemon object to call the slot on.
     * @param slot A pointer to the daemon slot method that should be called.
     * @param request The request to call the slot with.
     * @param server The concrete @c grpc::ServerWriterInterface to call the slot with (see doc on Server typename).
     * This will generally be a @c mpt::MockServerWriter. Notice that this is a <em>universal reference</em>, so it will
     * bind to both lvalue and rvalue references.
     * @return The @c grpc::Status that is produced by the slot
     */
    template <typename DaemonSlotPtr, typename Request, typename Server>
    grpc::Status call_daemon_slot(Daemon& daemon, DaemonSlotPtr slot, const Request& request, Server&& server);

    MockSSHTestFixture mock_ssh_test_fixture;
#ifdef MULTIPASS_PLATFORM_WINDOWS
    std::string server_address{"localhost:50051"};
#else
    std::string server_address{"unix:/tmp/test-multipassd.socket"};
#endif
    QEventLoop loop; // needed as signal/slots used internally by mp::Daemon
    TempDir cache_dir;
    TempDir data_dir;
    DaemonConfigBuilder config_builder;
    inline static std::stringstream trash_stream{}; // this may have contents (that we don't care about)
};
} // namespace test
} // namespace multipass
#endif // MULTIPASS_DAEMON_TEST_FIXTURE_H
