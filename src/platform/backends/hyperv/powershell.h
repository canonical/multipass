/*
 * Copyright (C) 2017-2020 Canonical, Ltd.
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

#include <multipass/process/process.h>

#include <QStringList>

#include <memory>
#include <string>

namespace multipass
{
// fwd declarations, to befriend below
class PowerShell;
namespace test
{
bool ps_write_accessor(PowerShell&, const QByteArray&);
}

class PowerShell
{
public:
    explicit PowerShell(const std::string& name);
    ~PowerShell();

    bool run(const QStringList& args, QString& output = QString());

    static bool exec(const QStringList& args, const std::string& name, QString& output = QString());

private:
    friend bool multipass::test::ps_write_accessor(PowerShell&, const QByteArray&); // for tests
    bool write(const QByteArray& data);

    std::unique_ptr<Process> powershell_proc;
    const std::string name;
};
} // namespace multipass

#endif // MULTIPASS_POWERSHELL_H
