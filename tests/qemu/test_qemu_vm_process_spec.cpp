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

#include "tests/common.h"
#include "tests/mock_environment_helpers.h"

#include <src/platform/backends/qemu/qemu_vm_process_spec.h>

#include <QDir>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>

namespace mp = multipass;
namespace mpt = multipass::test;
using namespace testing;

struct TestQemuVMProcessSpec : public Test
{
    const mp::VirtualMachineDescription desc{2 /*cores*/,
                                             mp::MemorySize{"3G"} /*mem_size*/,
                                             mp::MemorySize{"4G"} /*disk_space*/,
                                             "vm_name",
                                             "zone1",
                                             "00:11:22:33:44:55",
                                             {},
                                             "ssh_username",
                                             {"/path/to/image", "", "", "", "", {}}, // VMImage
                                             mp::Path{"/path/to/cloud_init.iso"},
                                             {},
                                             {},
                                             {},
                                             {}};
    const QStringList platform_args{
        {"--enable-kvm", "-nic", "tap,ifname=tap_device,script=no,downscript=no"}};
    const std::unordered_map<std::string, std::pair<std::string, QStringList>> mount_args{
        {"path/to/target",
         {"path/to/source",
          {"-virtfs",
           "local,security_model=passthrough,uid_map=1000:1000,gid_map=1000:1000,path=path/to/"
           "target,mount_tag=m810e457178f448d9afffc9d950d726"}}}};
};

TEST_F(TestQemuVMProcessSpec, defaultArgumentsCorrect)
{
    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, std::nullopt);

    EXPECT_EQ(spec.arguments(),
              QStringList({"--enable-kvm",
                           "-nic",
                           "tap,ifname=tap_device,script=no,downscript=no",
                           "-device",
                           "virtio-scsi-pci,id=scsi0",
                           "-drive",
                           "file=/path/to/image,if=none,format=qcow2,discard=unmap,id=hda",
                           "-device",
                           "scsi-hd,drive=hda,bus=scsi0.0",
                           "-smp",
                           "2",
                           "-m",
                           "3072M",
                           "-qmp",
                           "stdio",
                           "-chardev",
                           "null,id=char0",
                           "-serial",
                           "chardev:char0",
                           "-nographic",
                           "-cdrom",
                           "/path/to/cloud_init.iso",
                           "-virtfs",
                           "local,security_model=passthrough,uid_map=1000:1000,gid_map=1000:1000,"
                           "path=path/to/target,mount_tag=m810e457178f448d9afffc9d950d726"}));
}

TEST_F(TestQemuVMProcessSpec, resumeArgumentsTakenFromResumedata)
{
    const mp::QemuVMProcessSpec::ResumeData resume_data{"suspend_tag",
                                                        "machine_type",
                                                        false,
                                                        {"-one", "-two"}};

    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, resume_data);

    EXPECT_EQ(spec.arguments(),
              QStringList({"-one", "-two", "-loadvm", "suspend_tag", "-machine", "machine_type"})
                  << mount_args.begin()->second.second);
}

TEST_F(TestQemuVMProcessSpec, resumeWithMissingMachineTypeGuessesCorrectly)
{
    mp::QemuVMProcessSpec::ResumeData resume_data_missing_machine_info;
    resume_data_missing_machine_info.suspend_tag = "suspend_tag";
    resume_data_missing_machine_info.arguments = QStringList{"-args"};

    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, resume_data_missing_machine_info);

    EXPECT_EQ(spec.arguments(),
              QStringList({"-args", "-loadvm", "suspend_tag"})
                  << mount_args.begin()->second.second);
}

TEST_F(TestQemuVMProcessSpec, resumeFixesVmnetFormat)
{
    const mp::QemuVMProcessSpec::ResumeData resume_data{"suspend_tag",
                                                        "machine_type",
                                                        false,
                                                        {"vmnet-macos,mode=shared,foo"}};

    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, resume_data);

    EXPECT_EQ(
        spec.arguments(),
        QStringList({"vmnet-shared,foo", "-loadvm", "suspend_tag", "-machine", "machine_type"})
            << mount_args.begin()->second.second);
}

