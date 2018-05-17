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

#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>

#include <fmt/format.h>

#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include <array>
#include <random>
#include <regex>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
auto quote_for(const std::string& arg, mp::utils::QuoteType quote_type)
{
    if (quote_type == mp::utils::QuoteType::no_quotes)
        return "";
    return arg.find('\'') == std::string::npos ? "'" : "\"";
}
}

QDir mp::utils::base_dir(const QString& path)
{
    QFileInfo info{path};
    return info.absoluteDir();
}

bool mp::utils::valid_memory_value(const QString& mem_string)
{
    QRegExp matcher("\\d+((K|M|G)(B){0,1}){0,1}$");

    return matcher.exactMatch(mem_string);
}

bool mp::utils::valid_hostname(const QString& name_string)
{
    QRegExp matcher("^([a-zA-Z]|[a-zA-Z][a-zA-Z0-9\\-]*[a-zA-Z0-9])");

    return matcher.exactMatch(name_string);
}

bool mp::utils::invalid_target_path(const QString& target_path)
{
    QString sanitized_path{QDir::cleanPath(target_path)};
    QRegExp matcher("/+|/+(dev|proc|sys)(/.*)*|/+home(/*)(/ubuntu/*)*");

    return matcher.exactMatch(sanitized_path);
}

std::string mp::utils::to_cmd(const std::vector<std::string>& args, QuoteType quote_type)
{
    fmt::MemoryWriter out;
    for (auto const& arg : args)
    {
        out.write("{0}{1}{0} ", quote_for(arg, quote_type), arg);
    }

    // Remove the last space inserted
    auto cmd = out.str();
    cmd.pop_back();
    return cmd;
}

bool mp::utils::run_cmd_for_status(const QString& cmd, const QStringList& args)
{
    QProcess proc;
    proc.setProgram(cmd);
    proc.setArguments(args);

    proc.start();
    proc.waitForFinished();

    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

std::string mp::utils::run_cmd_for_output(const QString& cmd, const QStringList& args)
{
    QProcess proc;
    proc.setProgram(cmd);
    proc.setArguments(args);

    proc.start();
    proc.waitForFinished();

    return proc.readAllStandardOutput().trimmed().toStdString();
}

std::string& mp::utils::trim_end(std::string& s)
{
    auto rev_it = std::find_if(s.rbegin(), s.rend(), [](char ch) { return !std::isspace(ch); });
    s.erase(rev_it.base(), s.end());
    return s;
}

std::string mp::utils::escape_char(const std::string& in, char c)
{
    return std::regex_replace(in, std::regex({c}), fmt::format("\\{}", c));
}

std::vector<std::string> mp::utils::split(const std::string& string, const std::string& delimiter)
{
    std::regex regex(delimiter);
    return {std::sregex_token_iterator{string.begin(), string.end(), regex, -1}, std::sregex_token_iterator{}};
}

std::string mp::utils::generate_mac_address()
{
    std::default_random_engine gen;
    std::uniform_int_distribution<int> dist{0, 255};

    gen.seed(std::chrono::system_clock::now().time_since_epoch().count());
    std::array<int, 3> octets{{dist(gen), dist(gen), dist(gen)}};
    return fmt::format("52:54:00:{:02x}:{:02x}:{:02x}", octets[0], octets[1], octets[2]);
}

void mp::utils::wait_until_ssh_up(VirtualMachine* virtual_machine, std::chrono::milliseconds timeout,
                                  std::function<void()> const& process_vm_events)
{
    auto action = [virtual_machine, &process_vm_events] {
        process_vm_events();
        try
        {
            mp::SSHSession session{virtual_machine->ssh_hostname(), virtual_machine->ssh_port()};
            virtual_machine->state = VirtualMachine::State::running;
            return mp::utils::TimeoutAction::done;
        }
        catch (const std::exception&)
        {
            virtual_machine->state = VirtualMachine::State::unknown;
            return mp::utils::TimeoutAction::retry;
        }
    };
    auto on_timeout = [] { return std::runtime_error("timed out waiting for ssh service to start"); };
    mp::utils::try_action_for(on_timeout, timeout, action);
}

void mp::utils::wait_for_cloud_init(mp::VirtualMachine* virtual_machine, std::chrono::milliseconds timeout,
                                    std::function<void()> const& process_vm_events)
{
    auto action = [virtual_machine, &process_vm_events] {
        process_vm_events();
        try
        {
            mp::SSHSession session{virtual_machine->ssh_hostname(), virtual_machine->ssh_port(),
                                   virtual_machine->ssh_username(), virtual_machine->key_provider};
            auto ssh_process = session.exec({"[ -e /var/lib/cloud/instance/boot-finished ]"});
            return ssh_process.exit_code() == 0 ? mp::utils::TimeoutAction::done : mp::utils::TimeoutAction::retry;
        }
        catch (const std::exception& e)
        {
            mpl::log(mpl::Level::warning, virtual_machine->vm_name, e.what());
            return mp::utils::TimeoutAction::retry;
        }
    };
    auto on_timeout = [] { return std::runtime_error("timed out waiting for cloud-init to complete"); };
    mp::utils::try_action_for(on_timeout, timeout, action);
}
