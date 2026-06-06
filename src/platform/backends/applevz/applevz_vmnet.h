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

#pragma once

#include <Virtualization/Virtualization.h>

#include <multipass/availability_zone.h>

#include <memory>

namespace multipass::applevz
{

// Opaque handle whose lifetime must exceed that of the attachment it was created with.
using VmnetRelayHandle = std::shared_ptr<void>;

struct VmnetBridge
{
    explicit VmnetBridge(const std::string& physical_iface);
    explicit VmnetBridge(multipass::AvailabilityZone& zone);

    VZFileHandleNetworkDeviceAttachment* attachment;
    VmnetRelayHandle relay;
};

} // namespace multipass::applevz
