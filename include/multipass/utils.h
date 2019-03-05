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

#ifndef MULTIPASS_UTILS_H
#define MULTIPASS_UTILS_H

#include <multipass/path.h>
#include <multipass/virtual_machine.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include <QDir>
#include <QString>
#include <QStringList>

namespace multipass
{
class VirtualMachine;

namespace utils
{
enum class QuoteType
{
    no_quotes,
    quote_every_arg
};

QDir base_dir(const QString& path);
multipass::Path make_dir(const QDir& a_dir, const QString& name);
bool is_dir(const std::string& path);
std::string filename_for(const std::string& path);
QString make_uuid();
std::string contents_of(const multipass::Path& file_path);
bool has_only_digits(const std::string& value);
void validate_server_address(const std::string& value);
bool valid_memory_value(const QString& mem_string);
bool valid_hostname(const QString& name_string);
bool invalid_target_path(const QString& target_path);
std::string to_cmd(const std::vector<std::string>& args, QuoteType type);
bool run_cmd_for_status(const QString& cmd, const QStringList& args, const int timeout=30000);
std::string run_cmd_for_output(const QString& cmd, const QStringList& args, const int timeout=30000);
std::string& trim_end(std::string& s);
std::string escape_char(const std::string& s, char c);
std::vector<std::string> split(const std::string& string, const std::string& delimiter);
std::string generate_mac_address();
std::string timestamp();
bool is_running(const VirtualMachine::State& state);
void wait_until_ssh_up(VirtualMachine* virtual_machine, std::chrono::milliseconds timeout,
                       std::function<void()> const& process_vm_events = []() { });

enum class TimeoutAction
{
    retry,
    done
};
template <typename OnTimeoutCallable, typename TryAction, typename... Args>
void try_action_for(OnTimeoutCallable&& on_timeout, std::chrono::milliseconds timeout, TryAction&& try_action,
                    Args&&... args)
{

    static_assert(std::is_same<decltype(try_action(std::forward<Args>(args)...)), TimeoutAction>::value, "");
    using namespace std::literals::chrono_literals;

    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (try_action(std::forward<Args>(args)...) == TimeoutAction::done)
            return;

        if ((std::chrono::steady_clock::now() + 1s) >= deadline)
            break;

        std::this_thread::sleep_for(1s);
    }
    on_timeout();
}
}
}
#endif // MULTIPASS_UTILS_H
