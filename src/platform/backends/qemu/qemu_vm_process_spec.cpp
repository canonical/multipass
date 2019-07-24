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
#include <multipass/snap_utils.h>
#include <shared/linux/backend_utils.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mu = multipass::utils;

namespace
{
constexpr auto default_machine_type = "pc-i440fx-xenial";

// This returns the initial two Qemu command line options we used in Multipass. Only of use to resume old suspended
// images.
//  === Do not change this! ===
QStringList initial_qemu_arguments(const mp::VirtualMachineDescription& desc, const QString& tap_device_name,
                                   bool use_cdrom)
{
    auto mem_size = QString::number(desc.mem_size.in_megabytes()) + 'M'; /* flooring here; format documented in
    `man qemu-system`, under `-m` option; including suffix to avoid relying on default unit */

    QStringList args{
        "--enable-kvm",
        "-hda",
        desc.image.image_path,
        "-smp",
        QString::number(desc.num_cores),
        "-m",
        mem_size,
        "-device",
        QString("virtio-net-pci,netdev=hostnet0,id=net0,mac=%1").arg(QString::fromStdString(desc.mac_addr)),
        "-netdev",
        QString("tap,id=hostnet0,ifname=%1,script=no,downscript=no").arg(tap_device_name),
        "-qmp",
        "stdio",
        "-cpu",
        "host",
        "-chardev",
        "null,id=char0",
        "-serial",
        "chardev:char0",
        "-nographic"};

    if (use_cdrom)
    {
        args << "-cdrom" << desc.cloud_init_iso;
    }
    else
    {
        args << "-drive" << QString("file=%1,if=virtio,format=raw,snapshot=off,read-only").arg(desc.cloud_init_iso);
    }
    return args;
}
} // namespace

QString mp::QemuVMProcessSpec::default_machine_type()
{
    return QString::fromLatin1(::default_machine_type);
}

mp::QemuVMProcessSpec::QemuVMProcessSpec(const mp::VirtualMachineDescription& desc, const QString& tap_device_name,
                                         const multipass::optional<ResumeData>& resume_data)
    : desc(desc), tap_device_name(tap_device_name), resume_data{resume_data}
{
}

QString mp::QemuVMProcessSpec::program() const
{
    return "qemu-system-" + mp::backend::cpu_arch();
}

QStringList mp::QemuVMProcessSpec::arguments() const
{
    QStringList args;

    if (resume_data)
    {
        if (resume_data->arguments.length() > 0)
        {
            // arguments used were saved externally, import them
            args = resume_data->arguments;
        }
        else
        {
            // fall-back to reconstructing arguments
            args = initial_qemu_arguments(desc, tap_device_name, resume_data->use_cdrom_flag);
        }

        // need to append extra arguments for resume
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
    else
    {
        auto mem_size = QString::number(desc.mem_size.in_megabytes()) + 'M'; /* flooring here; format documented in
    `man qemu-system`, under `-m` option; including suffix to avoid relying on default unit */

        args << "--enable-kvm";
        // The VM image itself
        args << "-device"
             << "virtio-scsi-pci,id=scsi0"
             << "-drive" << QString("file=%1,if=none,format=qcow2,discard=unmap,id=hda").arg(desc.image.image_path)
             << "-device"
             << "scsi-hd,drive=hda,bus=scsi0.0";
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
        // Cloud-init disk
        args << "-cdrom" << desc.cloud_init_iso;
    }

    return args;
}

QString mp::QemuVMProcessSpec::working_directory() const
{
    if (mu::is_snap())
        return mu::snap_dir().append("/qemu");
    return QString();
}

QString mp::QemuVMProcessSpec::apparmor_profile() const
{
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

  # Allow multipassd send qemu signals
  signal (receive) peer=%2,

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
  %3 r,

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

  # binary and its libs
  %4/usr/bin/%5 ixr,
  %4/{,usr/}lib/{,@{multiarch}/}{,**/}*.so* rm,

  # CLASSIC ONLY: need to specify required libs from core snap
  /snap/core/*/{,usr/}lib/@{multiarch}/{,**/}*.so* rm,

  # Disk images
  %6 rwk,  # QCow2 filesystem image
  %7 rk,   # cloud-init ISO
}
    )END");

    /* Customisations depending on if running inside snap or not */
    QString root_dir;    // root directory: either "" or $SNAP
    QString signal_peer; // who can send kill signal to qemu
    QString firmware;    // location of bootloader firmware needed by qemu

    if (mu::is_snap())
    {
        root_dir = mu::snap_dir();
        signal_peer = "snap.multipass.multipassd"; // only multipassd can send qemu signals
        firmware = root_dir + "/qemu/*";           // if snap confined, firmware in $SNAP/qemu
    }
    else
    {
        signal_peer = "unconfined";
        firmware = "/usr/share/seabios/*";
    }

    return profile_template.arg(apparmor_profile_name(), signal_peer, firmware, root_dir, program(),
                                desc.image.image_path, desc.cloud_init_iso);
}

QString mp::QemuVMProcessSpec::identifier() const
{
    return QString::fromStdString(desc.vm_name);
}
