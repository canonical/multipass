/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/backend_utils.h>
#include <multipass/utils.h>

#include <fmt/format.h>

#include <QProcess>
#include <QString>
#include <QSysInfo>

#include <chrono>
#include <exception>
#include <random>

namespace mp = multipass;

namespace
{
std::default_random_engine gen;
std::uniform_int_distribution<int> dist{0, 255};

bool subnet_used_locally(const std::string& subnet)
{
    const auto ip_cmd = fmt::format("ip -4 route show | grep -q {}", subnet);
    return mp::utils::run_cmd_for_status("bash", {"-c", ip_cmd.c_str()});
}

bool can_reach_gateway(const std::string& ip)
{
    return mp::utils::run_cmd_for_status("ping", {"-n", "-q", ip.c_str(), "-c", "-1", "-W", "1"});
}
}

std::string mp::backend::generate_random_subnet()
{
    gen.seed(std::chrono::system_clock::now().time_since_epoch().count());
    for (auto i = 0; i < 100; ++i)
    {
        auto subnet = fmt::format("10.{}.{}", dist(gen), dist(gen));
        if (subnet_used_locally(subnet))
            continue;

        if (can_reach_gateway(fmt::format("{}.1", subnet)))
            continue;

        if (can_reach_gateway(fmt::format("{}.254", subnet)))
            continue;

        return subnet;
    }

    throw std::runtime_error("Could not determine a subnet for networking.");
}

std::string mp::backend::generate_virtual_bridge_name(const std::string& base_name)
{
    std::string bridge_name;

    for (auto i = 0; i < 100; ++i)
    {
        bridge_name = fmt::format("{}{}", base_name, i);

        if (!mp::utils::run_cmd_for_status("ip", {"addr", "show", QString::fromStdString(bridge_name)}))
            return bridge_name;
    }

    return "";
}

void mp::backend::check_hypervisor_support()
{
    auto arch = QSysInfo::currentCpuArchitecture();
    if (arch == "x86_64" || arch == "i386")
    {
        QProcess check_kvm;
        check_kvm.start("check_kvm_support");
        check_kvm.waitForFinished();

        if (check_kvm.exitCode() == 1)
        {
            throw std::runtime_error(check_kvm.readAll().trimmed().toStdString());
        }
    }
}

void mp::backend::resize_instance_image(const std::string& disk_space, const mp::Path& image_path)
{
    auto disk_size = QString::fromStdString(disk_space);

    if (disk_size.endsWith("B"))
        disk_size.chop(1);

    if (!mp::utils::run_cmd_for_status("qemu-img", {QStringLiteral("resize"), image_path, disk_size}))
        throw std::runtime_error("Cannot resize instance image");
}
