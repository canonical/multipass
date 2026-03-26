/*
 * Copyright (C) The vmnet-helper authors
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

/*
   Portions of this file are derived from vmnet-helper by Nir Soffer
   (https://github.com/nirs/vmnet-helper), originally licensed under the
   Apache License, Version 2.0.  The combined work is distributed under
   the terms of GPL-3.0.

   The following adaptations were made for use with Multipass:
   - rewritten in idiomatic Objective-C++ and integrated as a library;
   - connected to Apple Virtualization framework via a socketpair and
     VZFileHandleNetworkDeviceAttachment;
   - removed unused network options
   - Multipass logging;
 */

#include <applevz/applevz_utils.h>
#include <applevz/applevz_vmnet.h>
#include <multipass/logging/log.h>

#include <fmt/format.h>

#include <array>
#include <chrono>
#include <thread>

#include <errno.h>
#include <vmnet/vmnet.h>

namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "vmnet";
constexpr uint32_t kSendBufferSize{65 * 1024};
constexpr uint32_t kRecvBufferSize{4 * 1024 * 1024};
constexpr int kMaxPacketCount{200};
constexpr int kMaxPacketCountLarge{16};
constexpr uint32_t kLargePacketThreshold{64 * 1024};
constexpr uint64_t kSendRetryDelayNs{50 * 1000};

struct msghdr_x
{
    struct msghdr msg_hdr;
    size_t msg_len;
};
extern "C" ssize_t recvmsg_x(int s, const msghdr_x* msgp, unsigned int cnt, int flags);
extern "C" ssize_t sendmsg_x(int s, const msghdr_x* msgp, unsigned int cnt, int flags);

struct VmnetRelay
{
    interface_ref iface{nullptr};
    int fd{-1};
    dispatch_queue_t queue{nullptr};
    dispatch_queue_t vm_queue{nullptr};
    uint32_t max_packet_bytes{0};

    // Pre-allocated buffers for host->vm (vmnet read / socket write).
    std::vector<uint8_t> host_buffers;
    std::vector<vmpktdesc> host_packets;
    std::vector<iovec> host_iovs;
    std::vector<msghdr_x> host_msgs;

    // Pre-allocated buffers for vm->host (socket read / vmnet write).
    std::vector<uint8_t> vm_buffers;
    std::vector<vmpktdesc> vm_packets;
    std::vector<iovec> vm_iovs;
    std::vector<msghdr_x> vm_msgs;

    void init_buffers()
    {
        const int packet_count =
            (max_packet_bytes >= kLargePacketThreshold) ? kMaxPacketCountLarge : kMaxPacketCount;

        auto bind_endpoint = [&](std::vector<uint8_t>& buffers,
                                 std::vector<vmpktdesc>& packets,
                                 std::vector<iovec>& iovs,
                                 std::vector<msghdr_x>& msgs) {
            buffers.resize((size_t)packet_count * max_packet_bytes);
            packets.resize(packet_count);
            iovs.resize(packet_count);
            msgs.resize(packet_count);
            for (int i = 0; i < packet_count; i++)
            {
                iovs[i].iov_base = buffers.data() + (size_t)i * max_packet_bytes;
                iovs[i].iov_len = max_packet_bytes;
                packets[i].vm_pkt_iov = &iovs[i];
                packets[i].vm_pkt_iovcnt = 1;
                msgs[i].msg_hdr.msg_iov = &iovs[i];
                msgs[i].msg_hdr.msg_iovlen = 1;
            }
        };

        bind_endpoint(host_buffers, host_packets, host_iovs, host_msgs);
        bind_endpoint(vm_buffers, vm_packets, vm_iovs, vm_msgs);
    }

    ~VmnetRelay()
    {
        if (fd >= 0)
            close(fd);

        // Wait for the vm->host forwarding loop to exit before stopping vmnet.
        if (vm_queue)
            dispatch_sync(vm_queue,
                          ^{
                          });

        if (iface)
        {
            dispatch_semaphore_t s = dispatch_semaphore_create(0);
            vmnet_stop_interface(iface,
                                 dispatch_get_global_queue(QOS_CLASS_UTILITY, 0),
                                 ^(vmnet_return_t status) {
                                   if (status != VMNET_SUCCESS)
                                       mpl::warn(category,
                                                 "vmnet_stop_interface() failed (status {})",
                                                 static_cast<int>(status));
                                   dispatch_semaphore_signal(s);
                                 });
            if (dispatch_semaphore_wait(s, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)) != 0)
                mpl::warn(category, "timed out waiting for vmnet_stop_interface() to complete");
        }
    }

    VmnetRelay() = default;
    VmnetRelay(const VmnetRelay&) = delete;
    VmnetRelay& operator=(const VmnetRelay&) = delete;
};

