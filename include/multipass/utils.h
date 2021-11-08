/*
 * Copyright (C) 2017-2021 Canonical, Ltd.
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

#include <multipass/logging/level.h>
#include <multipass/path.h>
#include <multipass/singleton.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/virtual_machine.h>

#include <yaml-cpp/yaml.h>

#include <chrono>
#include <functional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <QDir>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>
#include <QVariant>

#define MP_UTILS multipass::Utils::instance()

namespace multipass
{

// Fwd decl
class SSHKeyProvider;
class VirtualMachine;

namespace utils
{

// enum types
enum class QuoteType
{
    no_quotes,
    quote_every_arg
};

enum class TimeoutAction
{
    retry,
    done
};

// filesystem and path helpers
QDir base_dir(const QString& path);
multipass::Path make_dir(const QDir& a_dir, const QString& name = QString());
bool is_dir(const std::string& path);
QString backend_directory_path(const Path& path, const QString& subdirectory);
std::string filename_for(const std::string& path);
std::string contents_of(const multipass::Path& file_path);
bool invalid_target_path(const QString& target_path);
QTemporaryFile create_temp_file_with_path(const QString& filename_template);

// special-file helpers
void link_autostart_file(const QDir& link_dir, const QString& autostart_subdir, const QString& autostart_filename);
void check_and_create_config_file(const QString& config_file_path);

// command and process helpers
std::string to_cmd(const std::vector<std::string>& args, QuoteType type);
void process_throw_on_error(const QString& program, const QStringList& arguments, const QString& message,
                            const QString& category = "utils", const int timeout = 30000);
bool process_log_on_error(const QString& program, const QStringList& arguments, const QString& message,
                          const QString& category, multipass::logging::Level level = multipass::logging::Level::debug,
                          const int timeout = 30000);

// networking helpers
void validate_server_address(const std::string& value);
bool valid_hostname(const std::string& name_string);
std::string generate_mac_address();
bool valid_mac_address(const std::string& mac);

// string helpers
bool has_only_digits(const std::string& value);
std::string& trim_end(std::string& s);
std::string& trim_newline(std::string& s);
std::string escape_char(const std::string& s, char c);
std::string escape_for_shell(const std::string& s);
std::vector<std::string> split(const std::string& string, const std::string& delimiter);
std::string match_line_for(const std::string& output, const std::string& matcher);

// virtual machine helpers
bool is_running(const VirtualMachine::State& state);
void wait_until_ssh_up(VirtualMachine* virtual_machine, std::chrono::milliseconds timeout,
                       std::function<void()> const& ensure_vm_is_running = []() {});
void install_sshfs_for(const std::string& name, SSHSession& session,
                       const std::chrono::milliseconds timeout = std::chrono::minutes(5));
std::string run_in_ssh_session(SSHSession& session, const std::string& cmd);

// yaml helpers
std::string emit_yaml(const YAML::Node& node);
std::string emit_cloud_config(const YAML::Node& node);

// enum helpers
template <typename RegisteredQtEnum>
QString qenum_to_qstring(RegisteredQtEnum val);
template <typename RegisteredQtEnum>
std::string qenum_to_string(RegisteredQtEnum val);

// other helpers
QString get_driver_str();
QString make_uuid();
template <typename OnTimeoutCallable, typename TryAction, typename... Args>
void try_action_for(OnTimeoutCallable&& on_timeout, std::chrono::milliseconds timeout, TryAction&& try_action,
                    Args&&... args);

} // namespace utils

class Utils : public Singleton<Utils>
{
public:
    Utils(const Singleton<Utils>::PrivatePass&) noexcept;

    virtual qint64 filesystem_bytes_available(const QString& data_directory) const;
    virtual void exit(int code);
    virtual void make_file_with_content(const std::string& file_name, const std::string& content,
                                        const bool& overwrite = false);

    // command and process helpers
    virtual std::string run_cmd_for_output(const QString& cmd, const QStringList& args,
                                           const int timeout = 30000) const;
    virtual bool run_cmd_for_status(const QString& cmd, const QStringList& args, const int timeout = 30000) const;

    // virtual machine helpers
    virtual void wait_for_cloud_init(VirtualMachine* virtual_machine, std::chrono::milliseconds timeout,
                                     const SSHKeyProvider& key_provider) const;

    // system info helpers
    virtual std::string get_kernel_version() const;
};
} // namespace multipass

template <typename OnTimeoutCallable, typename TryAction, typename... Args>
void multipass::utils::try_action_for(OnTimeoutCallable&& on_timeout, std::chrono::milliseconds timeout,
                                      TryAction&& try_action, Args&&... args)
{

    static_assert(std::is_same<decltype(try_action(std::forward<Args>(args)...)), TimeoutAction>::value, "");
    using namespace std::literals::chrono_literals;

    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (try_action(std::forward<Args>(args)...) == TimeoutAction::done)
            return;

        // The < 1s is used for testing so unit tests don't have to sleep to 1 second
        std::this_thread::sleep_for(timeout < 1s ? timeout : 1s);
    }
    on_timeout();
}

template <typename RegisteredQtEnum>
QString multipass::utils::qenum_to_qstring(RegisteredQtEnum val)
{
    return QVariant::fromValue(val).toString();
}

template <typename RegisteredQtEnum>
std::string multipass::utils::qenum_to_string(RegisteredQtEnum val)
{
    return qenum_to_qstring(val).toStdString();
}

#endif // MULTIPASS_UTILS_H
