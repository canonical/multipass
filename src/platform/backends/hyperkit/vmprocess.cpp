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

#include "vmprocess.h"

#include "ptyreader.h"

#include <multipass/logging/log.h>
#include <multipass/virtual_machine_description.h>

#include <fmt/format.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaEnum>
#include <QTimer>

#include <sys/stat.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
QString dir(const QString& f)
{
    QFileInfo file(f);
    return file.absolutePath();
}

QByteArray generateUuid(const QByteArray& string)
{
    QByteArray uuid = QCryptographicHash::hash(string, QCryptographicHash::Md5);

    // Force bits to ensure it meets RFC-4122 v3 (MD5) standard...
    uuid[6] = (uuid[6] & 0x0f) | (3 << 4); // version '3' shifted left into top nibble
    uuid[8] = (uuid[8] & 0x3f) | 0x80;

    return uuid.toHex().insert(8, '-').insert(13, '-').insert(18, '-').insert(23, '-');
}

void throw_on_missing(const QStringList& files)
{
    QString missingFiles;
    for (const auto& file : files)
    {
        if (!QFile::exists(file))
        {
            missingFiles += file + ';';
        }
    }

    if (missingFiles.count() > 0)
    {
        throw std::runtime_error(qPrintable("Cannot start VM, files missing: " + missingFiles));
    }
}

auto make_hyperkit_process(const mp::VirtualMachineDescription& desc, const QString& pty)
{
    throw_on_missing({desc.image.image_path, desc.cloud_init_iso, desc.image.kernel_path, desc.image.initrd_path});

    QDir log_directory("/Library/Logs/Multipass/");
    if (!log_directory.exists())
    {
        mpl::log(mpl::Level::info, desc.vm_name,
                 fmt::format("creating log file dir {}", log_directory.path().toStdString()));
        log_directory.mkdir(log_directory.path());
        ::chmod(qPrintable(log_directory.path()), 0755);
    }

    QStringList args;
    args <<
        // Number of cpu cores
        "-c" << QString::number(desc.num_cores) <<
        // Memory to use for VM
        "-m" << QString::fromStdString(desc.mem_size) <<
        // RTC keeps UTC
        "-u" <<
        // ACPI tables
        "-A" <<
        // Send shutdown signal to VM on SIGTERM to hyperkit
        "-H" <<
        // VM having consistent UUID ensures it gets same IP address across reboots
        "-U" << generateUuid(QByteArray::fromStdString(desc.vm_name)) <<

        // PCI devices:
        // PCI host bridge
        "-s"
         << "0:0,hostbridge" <<
        // Network (root-only)
        "-s"
         << "2:0,virtio-net" <<
        // Entropy device emulation.
        "-s"
         << "5,virtio-rnd" <<
        // LPC = low-pin-count device, used for serial console
        "-s"
         << "31,lpc" <<
        // Forward all console output to a pseudo TTY of our choice, as well as to fixed-size circular log file
        "-l"
         << "com1,autopty=" + pty + ",log=" + log_directory.path() + "/" +
                QFile::encodeName(QString::fromStdString(desc.vm_name)) + "-hyperkit.log"
         <<
        // The VM image itself
        "-s"
         << "1:0,ahci-hd,file://" + desc.image.image_path +
                "?sync=os&buffered=1,format=qcow,qcow-config=discard=true;compact_after_unmaps=0;keep_"
                "erased=0;runtime_asserts=false"
         <<
        // Disk image for the cloud-init configuration
        "-s"
         << "1:1,ahci-cd," + desc.cloud_init_iso
         << "-f"
         // Firmware argument
         << "kexec," + desc.image.kernel_path + "," + desc.image.initrd_path +
                ",earlyprintk=serial console=ttyS0 root=/dev/sda1 rw panic=1 no_timer_check";

    /* Notes on some of the kernel parameters above
     *  - panic=1 causes the kernel to reboot the system 1 second after a panic. Necessary as otherwise
     *    hyperkit will hang indefinitely (0 disable reboot entirely)
     *  - no_timer_check - as the OSX scheduler may interrupt the hyperkit process at any time, it can interfere
     *    with the kernel's timer checks, causing a panic if the timer fails those checks. This is frequently set
     *    for virtualised kernels, e.g. https://lists.fedoraproject.org/pipermail/cloud/2014-June/003975.html
     */

    auto process = std::make_unique<QProcess>();
    process->setProgram(QCoreApplication::applicationDirPath() + "/hyperkit");
    process->setArguments(args);

    mpl::log(mpl::Level::debug, desc.vm_name,
             fmt::format("process working dir '{}'", process->workingDirectory().toStdString()));
    mpl::log(mpl::Level::info, desc.vm_name, fmt::format("process program '{}'", process->program().toStdString()));
    mpl::log(mpl::Level::info, desc.vm_name,
             fmt::format("process arguments '{}'", process->arguments().join(", ").toStdString()));

    return process;
}
} // namespace

mp::VMProcess::VMProcess() : network_configured(false)
{
    qRegisterMetaType<std::string>();
}

mp::VMProcess::~VMProcess()
{
    stop();
}