void start_vmnet_interface(VmnetRelay& relay, const std::string& physical_iface)
{
    relay.queue = dispatch_queue_create(
        fmt::format("com.canonical.multipassd.vmnet.{}", physical_iface).c_str(),
        DISPATCH_QUEUE_SERIAL);

    xpc_object_t iface_opts = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_dictionary_set_uint64(iface_opts, vmnet_operation_mode_key, VMNET_BRIDGED_MODE);
    xpc_dictionary_set_string(iface_opts, vmnet_shared_interface_name_key, physical_iface.c_str());

    dispatch_semaphore_t vmnet_sema = dispatch_semaphore_create(0);
    __block vmnet_return_t vmnet_status = VMNET_FAILURE;

    // Starting the vmnet interface requires root on macOS 15 and older
    relay.iface = vmnet_start_interface(
        iface_opts,
        relay.queue,
        ^(vmnet_return_t status, xpc_object_t params) {
          vmnet_status = status;
          if (status == VMNET_SUCCESS)
              relay.max_packet_bytes =
                  (uint32_t)xpc_dictionary_get_uint64(params, vmnet_max_packet_size_key);
          dispatch_semaphore_signal(vmnet_sema);
        });

    dispatch_semaphore_wait(vmnet_sema, DISPATCH_TIME_FOREVER);

    if (vmnet_status != VMNET_SUCCESS || !relay.iface)
    {
        throw std::runtime_error(fmt::format("vmnet_start_interface() failed (status {}) for '{}'",
                                             static_cast<int>(vmnet_status),
                                             physical_iface));
    }

    mpl::debug(category, "vmnet bridged interface started on '{}'", physical_iface);
}

// Forward packets from the socket to the vmnet interface.
// Runs as a blocking loop on a dedicated queue until the socket is closed.
void forward_from_vm(VmnetRelay& relay, bool bulk)
{
    for (;;)
    {
        int count = 0;

        if (bulk)
        {
            int max_count = (int)relay.vm_msgs.size();
            for (int i = 0; i < max_count; i++)
                relay.vm_iovs[i].iov_len = relay.max_packet_bytes;

            count = (int)recvmsg_x(relay.fd, relay.vm_msgs.data(), max_count, 0);
            if (count < 0 && errno != EBADF && errno != ECONNRESET)
                mpl::trace(category, "recvmsg_x() failed: {}", strerror(errno));
        }
        else
        {
            relay.vm_iovs[0].iov_len = relay.max_packet_bytes;
            ssize_t n = recv(relay.fd, relay.vm_iovs[0].iov_base, relay.max_packet_bytes, 0);
            if (n < 0 && errno != EBADF && errno != ECONNRESET)
                mpl::trace(category, "recv() failed: {}", strerror(errno));
            if (n > 0)
            {
                relay.vm_msgs[0].msg_len = (size_t)n;
                count = 1;
            }
        }

        if (count <= 0)
            break;

        for (int i = 0; i < count; i++)
        {
            relay.vm_packets[i].vm_pkt_size = relay.vm_msgs[i].msg_len;
            relay.vm_packets[i].vm_flags = 0;
            relay.vm_iovs[i].iov_len = relay.vm_msgs[i].msg_len;
        }

        int write_count = count;
        vmnet_return_t status = vmnet_write(relay.iface, relay.vm_packets.data(), &write_count);
        if (status != VMNET_SUCCESS)
            mpl::trace(category, "vmnet_write() failed (status {})", static_cast<int>(status));
    }
}

