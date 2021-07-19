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

#include <QHash>
#include <QString>
#include <QStringList>
#include <QSysInfo>

#include <memory>
#include <string>

namespace
{
const QHash<QString, QString> cpu_to_arch{{"x86_64", "x86_64"}, {"arm", "arm"},   {"arm64", "aarch64"},
                                          {"i386", "i386"},     {"power", "ppc"}, {"power64", "ppc64le"},
                                          {"s390x", "s390x"}};
}

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
    virtual QString get_directory_name()
    {
        return {};
    };
    virtual QString get_host_arch()
    {
        return host_arch;
    };

protected:
    explicit QemuPlatform() : host_arch{cpu_to_arch.value(QSysInfo::currentCpuArchitecture())} {};
    QemuPlatform(const QemuPlatform&) = delete;
    QemuPlatform& operator=(const QemuPlatform&) = delete;

private:
    const QString host_arch;
};

QemuPlatform::UPtr make_qemu_platform(const Path& data_dir);
} // namespace multipass
#endif // MULTIPASS_QEMU_PLATFORM_H
