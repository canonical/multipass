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

#ifndef MULTIPASS_QEMU_PLATFORM_H
#define MULTIPASS_QEMU_PLATFORM_H

#include <multipass/disabled_copy_move.h>
#include <multipass/exceptions/not_implemented_on_this_backend_exception.h>
#include <multipass/ip_address.h>
#include <multipass/network_interface_info.h>
#include <multipass/path.h>
#include <multipass/singleton.h>
#include <multipass/virtual_machine_description.h>

#include <QString>
#include <QStringList>

#include <memory>
#include <optional>
#include <string>

namespace multipass
{
class QemuPlatform : private DisabledCopyMove
{
public:
    using UPtr = std::unique_ptr<QemuPlatform>;

    virtual ~QemuPlatform() = default;

    virtual std::optional<IPAddress> get_ip_for(const std::string& hw_addr) = 0;
    virtual void remove_resources_for(const std::string&) = 0;
    virtual void platform_health_check() = 0;
    virtual QStringList vmstate_platform_args()
    {
        return {};
    };
    virtual QStringList vm_platform_args(const VirtualMachineDescription& vm_desc) = 0;
    virtual QString get_directory_name() const
    {
        return {};
    };
    virtual bool is_network_supported(const std::string& network_type) const = 0;
    virtual bool needs_network_prep() const = 0;
    virtual std::string create_bridge_with(const NetworkInterfaceInfo& interface) const = 0;
    virtual void set_authorization(std::vector<NetworkInterfaceInfo>& networks) = 0;

protected:
    explicit QemuPlatform() = default;
};

#define MP_QEMU_PLATFORM_FACTORY multipass::QemuPlatformFactory::instance()

class QemuPlatformFactory : public Singleton<QemuPlatformFactory>
{
public:
    QemuPlatformFactory(const Singleton<QemuPlatformFactory>::PrivatePass& pass) noexcept
        : Singleton<QemuPlatformFactory>::Singleton{pass} {};

    virtual QemuPlatform::UPtr make_qemu_platform(const Path& data_dir) const;
};
} // namespace multipass
#endif // MULTIPASS_QEMU_PLATFORM_H
