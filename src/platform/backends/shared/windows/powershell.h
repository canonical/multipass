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

#ifndef MULTIPASS_POWERSHELL_H
#define MULTIPASS_POWERSHELL_H

#include <multipass/process/process.h>

#include <QStringList>

#include <memory>
#include <string>

namespace multipass
{
namespace test
{
class PowerShellTestHelper; // fwd declaration, to befriend below
}

class PowerShell
{
public:
    explicit PowerShell(const std::string& name);
    ~PowerShell();

    // require rvalue ref for the error msg, to avoid confusion with output parameter in run
    void checked_run(const QStringList& args, std::string&& error_msg); // signals failures with exception
    bool run(const QStringList& args, QString* output = nullptr, bool whisper = false);

    static bool exec(const QStringList& args, const std::string& name, QString* output = nullptr);

    struct Snippets
    {
        static const QStringList expand_property;
        static const QStringList to_bare_csv;
    };

private:
    friend class multipass::test::PowerShellTestHelper;
    bool write(const QByteArray& data);

    std::unique_ptr<Process> powershell_proc;
    const std::string name;

    inline static const QString output_end_marker = "cmdlet status is";
};
} // namespace multipass

#endif // MULTIPASS_POWERSHELL_H
