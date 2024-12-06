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

#include <multipass/constants.h>
#include <multipass/exceptions/autostart_setup_exception.h>
#include <multipass/exceptions/file_open_failed_exception.h>
#include <multipass/exceptions/ssh_exception.h>
#include <multipass/file_ops.h>
#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/network_interface_info.h>
#include <multipass/platform.h>
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
#include <optional>
#include <random>
#include <regex>
#include <sstream>

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "utils";
constexpr auto scrypt_hash_size{64};
}
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
        throw std::runtime_error(
            fmt::format("failed to open file '{}' for writing: {}", file_name, file.errorString()));

    // TODO use a QTextStream instead. Theoretically, this may fail to write it all in one go but still succeed.
    // In practice, that seems unlikely. See https://stackoverflow.com/a/70933650 for more.
    if (MP_FILEOPS.write(file, content.c_str(), content.size()) != (qint64)content.size())
        throw std::runtime_error(fmt::format("failed to write to file '{}': {}", file_name, file.errorString()));

    if (!MP_FILEOPS.flush(file)) // flush manually to check return (which QFile::close ignores)
        throw std::runtime_error(fmt::format("failed to flush file '{}': {}", file_name, file.errorString()));

    return; // file closed, flush called again with errors ignored
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
    QRegularExpression matcher{QRegularExpression::anchoredPattern("^([a-zA-Z]|[a-zA-Z][a-zA-Z0-9\\-]*[a-zA-Z0-9])")};

    return matcher.match(QString::fromStdString(name_string)).hasMatch();
}

bool mp::utils::invalid_target_path(const QString& target_path)
{
    QString sanitized_path{QDir::cleanPath(target_path)};
    QRegularExpression matcher{QRegularExpression::anchoredPattern("/+|/+(dev|proc|sys)(/.*)*|/+home(/*)(/ubuntu/*)*")};

    return matcher.match(sanitized_path).hasMatch();
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
        fmt::format_to(std::back_inserter(buf), "{0} ",
                       quote_type == QuoteType::no_quotes ? arg : escape_for_shell(arg));
    }

    // Remove the last space inserted
    auto cmd = fmt::to_string(buf);
    cmd.pop_back();
    return cmd;
}

std::string& mp::utils::trim_newline(std::string& s)
{
    assert(!s.empty() && '\n' == s.back());
    s.pop_back();
    return s;
}

// Escape all characters which need to be escaped in the shell.
std::string mp::utils::escape_for_shell(const std::string& in)
{
    // If the input string is empty, it means that the shell received an empty string enclosed in quotes and removed
    // them. It must be quoted again for the shell to recognize it.
    if (in.empty())
    {
        return "\'\'";
    }

    std::string ret;

    std::back_insert_iterator<std::string> ret_insert = std::back_inserter(ret);

    for (char c : in)
    {
        if ('\n' == c) // newline
        {
            *ret_insert++ = '"';  // double quotes
            *ret_insert++ = '\n'; // newline
            *ret_insert++ = '"';  // double quotes
        }
        else
        {
            // If the character is in one of these code ranges, then it must be escaped.
            if (c < 0x25 || c > 0x7a || (c > 0x25 && c < 0x2b) || (c > 0x5a && c < 0x5f) || 0x2c == c || 0x3b == c ||
                0x3c == c || 0x3e == c || 0x3f == c || 0x60 == c)
            {
                *ret_insert++ = '\\'; // backslash
            }

            *ret_insert++ = c;
        }
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

// Executes a given command on the given session. Returns the output of the command, with spaces and feeds trimmed.
std::string mp::Utils::run_in_ssh_session(mp::SSHSession& session, const std::string& cmd, bool whisper) const
{
    auto proc = session.exec(cmd, whisper);

    if (auto ec = proc.exit_code() != 0)
    {
        auto error_msg = mp::utils::trim_end(proc.read_std_error());
        mpl::log(mpl::Level::debug, category, fmt::format("failed to run '{}', error message: '{}'", cmd, error_msg));
        throw mp::SSHExecFailure(error_msg, ec);
    }

    auto output = proc.read_std_output();
    return mp::utils::trim_end(output);
}

mp::Path mp::Utils::make_dir(const QDir& a_dir, const QString& name, QFileDevice::Permissions permissions)
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
        MP_PLATFORM.set_permissions(dir_path, permissions);
    }

    return dir_path;
}

mp::Path mp::Utils::make_dir(const QDir& dir, QFileDevice::Permissions permissions)
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

QString mp::utils::make_uuid(const std::optional<std::string>& seed)
{
    auto uuid = seed ? QUuid::createUuidV3(QUuid{}, QString::fromStdString(*seed)) : QUuid::createUuid();
    return uuid.toString(QUuid::WithoutBraces);
}

