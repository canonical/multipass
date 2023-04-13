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

// namespace mpl = multipass::logging;
// namespace mpu = multipass::utils;

namespace
{
// constexpr std::string_view category = "lxd-mount-handler";
} // namespace

namespace multipass
{
LXDMountHandler::LXDMountHandler(mp::NetworkAccessManager* network_manager, LXDVirtualMachine* vm,
                                 const SSHKeyProvider* ssh_key_provider, const std::string& target,
                                 const VMMount& mount)
    : MountHandler{vm, ssh_key_provider, target, mount.source_path},
      network_manager_{network_manager},
      lxd_instance_endpoint_new_(
          QString("%1/instances/%2").arg(lxd_socket_url.toString()).arg(vm->vm_name.c_str()))
{
    const VirtualMachine::State state = vm->current_state();
    if (state != VirtualMachine::State::off && state != VirtualMachine::State::stopped)
    {
        throw std::runtime_error("Please stop the instance " + vm->vm_name + " before mount it natively.");
    }

    std::lock_guard active_lock{active_mutex};
    lxd_device_add();
}

void LXDMountHandler::start_impl(ServerVariant /**/, std::chrono::milliseconds /**/)
{
}

void LXDMountHandler::stop_impl(bool force)
{
}

LXDMountHandler::~LXDMountHandler()
{
    vm->stop();
    std::lock_guard active_lock{active_mutex};
    lxd_device_remove();
}

void LXDMountHandler::lxd_device_remove()
{
    const QJsonObject instance_info = lxd_request(network_manager_, "GET", lxd_instance_endpoint_new_);
    QJsonObject instance_info_metadata = instance_info["metadata"].toObject();
    QJsonObject device_list = instance_info_metadata["devices"].toObject();

    // mydisk is the hard coded name for now, it will be replaced by proper generated later
    device_list.remove("mydisk");
    instance_info_metadata["devices"] = device_list;

    const QJsonObject json_reply =
        lxd_request(network_manager_, "PUT", lxd_instance_endpoint_new_, instance_info_metadata);
    lxd_wait(network_manager_, multipass::lxd_socket_url, json_reply, 300000);
}

void LXDMountHandler::lxd_device_add()
{
    const QJsonObject instance_info = lxd_request(network_manager_, "GET", lxd_instance_endpoint_new_);
    QJsonObject instance_info_metadata = instance_info["metadata"].toObject();
    QJsonObject device_list = instance_info_metadata["devices"].toObject();

    const QJsonObject new_device_object{{"path", target.c_str()}, {"source", source.c_str()}, {"type", "disk"}};

    // mydisk is the hard coded name for now, it will be replaced by proper generated later
    device_list.insert("mydisk", new_device_object);
    instance_info_metadata["devices"] = device_list;

    const QJsonObject json_reply =
        lxd_request(network_manager_, "PUT", lxd_instance_endpoint_new_, instance_info_metadata);
    lxd_wait(network_manager_, multipass::lxd_socket_url, json_reply, 300000);
}

} // namespace multipass
