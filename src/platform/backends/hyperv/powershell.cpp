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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "powershell.h"

#include <QDebug>
#include <QProcess>

namespace mp = multipass;

bool mp::powershell_run(QStringList args)
{
    QProcess power_shell;
    power_shell.setProgram("powershell.exe");
    qDebug() << "QProcess::program" << power_shell.program();
    power_shell.setArguments(args);
    qDebug() << "QProcess::arguments" << power_shell.arguments();

    QObject::connect(&power_shell, &QProcess::started, []() { qDebug() << "powershell started"; });
    QObject::connect(&power_shell, &QProcess::readyReadStandardOutput,
                     [&]() { qDebug("powershell.out: %s", power_shell.readAllStandardOutput().data()); });

    QObject::connect(&power_shell, &QProcess::readyReadStandardError,
                     [&]() { qDebug("powershell.err: %s", power_shell.readAllStandardError().data()); });

    QObject::connect(&power_shell, &QProcess::stateChanged, [](QProcess::ProcessState newState) {
        qDebug() << "QProcess::stateChanged"
                 << "newState" << newState;
    });

    QObject::connect(&power_shell, &QProcess::errorOccurred, [](QProcess::ProcessError error) {
        qDebug() << "QProcess::errorOccurred"
                 << "error" << error;
    });

    QObject::connect(&power_shell, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
                     [](int exitCode, QProcess::ExitStatus exitStatus) {
                         qDebug() << "QProcess::finished"
                                  << "exitCode" << exitCode << "exitStatus" << exitStatus;
                     });

    power_shell.start();
    power_shell.waitForFinished();

    return power_shell.exitStatus() == QProcess::NormalExit && power_shell.exitCode() == 0;
}
