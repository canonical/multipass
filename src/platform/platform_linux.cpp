/*
 * Copyright (C) 2017-2019 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include <multipass/platform.h>

#include <multipass/logging/log.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/backend_utils/apparmored_process_factory.h"
#include "backends/backend_utils/process.h"
#include "backends/backend_utils/process_factory.h"
#include "backends/backend_utils/snap_utils.h"
#include "backends/backend_utils/sshfs_server_process_spec.h"
#include "backends/libvirt/libvirt_virtual_machine_factory.h"
#include "backends/qemu/qemu_virtual_machine_factory.h"
#include "logger/journald_logger.h"

namespace mp = multipass;
namespace ms = multipass::snap;
namespace mpl = multipass::logging;

namespace
{
auto category = "security";
static mp::ProcessFactory::UPtr static_process_factory;

mp::ProcessFactory* process_factory()
{
    if (!static_process_factory)
    {
        auto driver = qgetenv("MULTIPASS_VM_DRIVER");

        if (!qEnvironmentVariableIsSet("DISABLE_APPARMOR") && driver != "LIBVIRT")
        {
            mpl::log(mpl::Level::info, category, "AppArmor enabled");
            static_process_factory = std::make_unique<mp::AppArmoredProcessFactory>();
        }
        else
        {
            mpl::log(mpl::Level::info, category, "Warning: AppArmor disabled");
            static_process_factory = std::make_unique<mp::ProcessFactory>();
        }
    }
    return static_process_factory.get();
}
} // namespace

std::string mp::platform::default_server_address()
{
    std::string base_dir;

    // if Snap confined, client and daemon can both access $SNAP_COMMON
    if (ms::is_snap_confined())
    {
        base_dir = ms::snap_common_dir().toStdString();
    }
    else
    {
        base_dir = "/run";
    }
    return "unix:" + base_dir + "/multipass_socket";
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir)
{
    auto driver = qgetenv("MULTIPASS_VM_DRIVER");

    if (driver.isEmpty() || driver == "QEMU")
    {
        return std::make_unique<QemuVirtualMachineFactory>(::process_factory(), data_dir);
    }
    else if (driver == "LIBVIRT")
    {
        return std::make_unique<LibVirtVirtualMachineFactory>(::process_factory(), data_dir);
    }

    throw std::runtime_error("Invalid virtualization driver set in the environment");
}

std::unique_ptr<QProcess> mp::platform::make_sshfs_server_process(const mp::SSHFSServerConfig& config)
{
    return ::process_factory()->create_process(std::make_unique<mp::SSHFSServerProcessSpec>(config));
}

mp::logging::Logger::UPtr mp::platform::make_logger(mp::logging::Level level)
{
    return std::make_unique<logging::JournaldLogger>(level);
}

bool mp::platform::link(const char* target, const char* link)
{
    return ::link(target, link) == 0;
}

bool mp::platform::is_alias_supported(const std::string& alias, const std::string& remote)
{
    return true;
}

bool mp::platform::is_remote_supported(const std::string& remote)
{
    return true;
}

bool mp::platform::is_image_url_supported()
{
    return true;
}
