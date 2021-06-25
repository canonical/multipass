/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include <multipass/ip_address.h>
#include <multipass/optional.h>
#include <multipass/path.h>

#include <QString>
#include <QStringList>

#include <memory>
#include <string>

namespace multipass
{
class QemuPlatform
{
public:
    using UPtr = std::unique_ptr<QemuPlatform>;

    virtual ~QemuPlatform() = default;

    virtual optional<IPAddress> get_ip_for(const std::string& hw_addr) = 0;
    virtual void remove_resources_for(const std::string&){};
    virtual void platform_health_check() = 0;
    virtual QString qemu_netdev(const std::string& name, const std::string& hw_addr) = 0;
    virtual QStringList qemu_platform_args()
    {
        return {};
    };

protected:
    explicit QemuPlatform() = default;
    QemuPlatform(const QemuPlatform&) = delete;
    QemuPlatform& operator=(const QemuPlatform&) = delete;
};

QemuPlatform::UPtr make_qemu_platform(const Path& data_dir);
} // namespace multipass
#endif // MULTIPASS_QEMU_PLATFORM_H