void mp::VMProcess::start(const VirtualMachineDescription& desc)
{
    network_configured = false;
    const QString pty = QString("%1/pty").arg(dir(desc.image.image_path));
    vm_process = make_hyperkit_process(desc, pty);
    vm_name = desc.vm_name;

    /* Hyperkit process monitoring */
    connect(vm_process.get(), &QProcess::started, this, &VMProcess::started);

    connect(vm_process.get(), &QProcess::readyReadStandardOutput,
            [this]() { mpl::log(mpl::Level::info, vm_name, vm_process->readAllStandardOutput().constData()); });
    connect(vm_process.get(), &QProcess::readyReadStandardError, this, &mp::VMProcess::on_ready_read_standard_error);

    connect(vm_process.get(), &QProcess::stateChanged, [this](QProcess::ProcessState newState) {
        auto meta = QMetaEnum::fromType<QProcess::ProcessState>();
        mpl::log(mpl::Level::info, vm_name, fmt::format("process state changed to {}", meta.valueToKey(newState)));
    });

    connect(vm_process.get(), static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [this](int exitCode, QProcess::ExitStatus exitStatus) {
                mpl::log(mpl::Level::info, vm_name, fmt::format("process finished with exit code {}", exitCode));
                emit stopped(exitCode != QProcess::NormalExit);
            });

    connect(vm_process.get(), &QProcess::errorOccurred, [this](QProcess::ProcessError error) {
        auto meta = QMetaEnum::fromType<QProcess::ProcessError>();
        mpl::log(mpl::Level::error, vm_name, fmt::format("process error occurred {}", meta.valueToKey(error)));
        emit stopped(true);
    });

    /* Hyperkit PTY socket monitoring and opening
     *
     * We've specified to hyperkit the desired path for the PTY socket, and need to wait for it to be
     * created before connecting to it and reading.
     */
    if (QFile::exists(pty))
        QFile::remove(pty);

    mpl::log(mpl::Level::info, vm_name, fmt::format("setting up watch for PTY in dir {}", dir(pty).toStdString()));
    pty_watcher.addPath(dir(pty));

    connect(&pty_watcher, &QFileSystemWatcher::directoryChanged, this, &mp::VMProcess::pty_available);
    vm_process->start();
}

void mp::VMProcess::stop()
{
    if (!vm_process || vm_process->state() == QProcess::NotRunning)
        return;

    if (vm_process->state() == QProcess::Starting)
    {
        mpl::log(mpl::Level::info, vm_name, "hyperkit in starting state, waiting for it to finish booting");
        vm_process->waitForStarted(10000);
    }

    mpl::log(mpl::Level::info, vm_name, "sending shutdown signal to hyperkit process, waiting for it to shutdown...");
    // hyperkit intercepts a SIGTERM signal and sends shutdown signal to the VM
    vm_process->terminate();

    if (!vm_process->waitForFinished(15000))
    {
        mpl::log(mpl::Level::info, vm_name, "hyperkit not responding to shutdown signal, killing it");
        vm_process->kill();
    }

    vm_process.reset();
}

void mp::VMProcess::pty_available(const QString& dir)
{
    const QString pty = QString("%1/pty").arg(dir);
    if (QFile::exists(pty))
    {
        pty_watcher.removePath(dir);
        QObject::disconnect(&pty_watcher, &QFileSystemWatcher::directoryChanged, nullptr, nullptr);

        mpl::log(mpl::Level::info, vm_name, fmt::format("hyperkit PTY found, opening {}", pty.toStdString()));
        try
        {
            pty_reader = std::make_unique<PtyReader>(pty);
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::error, vm_name, fmt::format("failed to instantiate PtyReader: {}", e.what()));
            stop(); // FIXME: is this the most suitable decision?
            return;
        }

        connect(pty_reader.get(), &mp::PtyReader::lineRead, this, &mp::VMProcess::console_output);
    }
}

void mp::VMProcess::console_output(const QByteArray& line)
{
    if (!network_configured)
    {
        // Need to use Regex to parse VM output to find the IP address - happens to be the first
        // mention of 192.168.*.* in the output.
        QRegExp ip_regex("192\\.168\\.(\\d{1,3})\\.(\\d{1,3})");
        if (ip_regex.indexIn(line) > -1)
        {
            uint8_t third = ip_regex.cap(1).toUInt();
            uint8_t fourth = ip_regex.cap(2).toUInt();

            std::string ip_address = "192.168." + std::to_string(third) + "." + std::to_string(fourth);
            mpl::log(mpl::Level::info, vm_name, fmt::format("IP address found: {}", ip_address.c_str()));
            emit ip_address_found(ip_address);
            network_configured = true;
        }
    }
}

void mp::VMProcess::on_ready_read_standard_error()
{
    // Hyperkit has no consistent error output format, so am using a naive filter to distinguish
    // informative messages from errors.
    const auto lines = vm_process->readAllStandardError().split('\n');
    for (auto line : lines)
    {
        line = line.trimmed();
        if (!line.isEmpty())
        {
            if (line.contains("[INFO]") || line.contains("fcntl(F_PUNCHHOLE)") || line.contains("rdmsr to register"))
            {
                mpl::log(mpl::Level::info, vm_name, line.constData());
            }
            else
            {
                mpl::log(mpl::Level::error, vm_name, line.constData());
            }
        }
    }
}
