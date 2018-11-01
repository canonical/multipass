/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include "qemu_process.h"

namespace
{
const QHash<QString, QString> cpu_to_arch{{"x86_64", "x86_64"}, {"arm", "arm"},   {"arm64", "aarch64"},
                                          {"i386", "i386"},     {"power", "ppc"}, {"power64", "ppc64le"},
                                          {"s390x", "s390x"}};
}

QemuProcess::QemuProcess(const multipass::VirtualMachineDescription &desc, const QString &tap_device_name, const QString &mac_addr)
    : desc(desc)
    , tap_device_name(tap_device_name)
    , mac_addr(mac_addr)
{}

QString QemuProcess::program() const
{
    return QStringLiteral("qemu-system-") + cpu_to_arch.value(QSysInfo::currentCpuArchitecture());
}

QStringList QemuProcess::arguments() const
{
    auto mem_size = QString::fromStdString(desc.mem_size);
    if (mem_size.endsWith("B"))
    {
        mem_size.chop(1);
    }

    QStringList args{"--enable-kvm"};
    // The VM image itself
    args << "-hda" << desc.image.image_path;
    // For the cloud-init configuration
    args << "-drive" << QString{"file="} + desc.cloud_init_iso + QString{",if=virtio,format=raw"};
    // Number of cpu cores
    args << "-smp" << QString::number(desc.num_cores);
    // Memory to use for VM
    args << "-m" << mem_size;
    // Create a virtual NIC in the VM
    args << "-device" << QString("virtio-net-pci,netdev=hostnet0,id=net0,mac=%1").arg(mac_addr);
    // Create tap device to connect to virtual bridge
    args << "-netdev";
    args << QString("tap,id=hostnet0,ifname=%1,script=no,downscript=no").arg(tap_device_name);
    // Control interface
    args << "-qmp"
         << "stdio";
    // No console
    args << "-chardev"
         // TODO Read and log machine output when verbose
         << "null,id=char0"
         << "-serial"
         << "chardev:char0"
         // TODO Add a debugging mode with access to console
         << "-nographic";

    return args;
}

QString QemuProcess::apparmor_profile() const
{
    return "";
}

QString QemuProcess::identifier() const
{
    return QString::fromStdString(desc.vm_name);
}
