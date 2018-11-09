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

#include "qemu_process_spec.h"

#include <QHash>
#include <QString>

namespace mp = multipass;

namespace
{
const QHash<QString, QString> cpu_to_arch{{"x86_64", "x86_64"}, {"arm", "arm"},   {"arm64", "aarch64"},
                                          {"i386", "i386"},     {"power", "ppc"}, {"power64", "ppc64le"},
                                          {"s390x", "s390x"}};
} // namespace

mp::QemuProcessSpec::QemuProcessSpec(const mp::VirtualMachineDescription& desc, const QString& tap_device_name,
                             const QString& mac_addr)
    : desc(desc), tap_device_name(tap_device_name), mac_addr(mac_addr)
{
}

QString mp::QemuProcessSpec::program() const
{
    return QStringLiteral("qemu-system-") + cpu_to_arch.value(QSysInfo::currentCpuArchitecture());
}

QStringList mp::QemuProcessSpec::arguments() const
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

QString mp::QemuProcessSpec::apparmor_profile() const
{
    const auto base_path = qgetenv("SNAP"); // Validate??

    // Following profile is based on /etc/apparmor.d/abstractions/libvirt-qemu
    QString profile_template(R"END(
#include <tunables/global>
profile %1 flags=(attach_disconnected) {
    #include <abstractions/base>
    #include <abstractions/consoles>
    #include <abstractions/nameservice>

    # required for reading disk images
    capability dac_override,
    capability dac_read_search,
    capability chown,

    # needed to drop privileges
    capability setgid,
    capability setuid,

    network inet stream,
    network inet6 stream,

    /dev/net/tun rw,
    /dev/kvm rw,
    /dev/ptmx rw,
    /dev/kqemu rw,
    @{PROC}/*/status r,
    # When qemu is signaled to terminate, it will read cmdline of signaling
    # process for reporting purposes. Allowing read access to a process
    # cmdline may leak sensitive information embedded in the cmdline.
    @{PROC}/@{pid}/cmdline r,
    # Per man(5) proc, the kernel enforces that a thread may
    # only modify its comm value or those in its thread group.
    owner @{PROC}/@{pid}/task/@{tid}/comm rw,
    @{PROC}/sys/kernel/cap_last_cap r,
    owner @{PROC}/*/auxv r,
    @{PROC}/sys/vm/overcommit_memory r,

    # access to firmware's etc (selectively chosen for multipass' usage)
    %2/usr/share/seabios/** r,

    # for save and resume
    /{usr/,}bin/dash rmix,
    /{usr/,}bin/dd rmix,
    /{usr/,}bin/cat rmix,

    # for restore
    /{usr/,}bin/bash rmix,

    # for file-posix getting limits since 9103f1ce
    /sys/devices/**/block/*/queue/max_segments r,

    # for gathering information about available host resources
    /sys/devices/system/cpu/ r,
    /sys/devices/system/node/ r,
    /sys/devices/system/node/node[0-9]*/meminfo r,
    /sys/module/vhost/parameters/max_mem_regions r,

    # Disk images
    %3 rwk,  # QCow2 filesystem image
    %4 rwk,  # cloud-init ISO (TODO: why not read-only? qemu needs write for some reason)
}
    )END");

    return profile_template.arg(apparmor_profile_name(), base_path, desc.image.image_path, desc.cloud_init_iso);
}

QString mp::QemuProcessSpec::identifier() const
{
    return QString::fromStdString(desc.vm_name);
}
