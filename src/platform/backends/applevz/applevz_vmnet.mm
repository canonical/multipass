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

#include <applevz/applevz_vmnet.h>
#include <multipass/logging/log.h>

#include <fmt/format.h>

#include <errno.h>
#include <vmnet/vmnet.h>

namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "vmnet";
constexpr uint32_t kSendBufferSize{65 * 1024};
constexpr uint32_t kRecvBufferSize{4 * 1024 * 1024};

struct VmnetRelay
{
    interface_ref iface{nullptr};
    int fd{-1};
    dispatch_source_t source{nullptr};
    dispatch_queue_t queue{nullptr};
    uint32_t max_packet_bytes{0};

    ~VmnetRelay()
    {
        if (source)
            dispatch_source_cancel(source);

        if (iface)
        {
            dispatch_semaphore_t s = dispatch_semaphore_create(0);
            vmnet_stop_interface(iface,
                                 dispatch_get_global_queue(QOS_CLASS_UTILITY, 0),
                                 ^(vmnet_return_t status) {
                                   if (status != VMNET_SUCCESS)
                                       mpl::info(category,
                                                 "vmnet_stop_interface() failed (status {})",
                                                 static_cast<int>(status));
                                   dispatch_semaphore_signal(s);
                                 });
            if (dispatch_semaphore_wait(s, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)) != 0)
                mpl::info(category, "timed out waiting for vmnet_stop_interface() to complete");
        }

        if (fd >= 0)
            close(fd);
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

void setup_relay(VmnetRelay& relay, int relay_fd)
{
    relay.fd = relay_fd;

    const auto iface = relay.iface;
    const auto max_packet_bytes = relay.max_packet_bytes;

    // read from socket, write to vmnet
    relay.source =
        dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)relay_fd, 0, relay.queue);

    dispatch_source_set_event_handler(relay.source, ^{
      std::vector<uint8_t> buf(max_packet_bytes);
      ssize_t n = recv(relay_fd, buf.data(), buf.size(), 0);
      if (n <= 0)
          return;

      struct iovec iov{buf.data(), (size_t)n};
      struct vmpktdesc pkt{};
      pkt.vm_pkt_size = (size_t)n;
      pkt.vm_pkt_iov = &iov;
      pkt.vm_pkt_iovcnt = 1;
      pkt.vm_flags = 0;

      int count = 1;
      vmnet_write(iface, &pkt, &count);
    });
    dispatch_resume(relay.source);

    // read from vmnet, write to socket
    vmnet_interface_set_event_callback(iface,
                                       VMNET_INTERFACE_PACKETS_AVAILABLE,
                                       relay.queue,
                                       ^(interface_event_t /*event*/, xpc_object_t /*params*/) {
                                         for (;;)
                                         {
                                             std::vector<uint8_t> buf(max_packet_bytes);
                                             struct iovec iov{buf.data(), buf.size()};
                                             struct vmpktdesc pkt{};
                                             pkt.vm_pkt_size = buf.size();
                                             pkt.vm_pkt_iov = &iov;
                                             pkt.vm_pkt_iovcnt = 1;
                                             pkt.vm_flags = 0;

                                             int count = 1;
                                             if (vmnet_read(iface, &pkt, &count) != VMNET_SUCCESS ||
                                                 count == 0)
                                                 break;

                                             send(relay_fd, buf.data(), pkt.vm_pkt_size, 0);
                                         }
                                       });
}

std::pair<int, int> create_socket_pair()
{
    int fds[2];
    if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) < 0)
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
    setup_relay(*relay, relay_fd);

    NSFileHandle* vz_handle = [[NSFileHandle alloc] initWithFileDescriptor:vz_fd
                                                            closeOnDealloc:YES];
    VZFileHandleNetworkDeviceAttachment* attachment =
        [[VZFileHandleNetworkDeviceAttachment alloc] initWithFileHandle:vz_handle];

    return VmnetBridge{attachment, std::move(relay)};
}
} // namespace multipass::applevz