std::string mp::utils::contents_of(const multipass::Path& file_path) // TODO this should protect against long contents
{
    const std::string name{file_path.toStdString()};
    std::ifstream in(name, std::ios::in | std::ios::binary);
    if (!in)
        throw FileOpenFailedException(name);

    std::stringstream stream;
    stream << in.rdbuf();
    return stream.str();
}

std::string mp::Utils::contents_of(const multipass::Path& file_path) const
{
    return mp::utils::contents_of(file_path);
}

std::vector<uint8_t> mp::Utils::random_bytes(size_t len)
{
    std::vector<uint8_t> bytes(len, 0);

#ifdef MULTIPASS_COMPILER_GCC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

    RAND_bytes(bytes.data(), bytes.size());

#ifdef MULTIPASS_COMPILER_GCC
#pragma GCC diagnostic pop
#endif

    return bytes;
}

QString mp::Utils::make_uuid(const std::optional<std::string>& seed) const
{
    return mp::utils::make_uuid(seed);
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

bool mp::Utils::is_running(const VirtualMachine::State& state) const
{
    return state == VirtualMachine::State::running || state == VirtualMachine::State::delayed_shutdown;
}

void mp::utils::check_and_create_config_file(const QString& config_file_path)
{
    QFile config_file{config_file_path};

    if (!config_file.exists())
    {
        MP_UTILS.make_dir({}, QFileInfo{config_file_path}.dir().path()); // make sure parent dir is there
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

std::string mp::utils::get_resolved_target(mp::SSHSession& session, const std::string& target)
{
    std::string absolute;

    switch (target[0])
    {
    case '~':
        absolute = MP_UTILS.run_in_ssh_session(
            session,
            fmt::format("echo ~{}", mp::utils::escape_for_shell(target.substr(1, target.size() - 1))));
        break;
    case '/':
        absolute = target;
        break;
    default:
        absolute =
            MP_UTILS.run_in_ssh_session(session, fmt::format("echo $PWD/{}", mp::utils::escape_for_shell(target)));
        break;
    }

    return absolute;
}

// Split a path into existing and to-be-created parts.
std::pair<std::string, std::string> mp::utils::get_path_split(mp::SSHSession& session, const std::string& target)
{
    std::string absolute{get_resolved_target(session, target)};

    std::string existing = MP_UTILS.run_in_ssh_session(
        session,
        fmt::format("sudo /bin/bash -c 'P={:?}; while [ ! -d \"$P/\" ]; do P=\"${{P%/*}}\"; done; echo $P/'",
                    absolute));

    return {existing,
            QDir(QString::fromStdString(existing)).relativeFilePath(QString::fromStdString(absolute)).toStdString()};
}

// Create a directory on a given root folder.
void mp::utils::make_target_dir(mp::SSHSession& session, const std::string& root, const std::string& relative_target)
{
    MP_UTILS.run_in_ssh_session(session,
                                fmt::format("sudo /bin/bash -c 'cd {:?} && mkdir -p {:?}'", root, relative_target));
}

// Set ownership of all directories on a path starting on a given root.
// Assume it is already created.
void mp::utils::set_owner_for(mp::SSHSession& session, const std::string& root, const std::string& relative_target,
                              int vm_user, int vm_group)
{
    MP_UTILS.run_in_ssh_session(session,
                                fmt::format("sudo /bin/bash -c 'cd {:?} && chown -R {}:{} {:?}'",
                                            root,
                                            vm_user,
                                            vm_group,
                                            relative_target.substr(0, relative_target.find_first_of('/'))));
}

mp::Path mp::Utils::derive_instances_dir(const mp::Path& data_dir,
                                         const mp::Path& backend_directory_name,
                                         const mp::Path& instances_subdir) const
{
    if (backend_directory_name.isEmpty())
        return QDir(data_dir).filePath(instances_subdir);
    else
        return QDir(QDir(data_dir).filePath(backend_directory_name)).filePath(instances_subdir);
}

void mp::Utils::sleep_for(const std::chrono::milliseconds& ms) const
{
    std::this_thread::sleep_for(ms);
}

bool mp::Utils::is_ipv4_valid(const std::string& ipv4) const
{
    try
    {
        (mp::IPAddress(ipv4));
    }
    catch (std::invalid_argument&)
    {
        return false;
    }

    return true;
}

mp::Path mp::Utils::default_mount_target(const Path& source) const
{
    return source.isEmpty() ? "" : QDir{QDir::cleanPath(source)}.dirName().prepend("/home/ubuntu/");
}

auto mp::utils::find_bridge_with(const std::vector<mp::NetworkInterfaceInfo>& networks,
                                 const std::string& target_network,
                                 const std::string& bridge_type) -> std::optional<mp::NetworkInterfaceInfo>
{
    const auto it = std::find_if(std::cbegin(networks),
                                 std::cend(networks),
                                 [&target_network, &bridge_type](const NetworkInterfaceInfo& info) {
                                     return info.type == bridge_type && info.has_link(target_network);
                                 });
    return it == std::cend(networks) ? std::nullopt : std::make_optional(*it);
}
