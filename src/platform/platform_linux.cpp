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
 */

#include <multipass/constants.h>
#include <multipass/format.h>
#include <multipass/platform.h>
#include <multipass/settings.h>
#include <multipass/virtual_machine_factory.h>

#include "backends/libvirt/libvirt_virtual_machine_factory.h"
#include "backends/qemu/qemu_virtual_machine_factory.h"
#include "backends/shared/linux/process.h"
#include "backends/shared/linux/process_factory.h"
#include "logger/journald_logger.h"
#include <disabled_update_prompt.h>

namespace mp = multipass;

namespace
{
static mp::ProcessFactory::UPtr static_process_factory;

mp::ProcessFactory* process_factory()
{
    if (!static_process_factory)
    {
        static_process_factory = std::make_unique<mp::ProcessFactory>();
    }
    return static_process_factory.get();
}
} // namespace

std::string mp::platform::default_server_address()
{
    return {"unix:/run/multipass_socket"};
}

QString mp::platform::default_driver()
{
    return QStringLiteral("qemu");
}

mp::VirtualMachineFactory::UPtr mp::platform::vm_backend(const mp::Path& data_dir)
{
    const auto& driver = Settings::instance().get(driver_key);
    if (driver == QStringLiteral("qemu"))
        return std::make_unique<QemuVirtualMachineFactory>(::process_factory(), data_dir);
    else if (driver == QStringLiteral("libvirt"))
        return std::make_unique<LibVirtVirtualMachineFactory>(::process_factory(), data_dir);
    else
    {
        std::string msg_tail{};
        if (driver == QStringLiteral("hyperkit") || driver == QStringLiteral("hyper-v"))
            msg_tail = " on this platform";

        throw std::runtime_error(fmt::format("Unsupported virtualization driver{}: {}", msg_tail, driver));
    }
}

mp::UpdatePrompt::UPtr mp::platform::make_update_prompt()
{
    return std::make_unique<DisabledUpdatePrompt>();
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
