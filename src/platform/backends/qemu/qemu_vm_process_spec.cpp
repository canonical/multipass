/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "qemu_vm_process_spec.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <shared/linux/backend_utils.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto default_machine_type = "pc-i440fx-xenial";
} // namespace

/*
 * Qemu will often fail to resume a VM that was run with a different command line.
 * To support backward compatibility, we version each Qemu command line iteration.
 *
 * Versions:
 *  1 - changed how cloud-init ISO was specified:
 *      Replaced: "-drive file=cloud-init.iso,if=virtio,format=raw,snapshot=off,read-only"
 *      With:     "-cdrom cloud-init.iso"
 *      Note this was originally encompassed in the metadata as "use_cdrom" being true.
 *  0 - original
 */

int multipass::QemuVMProcessSpec::latest_version()
{
    return 1;
}

QString mp::QemuVMProcessSpec::default_machine_type()
{
    return QString::fromLatin1(::default_machine_type);
}

mp::QemuVMProcessSpec::QemuVMProcessSpec(const mp::VirtualMachineDescription& desc, int version,
                                         const QString& tap_device_name,
                                         const multipass::optional<ResumeData>& resume_data)
    : desc(desc), version(version), tap_device_name(tap_device_name), resume_data{resume_data}
{
}

QString mp::QemuVMProcessSpec::program() const
{
    return "qemu-system-" + mp::backend::cpu_arch();
}

QStringList mp::QemuVMProcessSpec::arguments() const
{
    auto mem_size = QString::number(desc.mem_size.in_megabytes()) + 'M'; /* flooring here; format documented in
    `man qemu-system`, under `-m` option; including suffix to avoid relying on default unit */

    QStringList args{"--enable-kvm"};
    // The VM image itself
    args << "-hda" << desc.image.image_path;
    // Number of cpu cores
    args << "-smp" << QString::number(desc.num_cores);
    // Memory to use for VM
    args << "-m" << mem_size;
    // Create a virtual NIC in the VM
    args << "-device"
         << QString("virtio-net-pci,netdev=hostnet0,id=net0,mac=%1").arg(QString::fromStdString(desc.mac_addr));
    // Create tap device to connect to virtual bridge
    args << "-netdev";
    args << QString("tap,id=hostnet0,ifname=%1,script=no,downscript=no").arg(tap_device_name);
    // Control interface
    args << "-qmp"
         << "stdio";
    // Pass host CPU flags to VM
    args << "-cpu"
         << "host";
    // No console
    args << "-chardev"
         // TODO Read and log machine output when verbose
         << "null,id=char0"
         << "-serial"
         << "chardev:char0"
         // TODO Add a debugging mode with access to console
         << "-nographic";

    if (version > 0)
    { // version=1+
        args << "-cdrom" << desc.cloud_init_iso;
    }
    else
    { // version=0
        args << "-drive" << QString("file=%1,if=virtio,format=raw,snapshot=off,read-only").arg(desc.cloud_init_iso);
    }

    if (resume_data)
    {
        QString machine_type = resume_data->machine_type;
        if (machine_type.isEmpty())
        {
            mpl::log(mpl::Level::info, desc.vm_name,
                     fmt::format("Cannot determine QEMU machine type. Defaulting to '{}'.", default_machine_type()));
            machine_type = default_machine_type();
        }

        args << "-loadvm" << resume_data->suspend_tag;
        args << "-machine" << machine_type;
    }

    return args;
}

QString mp::QemuVMProcessSpec::working_directory() const
{
    auto snap = qgetenv("SNAP");
    if (!snap.isEmpty())
        return snap.append("/qemu");
    return QString();
}
