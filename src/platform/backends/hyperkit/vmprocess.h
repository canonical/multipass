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

#ifndef VMPROCESS_H
#define VMPROCESS_H

#include <QFile>
#include <QFileSystemWatcher>
#include <QProcess>

namespace multipass
{
class VirtualMachineDescription;
class VMProcessOutputMonitor;
class PtyReader;

class VMProcess : public QObject
{
    Q_OBJECT

public:
    VMProcess();
    virtual ~VMProcess();

public slots:
    void start(const VirtualMachineDescription& desc);
    void stop();

signals:
    void started();
    void stopped(bool unexpected = false);

private slots:
    void on_ready_read_standard_error();

private:
    std::unique_ptr<QProcess> vm_process;
    bool network_configured;
    std::string vm_name;
};

} // namespace multipass

Q_DECLARE_METATYPE(std::string)

#endif // VMPROCESS_H
