/*
 * Copyright (C) 2017 Canonical, Ltd.
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
 * Authored by: Gerry Boland <gerry.boland@canonical.com>
 */

#include "hyperkit_virtual_machine_factory.h"
#include "hyperkit_virtual_machine.h"

#include <multipass/virtual_machine_description.h>

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QProcess>

#include <unistd.h> // getuid

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QProcess>

#include <unistd.h> // getuid

namespace mp = multipass;

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

    QProcess uncompress;
    qDebug() << QCoreApplication::applicationDirPath() + "/qemu-img" << uncompress_args;
    uncompress.start(QCoreApplication::applicationDirPath() + "/qemu-img", uncompress_args);
    uncompress.waitForFinished();

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
    if (!desc.disk_space.empty())
    {
        QProcess resize_image;

        QStringList resize_image_args(
            {QStringLiteral("resize"), instance_image.image_path, QString::fromStdString(desc.disk_space)});

        resize_image.start(QCoreApplication::applicationDirPath() + "/qemu-img", resize_image_args);
        resize_image.waitForFinished();
    }
}

void mp::HyperkitVirtualMachineFactory::configure(const std::string& name, YAML::Node& meta_config,
                                                  YAML::Node& user_config)
{
}

void mp::HyperkitVirtualMachineFactory::check_hypervisor_support()
{
}
