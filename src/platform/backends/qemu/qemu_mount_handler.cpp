/*
 * Copyright (C) 2022 Canonical, Ltd.
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

#include "qemu_mount_handler.h"

#include <multipass/exceptions/exitless_sshprocess_exception.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>
#include <multipass/vm_mount.h>

#include <QDir>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "qemu-mount-handler";
} // namespace

mp::QemuMountHandler::QemuMountHandler(const SSHKeyProvider& ssh_key_provider) : MountHandler(ssh_key_provider)
{
}

void mp::QemuMountHandler::init_mount(VirtualMachine* vm, const std::string& target_path, const VMMount& vm_mount)
{
    auto state = vm->current_state();
    if (state != mp::VirtualMachine::State::suspended && state != mp::VirtualMachine::State::stopped &&
        state != mp::VirtualMachine::State::off)
        throw std::runtime_error("Please shutdown virtual machine before defining native mount.");

    if (!MP_FILEOPS.exists(QDir{QString::fromStdString(vm_mount.source_path)}))
    {
        throw std::runtime_error(fmt::format("Mount path \"{}\" does not exist.", vm_mount.source_path));
    }

    // Need to ensure no more than one uid/gid map is passed in here.
    if (vm_mount.uid_mappings.size() > 1 || vm_mount.gid_mappings.size() > 1)
    {
        throw std::runtime_error("Only one mapping per native mount allowed.");
    }

    mpl::log(mpl::Level::info, category,
             fmt::format("Initializing native mount {} => {} in {}", vm_mount.source_path, target_path, vm->vm_name));
    vm->add_vm_mount(target_path, vm_mount);

    // Need to save the vm pointer so it can be used later in stop.
    mounts[vm->vm_name] = std::make_pair(target_path, vm);
}

void mp::QemuMountHandler::start_mount(VirtualMachine* vm, ServerVariant server, const std::string& target_path,
                                       const std::chrono::milliseconds& timeout)
{
    // Need to SSHSession::exec() into the instance and run something like:
    // sudo mount -t 9p ${mount_tag} ${target_path} -o trans=virtio,version=9p2000.L,msize=536870912
    // where:
    // ${mount_tag} is the same mount_tag used in add_vm_mount()
    // ${target_path} is target_path
    // We may need to play around with the options here such as msize or other necessary options.
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};

    auto proc = session.exec(fmt::format("sudo mount -t 9p {} {} -o trans=virtio,version=9p2000.L,msize=536870912",
                                         "test_mount", "/home/ubuntu"));
    //  fmt::format("mount_{}", target_path), target_path));
    try
    {
        // TODO: modify timeout
        auto exit_code = proc.exit_code(timeout);

        if (exit_code != 0)
        {
            auto error_msg = proc.read_std_error();
            mpl::log(
                mpl::Level::warning, category,
                fmt::format("Failed to start native mount, error message: \'{}\'", mp::utils::trim_end(error_msg)));
            throw std::runtime_error(error_msg);
        }
    }
    catch (const mp::ExitlessSSHProcessException&)
    {
        mpl::log(mpl::Level::debug, category,
                 fmt::format("Timeout while starting native mount in \"{}\"", vm->vm_name));
        // TODO: timeout; not sure what to do here
    }
}

void mp::QemuMountHandler::stop_mount(const std::string& instance, const std::string& path)
{
    auto mount = mounts.find(instance);
    if (mount == mounts.end() && mount->second.first != path)
    {
        mpl::log(mpl::Level::info, category,
                 fmt::format("No native mount defined for \"{}\" serving '{}'", instance, path));
        return;
    }

    // The needs to SSHSession::exec() into the instance and unmount the path
    auto& vm = mount->second.second;
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};
    mpl::log(mpl::Level::info, category, fmt::format("Stopping native mount '{}' in instance \"{}\"", path, instance));

    auto proc = session.exec(fmt::format("sudo umount {}", path));
    try
    {
        auto exit_code = proc.exit_code();

        if (exit_code != 0)
        {
            auto error_msg = proc.read_std_error();
            mpl::log(
                mpl::Level::warning, category,
                fmt::format("Failed to start native mount, error message: \'{}\'", mp::utils::trim_end(error_msg)));
            throw std::runtime_error(error_msg);
        }

        vm->delete_vm_mount(path);
        mounts.erase(instance);
    }
    catch (const mp::ExitlessSSHProcessException&)
    {
        mpl::log(mpl::Level::info, category,
                 fmt::format("Failed to terminate mount '{}' in instance \"{}\"", path, instance));
    }
}

void mp::QemuMountHandler::stop_all_mounts_for_instance(const std::string& instance)
{
    // This just needs to iterate over all mounts for an instance and stop them
    auto mount = mounts.find(instance);
    if (mount == mounts.end())
    {
        mpl::log(mpl::Level::info, category, fmt::format("No native mounts to stop for instance \"{}\"", instance));
    }
    else
    {
        stop_mount(instance, mount->second.first);
    }
}

bool mp::QemuMountHandler::has_instance_already_mounted(const std::string& instance, const std::string& path) const
{
    // Just returns if a mount is already running for the given instance and target_path
    auto entry = mounts.find(instance);
    return entry != mounts.end() && entry->second.first != path;
}
