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

#include <src/platform/backends/qemu/qemu_vm_process_spec.h>

#include <gmock/gmock.h>

namespace mp = multipass;
using namespace testing;

struct TestQemuVMProcessSpec : public Test
{
    const mp::VirtualMachineDescription desc{2 /*cores*/,
                                             mp::MemorySize{"3G"} /*mem_size*/,
                                             mp::MemorySize{"4G"} /*disk_space*/,
                                             "vm_name",
                                             "00:11:22:33:44:55",
                                             "ssh_username",
                                             {"/path/to/image", "", "", "", "", "", "", {}}, // VMImage
                                             mp::Path{"/path/to/cloud_init.iso"}};
    const QString tap_device_name{"tap_device"};
    const mp::QemuVMProcessSpec::ResumeData resume_data{"suspend_tag", "machine_type"};
};

TEST_F(TestQemuVMProcessSpec, latest_version_correct)
{
    ASSERT_EQ(1, mp::QemuVMProcessSpec::latest_version());
}

TEST_F(TestQemuVMProcessSpec, version1_command_correct)
{
    mp::QemuVMProcessSpec spec(desc, 1, tap_device_name, mp::nullopt);

    EXPECT_EQ(spec.arguments(), QStringList({"--enable-kvm",
                                             "-hda",
                                             "/path/to/image",
                                             "-smp",
                                             "2",
                                             "-m",
                                             "3072M",
                                             "-device",
                                             "virtio-net-pci,netdev=hostnet0,id=net0,mac=00:11:22:33:44:55",
                                             "-netdev",
                                             "tap,id=hostnet0,ifname=tap_device,script=no,downscript=no",
                                             "-qmp",
                                             "stdio",
                                             "-cpu",
                                             "host",
                                             "-chardev",
                                             "null,id=char0",
                                             "-serial",
                                             "chardev:char0",
                                             "-nographic",
                                             "-cdrom",
                                             "/path/to/cloud_init.iso"}));
}

TEST_F(TestQemuVMProcessSpec, version0_command_correct)
{
    mp::QemuVMProcessSpec spec(desc, 0, tap_device_name, mp::nullopt);

    EXPECT_EQ(spec.arguments(),
              QStringList({"--enable-kvm",
                           "-hda",
                           "/path/to/image",
                           "-smp",
                           "2",
                           "-m",
                           "3072M",
                           "-device",
                           "virtio-net-pci,netdev=hostnet0,id=net0,mac=00:11:22:33:44:55",
                           "-netdev",
                           "tap,id=hostnet0,ifname=tap_device,script=no,downscript=no",
                           "-qmp",
                           "stdio",
                           "-cpu",
                           "host",
                           "-chardev",
                           "null,id=char0",
                           "-serial",
                           "chardev:char0",
                           "-nographic",
                           "-drive",
                           "file=/path/to/cloud_init.iso,if=virtio,format=raw,snapshot=off,read-only"}));
}

TEST_F(TestQemuVMProcessSpec, version1_resume_command_correct)
{
    mp::QemuVMProcessSpec spec(desc, 1, tap_device_name, resume_data);

    EXPECT_EQ(spec.arguments(), QStringList({"--enable-kvm",
                                             "-hda",
                                             "/path/to/image",
                                             "-smp",
                                             "2",
                                             "-m",
                                             "3072M",
                                             "-device",
                                             "virtio-net-pci,netdev=hostnet0,id=net0,mac=00:11:22:33:44:55",
                                             "-netdev",
                                             "tap,id=hostnet0,ifname=tap_device,script=no,downscript=no",
                                             "-qmp",
                                             "stdio",
                                             "-cpu",
                                             "host",
                                             "-chardev",
                                             "null,id=char0",
                                             "-serial",
                                             "chardev:char0",
                                             "-nographic",
                                             "-cdrom",
                                             "/path/to/cloud_init.iso",
                                             "-loadvm",
                                             "suspend_tag",
                                             "-machine",
                                             "machine_type"}));
}

TEST_F(TestQemuVMProcessSpec, version0_resume_command_correct)
{
    mp::QemuVMProcessSpec spec(desc, 0, tap_device_name, resume_data);

    EXPECT_EQ(spec.arguments(), QStringList({"--enable-kvm",
                                             "-hda",
                                             "/path/to/image",
                                             "-smp",
                                             "2",
                                             "-m",
                                             "3072M",
                                             "-device",
                                             "virtio-net-pci,netdev=hostnet0,id=net0,mac=00:11:22:33:44:55",
                                             "-netdev",
                                             "tap,id=hostnet0,ifname=tap_device,script=no,downscript=no",
                                             "-qmp",
                                             "stdio",
                                             "-cpu",
                                             "host",
                                             "-chardev",
                                             "null,id=char0",
                                             "-serial",
                                             "chardev:char0",
                                             "-nographic",
                                             "-drive",
                                             "file=/path/to/cloud_init.iso,if=virtio,format=raw,snapshot=off,read-only",
                                             "-loadvm",
                                             "suspend_tag",
                                             "-machine",
                                             "machine_type"}));
}

TEST_F(TestQemuVMProcessSpec, version0_resume_command_correct_with_missing_machine_type)
{
    mp::QemuVMProcessSpec::ResumeData resume_data_missing_machine_info;
    resume_data_missing_machine_info.suspend_tag = "suspend_tag";

    mp::QemuVMProcessSpec spec(desc, 0, tap_device_name, resume_data_missing_machine_info);

    EXPECT_EQ(spec.arguments(), QStringList({"--enable-kvm",
                                             "-hda",
                                             "/path/to/image",
                                             "-smp",
                                             "2",
                                             "-m",
                                             "3072M",
                                             "-device",
                                             "virtio-net-pci,netdev=hostnet0,id=net0,mac=00:11:22:33:44:55",
                                             "-netdev",
                                             "tap,id=hostnet0,ifname=tap_device,script=no,downscript=no",
                                             "-qmp",
                                             "stdio",
                                             "-cpu",
                                             "host",
                                             "-chardev",
                                             "null,id=char0",
                                             "-serial",
                                             "chardev:char0",
                                             "-nographic",
                                             "-drive",
                                             "file=/path/to/cloud_init.iso,if=virtio,format=raw,snapshot=off,read-only",
                                             "-loadvm",
                                             "suspend_tag",
                                             "-machine",
                                             "pc-i440fx-xenial"}));
}

