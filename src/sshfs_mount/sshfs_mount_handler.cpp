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
    auto stop_conn =
        QObject::connect(process, &mp::Process::finished, &event_loop, &QEventLoop::quit);
    auto running_conn =
        QObject::connect(process,
                         &mp::Process::ready_read_standard_output,
                         [process, &event_loop]() {
                             if (process->read_all_standard_output().contains(
                                     "Connected")) // Magic string printed by sshfs_server
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
        mpl::warn(category, "Snap support is not installed in '{}'", name);
        throw std::runtime_error(fmt::format(
            "Snap support needs to be installed in '{}' in order to support mounts.\n"
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
        mpl::debug(category, "The multipass-sshfs snap is already installed on '{}'", name);
        return true;
    }

    // Check if /snap exists for "classic" snap support
    if (session.exec("[ -e /snap ]").exit_code() != 0)
    {
        mpl::warn(category, "Classic snap support symlink is needed in '{}'", name);
        throw std::runtime_error(
            fmt::format("Classic snap support is not enabled for '{}'!\n\n"
                        "Please see https://docs.snapcraft.io/installing-snapd for information on\n"
                        "how to enable classic snap support for your instance's distribution.",
                        name));
    }

    return false;
}

void install_sshfs_for(const std::string& name,
                       mp::SSHSession& session,
                       const std::chrono::milliseconds& timeout)
try
{
    mpl::info(category, "Installing the multipass-sshfs snap in '{}'", name);
    auto proc = session.exec("sudo snap install multipass-sshfs");
    if (proc.exit_code(timeout) != 0)
    {
        auto error_msg = proc.read_std_error();
        mpl::error(category, "Failed to install 'multipass-sshfs': {}", mpu::trim_end(error_msg));
        throw mp::SSHFSMissingError();
    }
}
catch (const mp::ExitlessSSHProcessException& e)
{
    mpl::error(category, "Could not install 'multipass-sshfs' in '{}': {}", name, e.what());
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
    mpl::info(category,
              "initializing mount {} => {} in '{}'",
              this->mount_spec.get_source_path(),
              target,
              vm->vm_name);
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

    process.reset(platform::make_sshfs_server_process(config).release());

    QObject::connect(process.get(), &Process::finished, [this](const ProcessState& exit_state) {
        if (exit_state.completed_successfully())
        {
            mpl::info(category, "Mount \"{}\" in instance '{}' has stopped", target, vm->vm_name);
        }
        else
        {
            // not error as it failing can indicate we need to install sshfs in the VM
            mpl::warn(category,
                      "Mount \"{}\" in instance '{}' has stopped unsuccessfully: {}",
                      target,
                      vm->vm_name,
                      exit_state.failure_message());
        }
    });
    QObject::connect(
        process.get(),
        &Process::error_occurred,
        [this](auto error, auto error_string) {
            mpl::error(
                category,
                "There was an error with sshfs_server for instance '{}' with path \"{}\": {} - {}",
                vm->vm_name,
                target,
                mpu::qenum_to_string(error),
                error_string);
        });

    mpl::info(category, "process program '{}'", process->program());
    mpl::info(category, "process arguments '{}'", process->arguments().join(", "));

    start_and_block_until_connected(process.get());
    // after the process is started, it must be moved to the main thread
    // when starting an instance that already has mounts defined, it is started in a thread from the
    // global thread pool this makes the sshfs_server process to be owned by that thread when
    // stopping the mount from the main thread again, qt will try to send an event from the main
    // thread to the one in which the process lives this will result in an error since qt can't send
    // events from one thread to another
    process->moveToThread(QCoreApplication::instance()->thread());
    // So, for any future travelers, this^ is the main reason why we use qt_delete_later_unique_ptr
    // thingy.

    // Check in case sshfs_server stopped, usually due to an error
    const auto process_state = process->process_state();
    if (process_state.exit_code == 9) // Magic number returned by sshfs_server
        throw SSHFSMissingError();
    else if (process_state.exit_code || process_state.error)
        throw std::runtime_error(fmt::format("{}: {}",
                                             process_state.failure_message(),
                                             process->read_all_standard_error()));
}

void SSHFSMountHandler::deactivate_impl(bool force)
{
    mpl::info(category, "Stopping mount \"{}\" in instance '{}'", target, vm->vm_name);
    QObject::disconnect(process.get(), &Process::error_occurred, nullptr, nullptr);

    constexpr auto kProcessWaitTimeout = std::chrono::milliseconds{5000};
    if (process->terminate(); !process->wait_for_finished(kProcessWaitTimeout.count()))
    {
        auto fetch_stderr = [](Process& process) {
            return fmt::format("Failed to terminate SSHFS mount process gracefully: {}",
                               process.read_all_standard_error());
        };

        const auto err = fetch_stderr(*process.get());

        if (force)
        {
            mpl::warn(category,
                      "Failed to gracefully stop mount \"{}\" in instance '{}': {}, trying to stop "
                      "it forcefully.",
                      target,
                      vm->vm_name,
                      err);
            /**
             * Let's try brute force this time.
             */
            process->kill();
            const auto result = process->wait_for_finished(kProcessWaitTimeout.count());

            mpl::warn(category,
                      "{} to forcefully stop mount \"{}\" in instance '{}': {}",
                      result ? "Succeeded" : "Failed",
                      target,
                      vm->vm_name,
                      result ? "" : fetch_stderr(*process.get()));
        }
        else
            throw std::runtime_error{err};
    }

    // Finally, call reset() to disconnect all the signals and defer the deletion
    // to the owning thread, in this case it's the QT main.
    process.reset();
}

SSHFSMountHandler::~SSHFSMountHandler()
{
    deactivate(/*force=*/true);
}
} // namespace multipass
