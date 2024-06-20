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

#include <multipass/exceptions/exitless_sshprocess_exceptions.h>
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/platform.h>
#include <multipass/sshfs_mount/sshfs_mount_handler.h>
#include <multipass/utils.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>

namespace mp = multipass;
namespace mpl = multipass::logging;
namespace mpu = multipass::utils;

namespace
{
constexpr auto category = "sshfs-mount-handler";

void start_and_block_until_connected(mp::Process* process)
{
    QEventLoop event_loop;
    auto stop_conn = QObject::connect(process, &mp::Process::finished, &event_loop, &QEventLoop::quit);
    auto running_conn = QObject::connect(process, &mp::Process::ready_read_standard_output, [process, &event_loop]() {
        if (process->read_all_standard_output().contains("Connected")) // Magic string printed by sshfs_server
            event_loop.quit();
    });

    process->start();

    // This blocks and waits either for "Connected" or for failure.
    event_loop.exec();

    QObject::disconnect(stop_conn);
    QObject::disconnect(running_conn);
}

bool has_sshfs(const std::string& name, mp::SSHSession& session)
{
    // Check if snap support is installed in the instance
    if (session.exec("which snap").exit_code() != 0)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Snap support is not installed in '{}'", name));
        throw std::runtime_error(
            fmt::format("Snap support needs to be installed in '{}' in order to support mounts.\n"
                        "Please see https://docs.snapcraft.io/installing-snapd for information on\n"
                        "how to install snap support for your instance's distribution.\n\n"
                        "If your distribution's instructions specify enabling classic snap support,\n"
                        "please do that as well.\n\n"
                        "Alternatively, install `sshfs` manually inside the instance.",
                        name));
    }

    // Check if multipass-sshfs is already installed
    if (session.exec("sudo snap list multipass-sshfs").exit_code(std::chrono::seconds(15)) == 0)
    {
        mpl::log(mpl::Level::debug, category,
                 fmt::format("The multipass-sshfs snap is already installed on '{}'", name));
        return true;
    }

    // Check if /snap exists for "classic" snap support
    if (session.exec("[ -e /snap ]").exit_code() != 0)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Classic snap support symlink is needed in '{}'", name));
        throw std::runtime_error(
            fmt::format("Classic snap support is not enabled for '{}'!\n\n"
                        "Please see https://docs.snapcraft.io/installing-snapd for information on\n"
                        "how to enable classic snap support for your instance's distribution.",
                        name));
    }

    return false;
}

void install_sshfs_for(const std::string& name, mp::SSHSession& session, const std::chrono::milliseconds& timeout)
try
{
    mpl::log(mpl::Level::info, category, fmt::format("Installing the multipass-sshfs snap in '{}'", name));
    auto proc = session.exec("sudo snap install multipass-sshfs");
    if (proc.exit_code(timeout) != 0)
    {
        auto error_msg = proc.read_std_error();
        mpl::log(mpl::Level::error,
                 category,
                 fmt::format("Failed to install 'multipass-sshfs': {}", mpu::trim_end(error_msg)));
        throw mp::SSHFSMissingError();
    }
}
catch (const mp::ExitlessSSHProcessException& e)
{
    mpl::log(mpl::Level::error,
             category,
             fmt::format("Could not install 'multipass-sshfs' in '{}': {}", name, e.what()));
    throw mp::SSHFSMissingError();
}
} // namespace

namespace multipass
{
SSHFSMountHandler::SSHFSMountHandler(VirtualMachine* vm,
                                     const SSHKeyProvider* ssh_key_provider,
                                     const std::string& target,
                                     VMMount mount_spec)
    : MountHandler{vm, ssh_key_provider, std::move(mount_spec), target},
      process{nullptr},
      config{"",
             0,
             vm->ssh_username(),
             vm->vm_name,
             ssh_key_provider->private_key_as_base64(),
             source,
             target,
             this->mount_spec.get_gid_mappings(),
             this->mount_spec.get_uid_mappings()}
{
    mpl::log(
        mpl::Level::info,
        category,
        fmt::format("initializing mount {} => {} in '{}'", this->mount_spec.get_source_path(), target, vm->vm_name));
}

bool SSHFSMountHandler::is_active()
try
{
    if (!active || !process || !process->running())
        return false;

    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};

