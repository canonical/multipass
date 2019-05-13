/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include <multipass/cli/client_platform.h>

#include <QProcess>

namespace mp = multipass;
namespace mcp = multipass::cli::platform;

void mcp::open_multipass_shell(const QString& instance_name)
{
    QProcess::startDetached(
        "xterm", {"-title", instance_name, "-j", "-e", QString("multipass shell %1 || read").arg(instance_name)});
}
