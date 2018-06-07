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

#include "cli.h"

#include <multipass/logging/standard_logger.h>
#include <multipass/platform.h>

#include <QCommandLineOption>
#include <QCommandLineParser>

namespace mp = multipass;
namespace mpl = multipass::logging;

mp::DaemonConfigBuilder mp::cli::parse(const QCoreApplication& app)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("multipass service daemon");
    auto help_option = parser.addHelpOption();
    auto version_option = parser.addVersionOption();

    QCommandLineOption logger_option{"logger", "specifies which logger to use", "platform|stderr"};
    parser.addOption(logger_option);

    parser.process(app);

    DaemonConfigBuilder builder;
    if (parser.isSet(logger_option))
    {
        auto logger = parser.value(logger_option);
        if (logger == "platform")
            builder.logger = platform::make_logger(logging::Level::info);
        else if (logger == "stderr")
            builder.logger = std::make_unique<mpl::StandardLogger>(mpl::Level::info);
        else
            throw std::runtime_error("Invalid logger option: " + logger.toStdString());
    }

    return builder;
}
