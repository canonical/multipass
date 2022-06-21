/*
 * Copyright (C) 2017-2022 Canonical, Ltd.
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

#include <multipass/constants.h>
#include <multipass/exceptions/autostart_setup_exception.h>
#include <multipass/exceptions/exitless_sshprocess_exception.h>
#include <multipass/exceptions/sshfs_missing_error.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/ssh_session.h>
#include <multipass/standard_paths.h>
#include <multipass/utils.h>

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QSysInfo>
#include <QUuid>
#include <QtGlobal>

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <fstream>
#include <random>
#include <regex>
#include <sstream>

#include <openssl/evp.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

using namespace std::chrono_literals;

namespace
{
constexpr auto category = "utils";
constexpr auto scrypt_hash_size{64};

auto quote_for(const std::string& arg, mp::utils::QuoteType quote_type)
{
    if (quote_type == mp::utils::QuoteType::no_quotes)
        return "";
    return arg.find('\'') == std::string::npos ? "'" : "\"";
}

QString find_autostart_target(const QString& subdir, const QString& autostart_filename)
{
    const auto target_subpath = QDir{subdir}.filePath(autostart_filename);
    const auto target_path = MP_STDPATHS.locate(mp::StandardPaths::GenericDataLocation, target_subpath);

    if (target_path.isEmpty())
    {
        QString detail{};
        for (const auto& path : MP_STDPATHS.standardLocations(mp::StandardPaths::GenericDataLocation))
            detail += QStringLiteral("\n  ") + path + "/" + target_subpath;

        throw mp::AutostartSetupException{fmt::format("could not locate the autostart file '{}'", autostart_filename),
                                          fmt::format("Tried: {}", detail.toStdString())};
    }

    return target_path;
}
} // namespace

mp::Utils::Utils(const Singleton<Utils>::PrivatePass& pass) noexcept : Singleton<Utils>::Singleton{pass}
{
}

qint64 mp::Utils::filesystem_bytes_available(const QString& data_directory) const
{
    return QStorageInfo(QDir(data_directory)).bytesAvailable();
}

void mp::Utils::exit(int code)
{
    std::exit(code);
}

std::string mp::Utils::run_cmd_for_output(const QString& cmd, const QStringList& args, const int timeout) const
{
    QProcess proc;
    proc.setProgram(cmd);
    proc.setArguments(args);

    proc.start();
    proc.waitForFinished(timeout);

    return proc.readAllStandardOutput().trimmed().toStdString();
}

bool mp::Utils::run_cmd_for_status(const QString& cmd, const QStringList& args, const int timeout) const
{
    QProcess proc;
    proc.setProgram(cmd);
    proc.setArguments(args);

    proc.start();
    proc.waitForFinished(timeout);

    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

void mp::Utils::make_file_with_content(const std::string& file_name, const std::string& content, const bool& overwrite)
{
    QFile file(QString::fromStdString(file_name));
    if (!overwrite && MP_FILEOPS.exists(file))
        throw std::runtime_error(fmt::format("file '{}' already exists", file_name));

    QDir parent_dir{QFileInfo{file}.absoluteDir()};
    if (!MP_FILEOPS.mkpath(parent_dir, "."))
        throw std::runtime_error(fmt::format("failed to create dir '{}'", parent_dir.path()));

    if (!MP_FILEOPS.open(file, QFile::WriteOnly))
        throw std::runtime_error(fmt::format("failed to open file '{}' for writing", file_name));

    if (MP_FILEOPS.write(file, content.c_str(), content.size()) != (qint64)content.size())
        throw std::runtime_error(fmt::format("failed to write to file '{}'", file_name));

    return;
}

void mp::Utils::wait_for_cloud_init(mp::VirtualMachine* virtual_machine, std::chrono::milliseconds timeout,
                                    const mp::SSHKeyProvider& key_provider) const
{
    auto action = [virtual_machine, &key_provider] {
        virtual_machine->ensure_vm_is_running();
        try
        {
            mp::SSHSession session{virtual_machine->ssh_hostname(), virtual_machine->ssh_port(),
                                   virtual_machine->ssh_username(), key_provider};

            std::lock_guard<decltype(virtual_machine->state_mutex)> lock{virtual_machine->state_mutex};
            auto ssh_process = session.exec({"[ -e /var/lib/cloud/instance/boot-finished ]"});
            return ssh_process.exit_code() == 0 ? mp::utils::TimeoutAction::done : mp::utils::TimeoutAction::retry;
        }
        catch (const std::exception& e)
        {
            std::lock_guard<decltype(virtual_machine->state_mutex)> lock{virtual_machine->state_mutex};
            mpl::log(mpl::Level::warning, virtual_machine->vm_name, e.what());
            return mp::utils::TimeoutAction::retry;
        }
    };
    auto on_timeout = [] { throw std::runtime_error("timed out waiting for initialization to complete"); };
    mp::utils::try_action_for(on_timeout, timeout, action);
}

std::string mp::Utils::get_kernel_version() const
{
    return QSysInfo::kernelVersion().toStdString();
}

QString mp::Utils::generate_scrypt_hash_for(const QString& passphrase) const
{
    QByteArray hash(scrypt_hash_size, '\0');

    if (!EVP_PBE_scrypt(passphrase.toStdString().c_str(), passphrase.size(), nullptr, 0, 1 << 14, 8, 1, 0,
                        reinterpret_cast<unsigned char*>(hash.data()), scrypt_hash_size))
        throw std::runtime_error("Cannot generate passphrase hash");

    return QString(hash.toHex());
}

QDir mp::utils::base_dir(const QString& path)
{
    QFileInfo info{path};
    return info.absoluteDir();
}

bool mp::utils::valid_hostname(const std::string& name_string)
{
    QRegExp matcher("^([a-zA-Z]|[a-zA-Z][a-zA-Z0-9\\-]*[a-zA-Z0-9])");

    return matcher.exactMatch(QString::fromStdString(name_string));
}

bool mp::utils::invalid_target_path(const QString& target_path)
{
    QString sanitized_path{QDir::cleanPath(target_path)};
    QRegExp matcher("/+|/+(dev|proc|sys)(/.*)*|/+home(/*)(/ubuntu/*)*");

    return matcher.exactMatch(sanitized_path);
}

QTemporaryFile mp::utils::create_temp_file_with_path(const QString& filename_template)
{
    auto temp_folder = QFileInfo(filename_template).absoluteDir();

    if (!MP_FILEOPS.mkpath(temp_folder, temp_folder.absolutePath()))
    {
        throw std::runtime_error(fmt::format("Could not create path '{}'", temp_folder.absolutePath()));
    }

    return QTemporaryFile(filename_template);
}

std::string mp::utils::to_cmd(const std::vector<std::string>& args, QuoteType quote_type)
{
    if (args.empty())
        return "";

    fmt::memory_buffer buf;
    for (auto const& arg : args)
    {
        fmt::format_to(buf, "{0}{1}{0} ", quote_for(arg, quote_type), arg);
    }

    // Remove the last space inserted
    auto cmd = fmt::to_string(buf);
    cmd.pop_back();
    return cmd;
}

std::string& mp::utils::trim_end(std::string& s)
{
    auto rev_it = std::find_if(s.rbegin(), s.rend(), [](char ch) { return !std::isspace(ch); });
    s.erase(rev_it.base(), s.end());
    return s;
}

std::string& mp::utils::trim_newline(std::string& s)
{
    assert(!s.empty() && '\n' == s.back());
    s.pop_back();
    return s;
}

std::string mp::utils::escape_char(const std::string& in, char c)
{
    return std::regex_replace(in, std::regex({c}), fmt::format("\\{}", c));
}

// Escape all characters which need to be escaped in the shell.
std::string mp::utils::escape_for_shell(const std::string& in)
{
    std::string ret;
    std::back_insert_iterator<std::string> ret_insert = std::back_inserter(ret);

    for (char c : in)
    {
        // If the character is in one of these code ranges, then it must be escaped.
        if (c < 0x25 || c > 0x7a || (c > 0x25 && c < 0x2b) || (c > 0x5a && c < 0x5f) || 0x2c == c || 0x3b == c ||
            0x3c == c || 0x3e == c || 0x3f == c || 0x60 == c)
        {
            *ret_insert++ = '\\';
        }
        *ret_insert++ = c;
    }

    return ret;
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

bool mp::utils::valid_mac_address(const std::string& mac)
{
    // A MAC address is a string consisting of six pairs of colon-separated hexadecimal digits.
    const auto pattern = QStringLiteral("^([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}$");
    const auto regexp = QRegularExpression{pattern};
    const auto match = regexp.match(QString::fromStdString(mac));

    return match.hasMatch();
}

void mp::utils::wait_until_ssh_up(VirtualMachine* virtual_machine, std::chrono::milliseconds timeout,
                                  std::function<void()> const& ensure_vm_is_running)
{
    mpl::log(mpl::Level::debug, virtual_machine->vm_name, "Waiting for SSH to be up");
    auto action = [virtual_machine, &ensure_vm_is_running] {
        ensure_vm_is_running();
        try
        {
            mp::SSHSession session{virtual_machine->ssh_hostname(1ms), virtual_machine->ssh_port()};

            std::lock_guard<decltype(virtual_machine->state_mutex)> lock{virtual_machine->state_mutex};
            virtual_machine->state = VirtualMachine::State::running;
            virtual_machine->update_state();
            return mp::utils::TimeoutAction::done;
        }
        catch (const std::exception&)
        {
            return mp::utils::TimeoutAction::retry;
        }
    };
    auto on_timeout = [virtual_machine] {
        std::lock_guard<decltype(virtual_machine->state_mutex)> lock{virtual_machine->state_mutex};
        virtual_machine->state = VirtualMachine::State::unknown;
        virtual_machine->update_state();
        throw std::runtime_error(fmt::format("{}: timed out waiting for response", virtual_machine->vm_name));
    };

    mp::utils::try_action_for(on_timeout, timeout, action);
}

void mp::utils::install_sshfs_for(const std::string& name, mp::SSHSession& session,
                                  const std::chrono::milliseconds timeout)
{
    mpl::log(mpl::Level::info, category, fmt::format("Installing the multipass-sshfs snap in \'{}\'", name));

    // Check if snap support is installed in the instance
    auto which_proc = session.exec("which snap");
    if (which_proc.exit_code() != 0)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Snap support is not installed in \'{}\'", name));
        throw std::runtime_error(
            fmt::format("Snap support needs to be installed in \'{}\' in order to support mounts.\n"
                        "Please see https://docs.snapcraft.io/installing-snapd for information on\n"
                        "how to install snap support for your instance's distribution.\n\n"
                        "If your distribution's instructions specify enabling classic snap support,\n"
                        "please do that as well.\n\n"
                        "Alternatively, install `sshfs` manually inside the instance.",
                        name));
    }

    // Check if /snap exists for "classic" snap support
    auto test_file_proc = session.exec("[ -e /snap ]");
    if (test_file_proc.exit_code() != 0)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Classic snap support symlink is needed in \'{}\'", name));
        throw std::runtime_error(
            fmt::format("Classic snap support is not enabled for \'{}\'!\n\n"
                        "Please see https://docs.snapcraft.io/installing-snapd for information on\n"
                        "how to enable classic snap support for your instance's distribution.",
                        name));
    }

    try
    {
        auto proc = session.exec("sudo snap install multipass-sshfs");
        if (proc.exit_code(timeout) != 0)
        {
            auto error_msg = proc.read_std_error();
            mpl::log(mpl::Level::warning, category,
                     fmt::format("Failed to install \'multipass-sshfs\', error message: \'{}\'",
                                 mp::utils::trim_end(error_msg)));
            throw mp::SSHFSMissingError();
        }
    }
    catch (const mp::ExitlessSSHProcessException&)
    {
        mpl::log(mpl::Level::info, category, fmt::format("Timeout while installing 'sshfs' in '{}'", name));
    }
}

// Executes a given command on the given session. Returns the output of the command, with spaces and feeds trimmed.
// Caveat emptor: if the command fails, an empty string is returned.
std::string mp::utils::run_in_ssh_session(mp::SSHSession& session, const std::string& cmd)
{
    mpl::log(mpl::Level::debug, category, fmt::format("executing '{}'", cmd));
    auto proc = session.exec(cmd);

    if (proc.exit_code() != 0)
    {
        auto error_msg = proc.read_std_error();
        mpl::log(mpl::Level::warning, category,
                 fmt::format("failed to run '{}', error message: '{}'", cmd, mp::utils::trim_end(error_msg)));
        return std::string{};
    }

    auto output = proc.read_std_output();
    if (output.empty())
    {
        mpl::log(mpl::Level::warning, category, fmt::format("no output after running '{}'", cmd));
        return std::string{};
    }

    return mp::utils::trim_end(output);
}

void mp::utils::link_autostart_file(const QDir& link_dir, const QString& autostart_subdir,
                                    const QString& autostart_filename)
{
    const auto link_path = link_dir.absoluteFilePath(autostart_filename);
    const auto target_path = find_autostart_target(autostart_subdir, autostart_filename);

    const auto link_info = QFileInfo{link_path};
    const auto target_info = QFileInfo{target_path};
    auto target_file = QFile{target_path};
    auto link_file = QFile{link_path};

    if (link_info.isSymLink() && link_info.symLinkTarget() != target_info.absoluteFilePath())
        link_file.remove(); // get rid of outdated and broken links

    if (!link_file.exists())
    {
        link_dir.mkpath(".");
        if (!target_file.link(link_path))

            throw mp::AutostartSetupException{fmt::format("failed to link file '{}' to '{}'", link_path, target_path),
                                              fmt::format("Detail: {} (error code {})", strerror(errno), errno)};
    }
}

mp::Path mp::utils::make_dir(const QDir& a_dir, const QString& name, const QFileDevice::Permissions permissions)
{
    mp::Path dir_path;
    bool success{false};

    if (name.isEmpty())
    {
        success = a_dir.mkpath(".");
        dir_path = a_dir.absolutePath();
    }
    else
    {
        success = a_dir.mkpath(name);
        dir_path = a_dir.filePath(name);
    }

    if (!success)
    {
        throw std::runtime_error(fmt::format("unable to create directory '{}'", dir_path));
    }

    if (permissions)
    {
        QFile::setPermissions(dir_path, permissions);
    }

    return dir_path;
}

mp::Path mp::utils::make_dir(const QDir& dir, const QFileDevice::Permissions permissions)
{
    return make_dir(dir, QString(), permissions);
}

void mp::utils::remove_directories(const std::vector<QString>& dirs)
{
    for (const auto& dir : dirs)
    {
        QDir(dir).removeRecursively();
    }
}

QString mp::utils::backend_directory_path(const mp::Path& path, const QString& subdirectory)
{
    if (subdirectory.isEmpty())
        return path;

    return mp::Path("%1/%2").arg(path).arg(subdirectory);
}

QString mp::utils::get_multipass_storage()
{
    return QString::fromUtf8(qgetenv(mp::multipass_storage_env_var));
}

QString mp::utils::make_uuid()
{
    auto uuid = QUuid::createUuid().toString();

    // Remove curly brackets enclosing uuid
    return uuid.mid(1, uuid.size() - 2);
}

std::string mp::utils::contents_of(const multipass::Path& file_path)
{
    const std::string name{file_path.toStdString()};
    std::ifstream in(name, std::ios::in | std::ios::binary);
    if (!in)
        throw std::runtime_error(fmt::format("failed to open file '{}': {}({})", name, strerror(errno), errno));

    std::stringstream stream;
    stream << in.rdbuf();
    return stream.str();
}

bool mp::utils::has_only_digits(const std::string& value)
{
    return std::all_of(value.begin(), value.end(), [](char c) { return std::isdigit(c); });
}

void mp::utils::validate_server_address(const std::string& address)
{
    if (address.empty())
        throw std::runtime_error("empty server address");

    const auto tokens = mp::utils::split(address, ":");
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
}

std::string mp::utils::filename_for(const std::string& path)
{
    return QFileInfo(QString::fromStdString(path)).fileName().toStdString();
}

bool mp::utils::is_dir(const std::string& path)
{
    return QFileInfo(QString::fromStdString(path)).isDir();
}

std::string mp::utils::match_line_for(const std::string& output, const std::string& matcher)
{
    std::istringstream ss{output};
    std::string line;

    while (std::getline(ss, line, '\n'))
    {
        if (line.find(matcher) != std::string::npos)
        {
            return line;
        }
    }

    return std::string{};
}

bool mp::utils::is_running(const VirtualMachine::State& state)
{
    return state == VirtualMachine::State::running || state == VirtualMachine::State::delayed_shutdown;
}

void mp::utils::check_and_create_config_file(const QString& config_file_path)
{
    QFile config_file{config_file_path};

    if (!config_file.exists())
    {
        make_dir({}, QFileInfo{config_file_path}.dir().path()); // make sure parent dir is there
        config_file.open(QIODevice::WriteOnly);
    }
}

void mp::utils::process_throw_on_error(const QString& program, const QStringList& arguments, const QString& message,
                                       const QString& category, const int timeout)
{
    QProcess process;
    mpl::log(mpl::Level::debug, category.toStdString(),
             fmt::format("Running: {}, {}", program.toStdString(), arguments.join(", ").toStdString()));
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    auto success = process.waitForFinished(timeout);

    if (!success || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        mpl::log(mpl::Level::debug, category.toStdString(),
                 fmt::format("{} failed - errorString: {}, exitStatus: {}, exitCode: {}", program.toStdString(),
                             process.errorString().toStdString(), process.exitStatus(), process.exitCode()));

        auto output = process.readAllStandardOutput();
        throw std::runtime_error(fmt::format(
            message.toStdString(), output.isEmpty() ? process.errorString().toStdString() : output.toStdString()));
    }
}

bool mp::utils::process_log_on_error(const QString& program, const QStringList& arguments, const QString& message,
                                     const QString& category, mpl::Level level, const int timeout)
{
    QProcess process;
    mpl::log(mpl::Level::debug, category.toStdString(),
             fmt::format("Running: {}, {}", program.toStdString(), arguments.join(", ").toStdString()));
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    auto success = process.waitForFinished(timeout);

    if (!success || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        mpl::log(mpl::Level::debug, category.toStdString(),
                 fmt::format("{} failed - errorString: {}, exitStatus: {}, exitCode: {}", program.toStdString(),
                             process.errorString().toStdString(), process.exitStatus(), process.exitCode()));

        auto output = process.readAllStandardOutput();
        mpl::log(level, category.toStdString(),
                 fmt::format(message.toStdString(),
                             output.isEmpty() ? process.errorString().toStdString() : output.toStdString()));
        return false;
    }

    return true;
}

std::string mp::utils::emit_yaml(const YAML::Node& node)
{
    YAML::Emitter emitter;
    emitter.SetIndent(2);
    emitter << node;
    if (!emitter.good())
        throw std::runtime_error{fmt::format("Failed to emit YAML: {}", emitter.GetLastError())};

    emitter << YAML::Newline;
    return emitter.c_str();
}

std::string mp::utils::emit_cloud_config(const YAML::Node& node)
{
    return fmt::format("#cloud-config\n{}\n", emit_yaml(node));
}
