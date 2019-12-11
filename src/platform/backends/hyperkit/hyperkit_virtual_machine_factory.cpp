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

#include "hyperkit_virtual_machine_factory.h"
#include "hyperkit_virtual_machine.h"

#include <multipass/logging/log.h>
#include <multipass/virtual_machine_description.h>

#include <fmt/format.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QProcess>

#include <unistd.h> // getuid

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
const int conversion_timeout = 300000; // Timeout in milliseconds
constexpr auto category = "hyperkit-factory";
}

mp::VirtualMachine::UPtr
mp::HyperkitVirtualMachineFactory::create_virtual_machine(const VirtualMachineDescription& desc,
                                                          VMStatusMonitor& monitor)
{
    return std::make_unique<HyperkitVirtualMachine>(desc, monitor);
}

void mp::HyperkitVirtualMachineFactory::remove_resources_for(const std::string& name)
{
}

mp::HyperkitVirtualMachineFactory::HyperkitVirtualMachineFactory()
{
    if (getuid())
    {
        throw std::runtime_error("multipassd needs to run as root");
    }
}

mp::FetchType mp::HyperkitVirtualMachineFactory::fetch_type()
{
    return mp::FetchType::ImageKernelAndInitrd;
}

mp::VMImage mp::HyperkitVirtualMachineFactory::prepare_source_image(const VMImage& source_image)
{
    // QCow2 Image needs to be uncompressed before hyperkit/xhyve can boot from it
    QFileInfo compressed_file(source_image.image_path);
    QString uncompressed_file =
        QString("%1/%2.qcow2").arg(compressed_file.path()).arg(compressed_file.completeBaseName());

    QStringList uncompress_args(
        {QStringLiteral("convert"), "-p", "-O", "qcow2", source_image.image_path, uncompressed_file});

    mpl::log(mpl::Level::debug, category,
             fmt::format("app path '{}'", QCoreApplication::applicationDirPath().toStdString()));
    mpl::log(mpl::Level::debug, category, fmt::format("qemu-img {}", uncompress_args.join(", ").toStdString()));

    QProcess uncompress;
    uncompress.start(QCoreApplication::applicationDirPath() + "/qemu-img", uncompress_args);
    if (!uncompress.waitForFinished(conversion_timeout))
    {
        if (uncompress.state() == QProcess::Running)
        {
            uncompress.terminate();
        }

        throw std::runtime_error("Timed out waiting for source image conversion");
    }

    if (uncompress.exitCode() != QProcess::NormalExit)
    {
        throw std::runtime_error(
            qPrintable("Decompression of image failed with error: " + uncompress.readAllStandardError()));
    }
    if (!QFile::exists(uncompressed_file))
    {
        throw std::runtime_error("Decompressed image file missing!");
    }

    QFile::remove(source_image.image_path);
    QFile::rename(uncompressed_file, source_image.image_path);

    return source_image;
}

void mp::HyperkitVirtualMachineFactory::prepare_instance_image(const mp::VMImage& instance_image,
                                                               const VirtualMachineDescription& desc)
{
    QProcess resize_image;

    auto disk_size =
        QString::number(desc.disk_space.in_bytes()); // format documented in `man qemu-img` (look for "size")

    QStringList resize_image_args({QStringLiteral("resize"), instance_image.image_path, disk_size});

    resize_image.start(QCoreApplication::applicationDirPath() + "/qemu-img", resize_image_args);
    resize_image.waitForFinished();
}

void mp::HyperkitVirtualMachineFactory::configure(const std::string& name, YAML::Node& meta_config,
                                                  YAML::Node& user_config)
{
}

void mp::HyperkitVirtualMachineFactory::hypervisor_health_check()
{
}