TEST_F(TestQemuVMProcessSpec, apparmorProfileIncludesFileMountPerms)
{
    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, std::nullopt);

    EXPECT_TRUE(spec.apparmor_profile().contains("path/to/source/ rw"));
    EXPECT_TRUE(spec.apparmor_profile().contains("path/to/source/** rwlk"));
}

TEST_F(TestQemuVMProcessSpec, apparmorProfileHasCorrectName)
{
    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, std::nullopt);

    EXPECT_TRUE(spec.apparmor_profile().contains("profile multipass.vm_name.qemu-system-"));
}

TEST_F(TestQemuVMProcessSpec, apparmorProfileIncludesDiskImages)
{
    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, std::nullopt);

    EXPECT_TRUE(spec.apparmor_profile().contains("/path/to/image rwk,"));
    EXPECT_TRUE(spec.apparmor_profile().contains("/path/to/cloud_init.iso rk,"));
}

TEST_F(TestQemuVMProcessSpec, apparmorProfileIdentifier)
{
    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, std::nullopt);

    EXPECT_EQ(spec.identifier(), "vm_name");
}

TEST_F(TestQemuVMProcessSpec, apparmorProfileRunningAsSnapCorrect)
{
    const QByteArray snap_name{"multipass"};
    QTemporaryDir snap_dir;

    mpt::SetEnvScope e("SNAP", snap_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, std::nullopt);

    EXPECT_TRUE(
        spec.apparmor_profile().contains("signal (receive) peer=snap.multipass.multipassd"));
    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/qemu/* r,").arg(snap_dir.path())));
    EXPECT_TRUE(
        spec.apparmor_profile().contains(QString("%1/usr/bin/qemu-system-").arg(snap_dir.path())));
}

TEST_F(TestQemuVMProcessSpec, apparmorProfileRunningAsSymlinkedSnapCorrect)
{
    const QByteArray snap_name{"multipass"};
    QTemporaryDir snap_dir, link_dir;

    link_dir.remove();
    QFile::link(snap_dir.path(), link_dir.path());

    mpt::SetEnvScope e("SNAP", link_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, std::nullopt);

    EXPECT_TRUE(spec.apparmor_profile().contains(QString("%1/qemu/* r,").arg(snap_dir.path())));
    EXPECT_TRUE(
        spec.apparmor_profile().contains(QString("%1/usr/bin/qemu-system-").arg(snap_dir.path())));
}

TEST_F(TestQemuVMProcessSpec, apparmorProfileNotRunningAsSnapCorrect)
{
    const QByteArray snap_name{"multipass"};

    mpt::UnsetEnvScope e("SNAP");
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, std::nullopt);

    EXPECT_TRUE(spec.apparmor_profile().contains("signal (receive) peer=unconfined"));
    EXPECT_TRUE(
        spec.apparmor_profile().contains("/usr{,/local}/share/{seabios,ovmf,qemu,qemu-efi}/* r,"));
    EXPECT_TRUE(spec.apparmor_profile().contains(" /usr/bin/qemu-system-")); // space wanted
}

TEST_F(TestQemuVMProcessSpec, apparmorProfileLetsBridgeHelperRunInSnap)
{
    const QByteArray snap_name{"multipass"};
    QTemporaryDir snap_dir;

    mpt::SetEnvScope e("SNAP", snap_dir.path().toUtf8());
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, std::nullopt);

    EXPECT_TRUE(
        spec.apparmor_profile().contains(QString(" %1/bin/bridge_helper").arg(snap_dir.path())));
}

TEST_F(TestQemuVMProcessSpec, apparmorProfileLetsBridgeHelperRunOutsideSnap)
{
    const QByteArray snap_name{"multipass"};

    mpt::UnsetEnvScope e("SNAP");
    mpt::SetEnvScope e2("SNAP_NAME", snap_name);
    mp::QemuVMProcessSpec spec(desc, platform_args, mount_args, std::nullopt);

    EXPECT_TRUE(spec.apparmor_profile().contains(" /bin/bridge_helper"));
}
