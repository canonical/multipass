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

#include "lxd_mount_handler.h"
#include "lxd_request.h"

namespace mp = multipass;

namespace
{
constexpr std::string_view category = "lxd-mount-handler";
constexpr int length_of_unique_id_without_prefix = 25;
constexpr int timeout_milliseconds = 30000;
} // namespace

namespace multipass
{
LXDMountHandler::LXDMountHandler(mp::NetworkAccessManager* network_manager,
                                 LXDVirtualMachine* lxd_virtual_machine,
                                 const SSHKeyProvider* ssh_key_provider,
                                 const std::string& target_path,
                                 VMMount mount_spec)
    : MountHandler{lxd_virtual_machine, ssh_key_provider, std::move(mount_spec), target_path},
      network_manager{network_manager},
      lxd_instance_endpoint{
          QString("%1/instances/%2").arg(lxd_socket_url.toString(), lxd_virtual_machine->vm_name.c_str())},
      // make_uuid is a seed based unique id generator, that makes the device_name reproducible if the seed
      // (target_path) is the same. If the seeds are different, then the generated ids are likely to be different as
      // well. 27 (25 + 2(d_)) letters is the maximum device name length that LXD can accept and d_ stands for device
      // name.
      device_name{mp::utils::make_uuid(target_path)
                      .remove("-")
                      .left(length_of_unique_id_without_prefix)
                      .prepend("d_")
                      .toStdString()}
{
}

void LXDMountHandler::activate_impl(ServerVariant /**/, std::chrono::milliseconds /**/)
{
    const VirtualMachine::State state = vm->current_state();
    if (state != VirtualMachine::State::off && state != VirtualMachine::State::stopped)
    {
        throw mp::NativeMountNeedsStoppedVMException(vm->vm_name);
    }

    mpl::log(mpl::Level::info, std::string(category),
             fmt::format("initializing native mount {} => {} in '{}'", source, target, vm->vm_name));
    lxd_device_add();
}

void LXDMountHandler::deactivate_impl(bool /*force*/)
{
    // TODO: remove the throwing and adjust the unit tests once lxd fix the hot-unmount bug
    const VirtualMachine::State state = vm->current_state();
    if (state != VirtualMachine::State::off && state != VirtualMachine::State::stopped)
    {
        throw std::runtime_error("Please stop the instance " + vm->vm_name + " before unmount it natively.");
    }

    mpl::log(mpl::Level::info, std::string(category),
             fmt::format("Stopping native mount \"{}\" in instance '{}'", target, vm->vm_name));
    lxd_device_remove();
}

LXDMountHandler::~LXDMountHandler() = default;

void LXDMountHandler::lxd_device_remove()
{
    const QJsonObject instance_info = lxd_request(network_manager, "GET", lxd_instance_endpoint);
    QJsonObject instance_info_metadata = instance_info["metadata"].toObject();
    QJsonObject device_list = instance_info_metadata["devices"].toObject();

    device_list.remove(device_name.c_str());
    instance_info_metadata["devices"] = device_list;
    // TODO: make this put method If-Match pattern
    const QJsonObject json_reply = lxd_request(network_manager, "PUT", lxd_instance_endpoint, instance_info_metadata);
    lxd_wait(network_manager, multipass::lxd_socket_url, json_reply, timeout_milliseconds);
}

void LXDMountHandler::lxd_device_add()
{
    const QJsonObject instance_info = lxd_request(network_manager, "GET", lxd_instance_endpoint);
    QJsonObject instance_info_metadata = instance_info["metadata"].toObject();
    QJsonObject device_list = instance_info_metadata["devices"].toObject();

    const std::string abs_target_path =
        std::filesystem::path{target}.is_relative() ? fmt::format("/home/{}/{}", vm->ssh_username(), target) : target;
    const QJsonObject new_device_object{
        {"path", abs_target_path.c_str()}, {"source", source.c_str()}, {"type", "disk"}};

    device_list.insert(device_name.c_str(), new_device_object);
    instance_info_metadata["devices"] = device_list;

    // TODO: make this put method If-Match pattern
    const QJsonObject json_reply = lxd_request(network_manager, "PUT", lxd_instance_endpoint, instance_info_metadata);
    lxd_wait(network_manager, multipass::lxd_socket_url, json_reply, timeout_milliseconds);
}

} // namespace multipass
