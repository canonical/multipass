/*
 * Copyright (C) Canonical, Ltd.
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

#include <unistd.h>

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

    pause(); // wait to be terminated from the outside
    return 0;
}