    const auto resolved_target = mp::utils::get_resolved_target(session, target);

    return !session.exec(fmt::format("findmnt --type fuse.sshfs | grep -E '^{} +:{}'", resolved_target, source))
                .exit_code();
}
catch (const std::exception& e)
{
    mpl::log(mpl::Level::warning, category,
             fmt::format("Failed checking SSHFS mount \"{}\" in instance '{}': {}", target, vm->vm_name, e.what()));
    return false;
}

void SSHFSMountHandler::activate_impl(ServerVariant server, std::chrono::milliseconds timeout)
{
    SSHSession session{vm->ssh_hostname(), vm->ssh_port(), vm->ssh_username(), *ssh_key_provider};
    if (!has_sshfs(vm->vm_name, session))
    {
        auto visitor = [](auto server) {
            if (server)
            {
                auto reply = make_reply_from_server(server);
                reply.set_reply_message("Enabling support for mounting");
                server->Write(reply);
            }
        };
        std::visit(visitor, server);
        install_sshfs_for(vm->vm_name, session, timeout);
    }

    // Can't obtain hostname/IP address until instance is running
    config.host = vm->ssh_hostname();
    config.port = vm->ssh_port();

    if (process)
        process.reset();
    process.reset(platform::make_sshfs_server_process(config).release());

    QObject::connect(process.get(), &Process::finished, [this](const ProcessState& exit_state) {
        if (exit_state.completed_successfully())
        {
            mpl::log(mpl::Level::info, category,
                     fmt::format("Mount \"{}\" in instance '{}' has stopped", target, vm->vm_name));
        }
        else
        {
            // not error as it failing can indicate we need to install sshfs in the VM
            mpl::log(mpl::Level::warning, category,
                     fmt::format("Mount \"{}\" in instance '{}' has stopped unsuccessfully: {}", target, vm->vm_name,
                                 exit_state.failure_message()));
        }
    });
    QObject::connect(process.get(), &Process::error_occurred, [this](auto error, auto error_string) {
        mpl::log(mpl::Level::error, category,
                 fmt::format("There was an error with sshfs_server for instance '{}' with path \"{}\": {} - {}",
                             vm->vm_name, target, mpu::qenum_to_string(error), error_string));
    });

    mpl::log(mpl::Level::info, category, fmt::format("process program '{}'", process->program()));
    mpl::log(mpl::Level::info, category, fmt::format("process arguments '{}'", process->arguments().join(", ")));

    start_and_block_until_connected(process.get());
    // after the process is started, it must be moved to the main thread
    // when starting an instance that already has mounts defined, it is started in a thread from the global thread pool
    // this makes the sshfs_server process to be owned by that thread
    // when stopping the mount from the main thread again, qt will try to send an event from the main thread to the one
    // in which the process lives this will result in an error since qt can't send events from one thread to another
    process->moveToThread(QCoreApplication::instance()->thread());

    // Check in case sshfs_server stopped, usually due to an error
    auto process_state = process->process_state();
    if (process_state.exit_code == 9) // Magic number returned by sshfs_server
        throw SSHFSMissingError();
    else if (process_state.exit_code || process_state.error)
        throw std::runtime_error(
            fmt::format("{}: {}", process_state.failure_message(), process->read_all_standard_error()));
}

void SSHFSMountHandler::deactivate_impl(bool force)
{
    mpl::log(mpl::Level::info, category, fmt::format("Stopping mount \"{}\" in instance '{}'", target, vm->vm_name));
    QObject::disconnect(process.get(), &Process::error_occurred, nullptr, nullptr);
    if (process->terminate(); !process->wait_for_finished(5000))
    {
        auto err = fmt::format("Failed to terminate SSHFS mount process: {}", process->read_all_standard_error());
        if (force)
            mpl::log(
                mpl::Level::warning, category,
                fmt::format("Failed to gracefully stop mount \"{}\" in instance '{}': {}", target, vm->vm_name, err));
        else
            throw std::runtime_error{err};
    }
    process.reset();
}

SSHFSMountHandler::~SSHFSMountHandler()
{
    deactivate(/*force=*/true);
}
} // namespace multipass
