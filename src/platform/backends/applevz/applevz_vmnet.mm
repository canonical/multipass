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

namespace multipass::applevz
{
VmnetRelay::~VmnetRelay()
{
    if (source)
        dispatch_source_cancel(source);

    if (iface)
    {
        dispatch_semaphore_t s = dispatch_semaphore_create(0);
        vmnet_stop_interface(iface, queue, ^(vmnet_return_t) {
          dispatch_semaphore_signal(s);
        });
        dispatch_semaphore_wait(s, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
    }

    if (fd >= 0)
        close(fd);
}
} // namespace multipass::applevz
