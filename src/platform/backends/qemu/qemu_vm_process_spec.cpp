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

#include "qemu_vm_process_spec.h"

#include <multipass/exceptions/snap_environment_exception.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/snap_utils.h>
#include <shared/linux/backend_utils.h>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

mp::QemuVMProcessSpec::QemuVMProcessSpec(const mp::VirtualMachineDescription& desc, const QStringList& platform_args,
                                         const mp::QemuVirtualMachine::MountArgs& mount_args,
                                         const std::optional<ResumeData>& resume_data)
    : desc{desc}, platform_args{platform_args}, mount_args{mount_args}, resume_data{resume_data}
{
}

QStringList mp::QemuVMProcessSpec::arguments() const
{
    QStringList args;

    if (resume_data)
    {
        args = resume_data->arguments;

        // need to append extra arguments for resume
        args << "-loadvm" << resume_data->suspend_tag;

        QString machine_type = resume_data->machine_type;
        if (!machine_type.isEmpty())
        {
            args << "-machine" << machine_type;
        }
        else
        {
            mpl::log(mpl::Level::info, desc.vm_name,
                     fmt::format("Cannot determine QEMU machine type. Falling back to system default."));
        }

        // need to fix old-style vmnet arguments
        // TODO: remove in due time
        args.replaceInStrings("vmnet-macos,mode=shared,", "vmnet-shared,");
    }
    else
    {
        auto mem_size = QString::number(desc.mem_size.in_megabytes()) + 'M'; /* flooring here; format documented in
    `man qemu-system`, under `-m` option; including suffix to avoid relying on default unit */

        args << platform_args;
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
        // Cloud-init disk
        args << "-cdrom" << desc.cloud_init_iso;
    }

    for (const auto& [_, mount_data] : mount_args)
    {
        const auto& [__, mount_args] = mount_data;
        args << mount_args;
    }

    return args;
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

  # Enables modifying of file ownership and permissions
  capability fsetid,
  capability fowner,

  # needed to drop privileges
  capability setgid,
  capability setuid,

  # for bridge helper
  capability net_admin,
  capability net_raw,

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

  # to execute bridge helper
  %4/bin/bridge_helper ix,

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
  /{,var/lib/snapd/}snap/core18/*/{,usr/}lib/@{multiarch}/{,**/}*.so* rm,

  # Disk images
  %6 rwk,  # QCow2 filesystem image
  %7 rk,   # cloud-init ISO

  # allow full access just to user-specified mount directories on the host
  %8
}
    )END");

    /* Customisations depending on if running inside snap or not */
    QString root_dir;    // root directory: either "" or $SNAP
    QString signal_peer; // who can send kill signal to qemu
    QString firmware;    // location of bootloader firmware needed by qemu
    QString mount_dirs;  // directories on host that are mounted

    for (const auto& [_, mount_data] : mount_args)
    {
        const auto& [source_path, __] = mount_data;
        mount_dirs += QString::fromStdString(source_path) + "/ rw,\n  ";
        mount_dirs += QString::fromStdString(source_path) + "/** rwlk,\n  ";
    }

    try
    {
        root_dir = mpu::snap_dir();
        signal_peer = "snap.multipass.multipassd"; // only multipassd can send qemu signals
        firmware = root_dir + "/qemu/*";           // if snap confined, firmware in $SNAP/qemu
    }
    catch (const mp::SnapEnvironmentException&)
    {
        signal_peer = "unconfined";
        firmware = "/usr{,/local}/share/{seabios,ovmf,qemu,qemu-efi}/*";
    }

    return profile_template.arg(apparmor_profile_name(), signal_peer, firmware, root_dir, program(),
                                desc.image.image_path, desc.cloud_init_iso, mount_dirs);
}

QString mp::QemuVMProcessSpec::identifier() const
{
    return QString::fromStdString(desc.vm_name);
}
