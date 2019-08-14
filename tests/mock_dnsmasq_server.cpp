/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
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

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>

#include <sys/types.h>
#include <unistd.h>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    QCommandLineOption pidOption("pid-file", "Path for the pid file", "path");

    parser.addOption(pidOption);

    parser.parse(QCoreApplication::arguments());

    if (parser.isSet(pidOption))
    {
        QFile pid_file{parser.value(pidOption)};
        pid_file.open(QIODevice::WriteOnly);
        pid_file.write(QString::number(getpid()).toUtf8());
    }

    return 0;
}