// Forward one batch of packets from the vmnet interface to the socket.
// Returns false when vmnet has no more packets to deliver this callback.
bool forward_from_host(VmnetRelay& relay, bool bulk)
{
    int count = (int)relay.host_packets.size();
    for (int i = 0; i < count; i++)
    {
        relay.host_packets[i].vm_pkt_size = relay.max_packet_bytes;
        relay.host_packets[i].vm_flags = 0;
        relay.host_iovs[i].iov_len = relay.max_packet_bytes;
    }

    vmnet_return_t read_status = vmnet_read(relay.iface, relay.host_packets.data(), &count);
    if (read_status != VMNET_SUCCESS)
    {
        mpl::trace(category, "vmnet_read() failed (status {})", static_cast<int>(read_status));
        return false;
    }

    if (bulk)
    {
        for (int i = 0; i < count; i++)
        {
            relay.host_msgs[i].msg_len = relay.host_packets[i].vm_pkt_size;
            relay.host_iovs[i].iov_len = relay.host_packets[i].vm_pkt_size;
        }

        int sent = 0;
        while (sent < count)
        {
            ssize_t n = sendmsg_x(relay.fd, relay.host_msgs.data() + sent, count - sent, 0);
            if (n < 0)
            {
                if (errno == ENOBUFS)
                {
                    std::this_thread::sleep_for(std::chrono::nanoseconds{kSendRetryDelayNs});
                    continue;
                }
                mpl::trace(category, "sendmsg_x() failed: {}", strerror(errno));
                break;
            }
            sent += (int)n;
        }
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            const auto* data = static_cast<const uint8_t*>(relay.host_iovs[i].iov_base);
            size_t pkt_size = relay.host_packets[i].vm_pkt_size;
            ssize_t sent;
            do
            {
                sent = send(relay.fd, data, pkt_size, 0);
                if (sent < 0 && errno == ENOBUFS)
                    std::this_thread::sleep_for(std::chrono::nanoseconds{kSendRetryDelayNs});
            } while (sent < 0 && errno == ENOBUFS);

            if (sent < 0)
                mpl::trace(category, "send() failed: {}", strerror(errno));
        }
    }

    return count > 0;
}

void start_forwarding_from_vm(VmnetRelay& relay)
{
    const bool bulk = MP_APPLEVZ_UTILS.macos_at_least(14, 0);

    relay.vm_queue =
        dispatch_queue_create("com.canonical.multipassd.vmnet.vm", DISPATCH_QUEUE_SERIAL);
    dispatch_async(relay.vm_queue, ^{
      forward_from_vm(relay, bulk);
    });

    mpl::debug(category, "vmnet vm->host forwarding started");
}

void start_forwarding_from_host(VmnetRelay& relay)
{
    const bool bulk = MP_APPLEVZ_UTILS.macos_at_least(14, 0);

    vmnet_return_t status =
        vmnet_interface_set_event_callback(relay.iface,
                                           VMNET_INTERFACE_PACKETS_AVAILABLE,
                                           relay.queue,
                                           ^(interface_event_t /*event*/, xpc_object_t /*params*/) {
                                             while (forward_from_host(relay, bulk))
                                                 ;
                                           });

    if (status != VMNET_SUCCESS)
        throw std::runtime_error(
            fmt::format("vmnet_interface_set_event_callback() failed (status {})",
                        static_cast<int>(status)));

    mpl::debug(category, "vmnet host->vm forwarding started");
}

std::pair<int, int> create_socket_pair()
{
    std::array<int, 2> fds{};
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds.data()) < 0)
        throw std::runtime_error(fmt::format("socketpair() failed: {}", strerror(errno)));

    setsockopt(fds[0], SOL_SOCKET, SO_RCVBUF, &kRecvBufferSize, sizeof(kRecvBufferSize));
    setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &kSendBufferSize, sizeof(kSendBufferSize));
    setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &kRecvBufferSize, sizeof(kRecvBufferSize));
    setsockopt(fds[1], SOL_SOCKET, SO_SNDBUF, &kSendBufferSize, sizeof(kSendBufferSize));

    return {fds[0], fds[1]};
}
} // namespace

namespace multipass::applevz
{
VmnetBridge create_vmnet_bridge(const std::string& physical_iface)
{
    auto relay = std::make_unique<VmnetRelay>();
    start_vmnet_interface(*relay, physical_iface);

    auto [vz_fd, relay_fd] = create_socket_pair();

    relay->fd = relay_fd;
    relay->init_buffers();

    start_forwarding_from_host(*relay);
    start_forwarding_from_vm(*relay);

    NSFileHandle* vz_handle = [[NSFileHandle alloc] initWithFileDescriptor:vz_fd
                                                            closeOnDealloc:YES];
    VZFileHandleNetworkDeviceAttachment* attachment =
        [[VZFileHandleNetworkDeviceAttachment alloc] initWithFileHandle:vz_handle];

    return VmnetBridge{attachment, std::move(relay)};
}
} // namespace multipass::applevz
