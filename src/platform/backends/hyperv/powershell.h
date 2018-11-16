/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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

#ifndef MULTIPASS_POWERSHELL_H
#define MULTIPASS_POWERSHELL_H

#include <string>

#include <QProcess>
#include <QStringList>

namespace multipass
{
class PowerShell
{
public:
    explicit PowerShell(const std::string& name);
    ~PowerShell();

    bool run(const QStringList& args, std::string& output = std::string());

    static bool run_once(const QStringList& args, const std::string& name, std::string& output = std::string());

private:
    QProcess powershell_proc;
    const std::string name;
};
} // namespace multipass

#endif // MULTIPASS_POWERSHELL_H
