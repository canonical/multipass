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

#include <hyperv_api/hcs_plan9_mount_handler.h>

#include <hyperv_api/hcs/hyperv_hcs_wrapper_interface.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>

#include <fmt/format.h>

namespace multipass::hyperv::hcs
{

namespace mpu = utils;

constexpr auto kLogCategory = "hcs-plan9-mount-handler";

Plan9MountHandler::Plan9MountHandler(VirtualMachine* vm,
                                     const SSHKeyProvider* ssh_key_provider,
                                     VMMount mount_spec,
                                     const std::string& target,
                                     hcs_sptr_t hcs_w)
    : MountHandler(vm, ssh_key_provider, mount_spec, target), hcs{hcs_w}
{
    // No need to do anything special.
    if (nullptr == hcs)
    {
        throw std::invalid_argument{"HCS API wrapper object cannot be null."};
    }
    if (nullptr == vm)
    {
        throw std::invalid_argument{"VM pointer cannot be null."};
    }
}

Plan9MountHandler::~Plan9MountHandler() = default;

void Plan9MountHandler::activate_impl(ServerVariant server, std::chrono::milliseconds timeout)
{
    // https://github.com/microsoft/hcsshim/blob/d7e384230944f153215473fa6c715b8723d1ba47/internal/vm/hcs/plan9.go#L13
    // https://learn.microsoft.com/en-us/virtualization/api/hcs/schemareference#System_PropertyType
    // https://github.com/microsoft/hcsshim/blob/d7e384230944f153215473fa6c715b8723d1ba47/internal/hcs/schema2/plan9_share.go#L12
    // https://github.com/microsoft/hcsshim/blob/d7e384230944f153215473fa6c715b8723d1ba47/internal/vm/hcs/builder.go#L53
    const auto req = [this] {
        hcs::HcsAddPlan9ShareParameters params{};
        params.access_name = mpu::make_uuid(target).remove("-").left(30).prepend('m').toStdString();
        params.name = mpu::make_uuid(target).remove("-").left(30).prepend('m').toStdString();
        params.host_path = mount_spec.get_source_path();
        return hcs::HcsRequest{hcs::HcsResourcePath::Plan9Shares(),
                               hcs::HcsRequestType::Add(),
                               params};
    }();

    const auto result = hcs->modify_compute_system(vm->vm_name, req);

    if (!result)
    {
        throw std::runtime_error{"Failed to create a Plan9 share for the mount"};
    }

    try
    {
        // The host side 9P share setup is done. Let's handle the guest side.
        SSHSession session{vm->ssh_hostname(),
                           vm->ssh_port(),
                           vm->ssh_username(),
                           *ssh_key_provider};

        // Split the path in existing and missing parts
        // We need to create the part of the path which does not still exist, and set then the
        // correct ownership.
        if (const auto& [leading, missing] = mpu::get_path_split(session, target); missing != ".")
        {
            const auto default_uid = std::stoi(MP_UTILS.run_in_ssh_session(session, "id -u"));
            mpl::log(mpl::Level::debug,
                     kLogCategory,
                     fmt::format("{}:{} {}(): `id -u` = {}",
                                 __FILE__,
                                 __LINE__,
                                 __FUNCTION__,
                                 default_uid));
            const auto default_gid = std::stoi(MP_UTILS.run_in_ssh_session(session, "id -g"));
            mpl::log(mpl::Level::debug,
                     kLogCategory,
                     fmt::format("{}:{} {}(): `id -g` = {}",
                                 __FILE__,
                                 __LINE__,
                                 __FUNCTION__,
                                 default_gid));

            mpu::make_target_dir(session, leading, missing);
            mpu::set_owner_for(session, leading, missing, default_uid, default_gid);
        }

        // fmt::format("sudo mount -t 9p {} {} -o trans=virtio,version=9p2000.L,msize=536870912",
        // tag, target));
        constexpr std::string_view mount_command_fmtstr =
            "sudo mount -t 9p -o trans=virtio,version=9p2000.L,port={} {} {}";

        const auto& add_settings = std::get<hcs::HcsAddPlan9ShareParameters>(req.settings);
        const auto mount_command =
            fmt::format(mount_command_fmtstr, add_settings.port, add_settings.access_name, target);

        auto mount_command_result = session.exec(mount_command);

        if (mount_command_result.exit_code() == 0)
        {
            mpl::info(kLogCategory,
                      "Successfully mounted 9P share `{}` to VM `{}`",
                      req,
                      vm->vm_name);
        }
        else
        {
            mpl::error(kLogCategory,
                       "stdout: {} stderr: {}",
                       mount_command_result.read_std_output(),
                       mount_command_result.read_std_error());
            throw std::runtime_error{"Failed to mount the Plan9 share"};
        }
    }
    catch (...)
    {
        const auto req = [this] {
            hcs::HcsRemovePlan9ShareParameters params{};
            params.name = mpu::make_uuid(target).remove("-").left(30).prepend('m').toStdString();
            params.access_name =
                mpu::make_uuid(target).remove("-").left(30).prepend('m').toStdString();
            return hcs::HcsRequest{hcs::HcsResourcePath::Plan9Shares(),
                                   hcs::HcsRequestType::Remove(),
                                   params};
        }();
        if (!hcs->modify_compute_system(vm->vm_name, req))
        {
            // TODO: Warn here
        }
    }
}
void Plan9MountHandler::deactivate_impl(bool force)
{
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};
    constexpr std::string_view umount_command_fmtstr =
        "mountpoint -q {0}; then sudo umount {0}; else true; fi";
    const auto umount_command = fmt::format(umount_command_fmtstr, target);

    if (!(session.exec(umount_command).exit_code() == 0))
    {
        // TODO: Include output?
        mpl::warn(kLogCategory, "Plan9 share unmount failed.");

        if (!force)
        {
            return;
        }
    }

    const auto req = [this] {
        hcs::HcsRemovePlan9ShareParameters params{};
        params.name = mpu::make_uuid(target).remove("-").left(30).prepend('m').toStdString();
        params.access_name = mpu::make_uuid(target).remove("-").left(30).prepend('m').toStdString();
        return hcs::HcsRequest{hcs::HcsResourcePath::Plan9Shares(),
                               hcs::HcsRequestType::Remove(),
                               params};
    }();

    if (!hcs->modify_compute_system(vm->vm_name, req))
    {
        mpl::warn(kLogCategory, "Plan9 share removal failed.");
    }
}

// No need for custom active logic.

} // namespace multipass::hyperv::hcs
