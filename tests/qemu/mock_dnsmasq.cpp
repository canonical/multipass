/*
 * Copyright (C) 2018-2020 Canonical, Ltd.
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

#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <sys/prctl.h>
#include <unistd.h>

const auto unexpected_error = 5;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    QCommandLineOption listenOption("listen-address", "Address to listen on", "address");
    parser.addOption(listenOption);

    parser.parse(QCoreApplication::arguments());

    if (parser.isSet(listenOption))
    {
        auto address = parser.value(listenOption);
        // For testing, we treat a 0.0.0 subnet as an error
        if (address.contains("0.0.0"))
        {
            return 1;
        }
    }

    int pipefd[2];
    if (pipe(pipefd))
    {
        std::cerr << "Failed to create pipe: " << std::strerror(errno) << std::endl;
        return unexpected_error;
    }

    char exit_code;

    auto pid = fork();
    if (pid == 0)
    {
        close(pipefd[0]);

        if (prctl(PR_SET_PDEATHSIG, SIGHUP) != 0)
        {
            std::cerr << "Failed to set the parent-death signal: " << std::strerror(errno) << std::endl;
            return unexpected_error;
        }

        if (write(pipefd[1], "0", 1) < 1)
        {
            std::cerr << "Failed to write to pipe: " << std::strerror(errno) << std::endl;
            return unexpected_error;
        }

        close(pipefd[1]);

        return QCoreApplication::exec();
    }
    else if (pid > 0)
    {
        close(pipefd[1]);

        if (read(pipefd[0], &exit_code, 1) < 1)
        {
            std::cerr << "Failed to read from pipe: " << std::strerror(errno) << std::endl;
            return unexpected_error;
        }

        close(pipefd[0]);

        return atoi(&exit_code);
    }
}
