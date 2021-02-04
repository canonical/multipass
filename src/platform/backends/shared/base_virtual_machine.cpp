/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include "base_virtual_machine.h"

namespace multipass
{

std::vector<std::string> BaseVirtualMachine::get_all_ipv4(const SSHKeyProvider& key_provider)
{
    std::vector<std::string> all_ipv4;

    if (state == State::running)
    {
        SSHSession session{ssh_hostname(), ssh_port(), ssh_username(), key_provider};

        auto run_in_vm = [&session](const std::string& cmd) -> std::string {
            auto proc = session.exec(cmd);

            if (proc.exit_code() != 0)
            {
                auto error_msg = proc.read_std_error();
                mpl::log(mpl::Level::warning, "base vm",
                         fmt::format("failed to run '{}', error message: '{}'", cmd, mp::utils::trim_end(error_msg)));
                return std::string{};
            }

            auto output = proc.read_std_output();
            if (output.empty())
            {
                mpl::log(mpl::Level::warning, "base vm", fmt::format("no output after running '{}'", cmd));
                return std::string{};
            }

            return mp::utils::trim_end(output);
        };

        auto ip_a_output = QString::fromStdString(run_in_vm("ip -brief -family inet address show scope global"));

        QRegularExpression ipv4_re{QStringLiteral("([\\d\\.]+)\\/\\d+\\s*$"), QRegularExpression::MultilineOption};

        QRegularExpressionMatchIterator ip_it = ipv4_re.globalMatch(ip_a_output);

        while (ip_it.hasNext())
        {
            auto ip_match = ip_it.next();
            auto ip = ip_match.captured(1).toStdString();

            all_ipv4.push_back(ip);
        }
    }

    return all_ipv4;
}

} // namespace multipass
