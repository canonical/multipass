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

#ifndef MULTIPASS_QEMU_PLATFORM_DETAIL_H
#define MULTIPASS_QEMU_PLATFORM_DETAIL_H

#include <qemu_platform.h>

#include <multipass/path.h>

namespace multipass
{
// This class is the platform detail for QEMU on macOS
class QemuPlatformDetail : public QemuPlatform
{
public:
    optional<IPAddress> get_ip_for(const std::string& hw_addr) override;
    void remove_resources_for(const std::string& name) override;
    void platform_health_check() override;
    QStringList full_platform_args(const VirtualMachineDescription& vm_desc) override;
    QString get_directory_name() override;
};
} // namespace multipass
#endif // MULTIPASS_QEMU_PLATFORM_DETAIL_H
