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

#include <vmnet/vmnet.h>

namespace
{
constexpr uint32_t kDefaultMaxPacketBytes = 1518;

struct VmnetRelay
{
    interface_ref iface{nullptr};
    int fd{-1};
    dispatch_source_t source{nullptr};
    dispatch_queue_t queue{nullptr};
    uint32_t max_packet_bytes{kDefaultMaxPacketBytes};

    ~VmnetRelay()
    {
        if (source)
            dispatch_source_cancel(source);

        if (iface)
        {
            dispatch_semaphore_t s = dispatch_semaphore_create(0);
            vmnet_stop_interface(iface,
                                 dispatch_get_global_queue(QOS_CLASS_UTILITY, 0),
                                 ^(vmnet_return_t) {
                                   dispatch_semaphore_signal(s);
                                 });
            dispatch_semaphore_wait(s, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
        }

        if (fd >= 0)
            close(fd);
    }

    VmnetRelay() = default;
    VmnetRelay(const VmnetRelay&) = delete;
    VmnetRelay& operator=(const VmnetRelay&) = delete;
};
} // namespace

namespace multipass::applevz
{
} // namespace multipass::applevz
