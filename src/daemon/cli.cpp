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
#include <multipass/utils.h>

#include <fmt/format.h>

#include <QCommandLineOption>
#include <QCommandLineParser>

#include <algorithm>
#include <cctype>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
multipass::logging::Level to_logging_level(const QString& value)
{
    if (value == "error")
        return mpl::Level::error;
    if (value == "warning")
        return mpl::Level::warning;
    if (value == "info")
        return mpl::Level::info;
    if (value == "debug")
        return mpl::Level::debug;

    throw std::runtime_error("invalid logging verbosity: " + value.toStdString());
}
} // namespace

mp::DaemonConfigBuilder mp::cli::parse(const QCoreApplication& app)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("multipass service daemon");
    auto help_option = parser.addHelpOption();
    auto version_option = parser.addVersionOption();

    QCommandLineOption logger_option{"logger", "specifies which logger to use", "platform|stderr"};
    QCommandLineOption verbosity_option{
        {"V", "verbosity"}, "specifies the logging verbosity level", "error|warning|info|debug"};
    QCommandLineOption address_option{"address",
                                      "specifies which address to use for the multipassd service;"
                                      " a socket can be specified using unix:<socket_file>",
                                      "server_name:port"};

    parser.addOption(logger_option);
    parser.addOption(verbosity_option);
    parser.addOption(address_option);

    parser.process(app);

    DaemonConfigBuilder builder;

    if (parser.isSet(verbosity_option))
        builder.verbosity_level = to_logging_level(parser.value(verbosity_option));

    if (parser.isSet(logger_option))
    {
        auto logger = parser.value(logger_option);
        if (logger == "platform")
            builder.logger = platform::make_logger(builder.verbosity_level);
        else if (logger == "stderr")
            builder.logger = std::make_unique<mpl::StandardLogger>(builder.verbosity_level);
        else
            throw std::runtime_error(fmt::format("invalid logger option '{}'", logger.toStdString()));
    }

    if (parser.isSet(address_option))
    {
        auto address = parser.value(address_option).toStdString();
        if (address.empty())
            throw std::runtime_error("empty server address");

        auto tokens = mp::utils::split(address, ":");
        const auto server_name = tokens[0];
        if (tokens.size() == 1u)
        {
            if (server_name == "unix")
                throw std::runtime_error(fmt::format("missing socket file in address '{}'", address));
            else
                throw std::runtime_error(fmt::format("missing port number in address '{}'", address));
        }

        const auto port = tokens[1];
        if (server_name != "unix" && !mp::utils::has_only_digits(port))
            throw std::runtime_error(fmt::format("invalid port number in address '{}'", address));

        builder.server_address = address;
    }

    return builder;
}
