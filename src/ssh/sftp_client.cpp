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

#include <multipass/ssh/sftp_client.h>

#include "ssh_client_key_provider.h"
#include <multipass/file_ops.h>
#include <multipass/logging/log.h>
#include <multipass/ssh/sftp_utils.h>
#include <multipass/ssh/throw_on_error.h>
#include <multipass/utils.h>

#include <array>
#include <fcntl.h>
#include <fmt/std.h>

constexpr int file_mode = 0664;
constexpr auto max_transfer = 65536u;
const std::string stream_file_name{"stream_output.dat"};
const char* log_category = "sftp";

namespace multipass
{
namespace mpl = logging;

SFTPSessionUPtr make_sftp_session(ssh_session session)
{
    auto sftp = mp_sftp_new(session);
    if (!sftp)
        throw std::runtime_error(fmt::format("[sftp] could not create new sftp session: {}", ssh_get_error(session)));

    return sftp;
}

SFTPClient::SFTPClient(const std::string& host, int port, const std::string& username, const std::string& priv_key_blob)
    : SFTPClient{std::make_unique<SSHSession>(host, port, username, SSHClientKeyProvider(priv_key_blob))}
{
}

SFTPClient::SFTPClient(SSHSessionUPtr ssh_session)
    : ssh_session{std::move(ssh_session)}, sftp{make_sftp_session(*this->ssh_session)}
{
    SSH::throw_on_error(sftp, *this->ssh_session, "[sftp] init failed", sftp_init);
}

bool SFTPClient::is_remote_dir(const fs::path& path)
{
    auto attr = mp_sftp_stat(sftp.get(), path.u8string().c_str());
    return attr && attr->type == SSH_FILEXFER_TYPE_DIRECTORY;
}

bool SFTPClient::push(const fs::path& source_path, const fs::path& target_path, const Flags flags)
try
{
    auto source = source_path.string();
    utils::trim_end(source, [](char ch) { return ch == '/' || ch == fs::path::preferred_separator; });

    std::error_code err;
    if (MP_FILEOPS.is_directory(source_path, err) && !err)
    {
        if (!flags.testFlag(Flag::Recursive))
            throw SFTPError{"omitting local directory {}: recursive mode not specified", source_path};

        auto full_target_path = MP_SFTPUTILS.get_remote_dir_target(sftp.get(), source, target_path,
                                                                   flags.testFlag(SFTPClient::Flag::MakeParent));
        return push_dir(source, full_target_path);
    }
    else if (err)
        throw SFTPError{"cannot access {}: {}", source_path, err.message()};

    auto full_target_path = MP_SFTPUTILS.get_remote_file_target(sftp.get(), source, target_path,
                                                                flags.testFlag(SFTPClient::Flag::MakeParent));
    push_file(source, full_target_path);
    return true;
}
catch (const SFTPError& e)
{
    mpl::log(mpl::Level::error, log_category, e.what());
    return false;
}

bool SFTPClient::pull(const fs::path& source_path, const fs::path& target_path, const Flags flags)
try
{
    auto source = source_path.string();
    utils::trim_end(source, [](char ch) { return ch == '/' || ch == fs::path::preferred_separator; });

    if (is_remote_dir(source_path))
    {
        if (!flags.testFlag(Flag::Recursive))
            throw SFTPError{"omitting remote directory {}: recursive mode not specified", source_path};

        auto full_target_path =
            MP_SFTPUTILS.get_local_dir_target(source, target_path, flags.testFlag(SFTPClient::Flag::MakeParent));
        return pull_dir(source, full_target_path);
    }

    auto full_target_path =
        MP_SFTPUTILS.get_local_file_target(source, target_path, flags.testFlag(SFTPClient::Flag::MakeParent));
    pull_file(source, full_target_path);
    return true;
}
catch (const SFTPError& e)
{
    mpl::log(mpl::Level::error, log_category, e.what());
    return false;
}

void SFTPClient::push_file(const fs::path& source_path, const fs::path& target_path)
{
    auto local_file = MP_FILEOPS.open_read(source_path, std::ios_base::in | std::ios_base::binary);
    if (local_file->fail())
        throw SFTPError{"cannot open local file {}: {}", source_path, strerror(errno)};

    do_push_file(*local_file, target_path);

    std::error_code _;
    auto status = MP_FILEOPS.status(source_path, _);
    if (sftp_chmod(sftp.get(), target_path.u8string().c_str(), static_cast<mode_t>(status.permissions())) != SSH_FX_OK)
        throw SFTPError{"cannot set permissions for remote file {}: {}", target_path, ssh_get_error(sftp->session)};

    if (local_file->fail() && !local_file->eof())
        throw SFTPError{"cannot read from local file {}: {}", source_path, strerror(errno)};
}

void SFTPClient::pull_file(const fs::path& source_path, const fs::path& target_path)
{
    auto local_file = MP_FILEOPS.open_write(target_path, std::ios_base::out | std::ios_base::binary);
    if (local_file->fail())
        throw SFTPError{"cannot open local file {}: {}", target_path, strerror(errno)};

    do_pull_file(source_path, *local_file);

    auto source_perms = mp_sftp_stat(sftp.get(), source_path.u8string().c_str())->permissions;
    std::error_code err;
    if (MP_FILEOPS.permissions(target_path, static_cast<fs::perms>(source_perms), err); err)
        throw SFTPError{"cannot set permissions for local file {}: {}", target_path, err.message()};

    if (local_file->fail())
        throw SFTPError{"cannot write to local file {}: {}", target_path, strerror(errno)};
}

bool SFTPClient::push_dir(const fs::path& source_path, const fs::path& target_path)
{
    auto success = true;
    std::error_code err;

    auto local_iter = MP_FILEOPS.recursive_dir_iterator(source_path, err);
    if (err)
        throw SFTPError{"cannot open local directory {}: {}", source_path, err.message()};

    std::vector<std::pair<fs::path, fs::perms>> subdirectory_perms{
        {target_path, MP_FILEOPS.status(source_path, err).permissions()}};

    while (local_iter->hasNext())
    {
        try
        {
            const auto& entry = local_iter->next();
            auto remote_file_str =
                entry.path().u8string().replace(0, source_path.u8string().size(), target_path.u8string());
            std::replace(remote_file_str.begin(), remote_file_str.end(), (char)fs::path::preferred_separator, '/');
            const fs::path remote_file_path{remote_file_str};

            const auto status = entry.symlink_status();
            switch (status.type())
            {
            case fs::file_type::regular:
            {
                push_file(entry.path(), remote_file_path);
                break;
            }
            case fs::file_type::directory:
            {
                if (sftp_mkdir(sftp.get(), remote_file_path.u8string().c_str(), 0777) != SSH_OK &&
                    sftp_get_error(sftp.get()) != SSH_FX_FILE_ALREADY_EXISTS)
                    throw SFTPError{"cannot create remote directory {:?}: {}",
                                    remote_file_path,
                                    ssh_get_error(sftp->session)};

                subdirectory_perms.emplace_back(remote_file_path, status.permissions());
                break;
            }
            case fs::file_type::symlink:
            {
                auto link_target = MP_FILEOPS.read_symlink(entry.path(), err);
                if (err)
                    throw SFTPError{"cannot read local link {}: {}", entry.path(), err.message()};

                auto remote_file_info = mp_sftp_lstat(sftp.get(), remote_file_path.u8string().c_str());
                if (remote_file_info && remote_file_info->type == SSH_FILEXFER_TYPE_DIRECTORY)
                    throw SFTPError{"cannot overwrite remote directory {:?} with non-directory", remote_file_path};

                if ((sftp_unlink(sftp.get(), remote_file_path.u8string().c_str()) != SSH_FX_OK &&
                     sftp_get_error(sftp.get()) != SSH_FX_NO_SUCH_FILE) ||
                    sftp_symlink(sftp.get(), link_target.u8string().c_str(), remote_file_path.u8string().c_str()) !=
                        SSH_FX_OK)
                    throw SFTPError{"cannot create remote symlink {:?}: {}",
                                    remote_file_path,
                                    ssh_get_error(sftp->session)};
                break;
            }
            default:
                throw SFTPError{"cannot copy {}: not a regular file", entry.path()};
            }
        }
        catch (const SFTPError& e)
        {
            mpl::log(mpl::Level::error, log_category, e.what());
            success = false;
        }
    }

    for (auto it = subdirectory_perms.crbegin(); it != subdirectory_perms.crend(); ++it)
    {
        const auto& [path, perms] = *it;
        if (sftp_chmod(sftp.get(), path.u8string().c_str(), static_cast<mode_t>(perms)) != SSH_FX_OK)
        {
            mpl::log(
                mpl::Level::error, log_category,
                fmt::format("cannot set permissions for remote directory {}: {}", path, ssh_get_error(sftp->session)));
            success = false;
        }
    }

    return success;
}

bool SFTPClient::pull_dir(const fs::path& source_path, const fs::path& target_path)
{
    auto success = true;
    std::error_code err;

    auto remote_iter = MP_SFTPUTILS.make_SFTPDirIterator(sftp.get(), source_path);

    std::vector<std::pair<fs::path, mode_t>> subdirectory_perms{
        {target_path, mp_sftp_stat(sftp.get(), source_path.u8string().c_str())->permissions}};

    while (remote_iter->hasNext())
    {
        try
        {
            const auto entry = remote_iter->next();
            const auto local_file_path = target_path / (entry->name + source_path.string().size() + 1);

            switch (entry->type)
            {
            case SSH_FILEXFER_TYPE_REGULAR:
            {
                pull_file(entry->name, local_file_path);
                break;
            }
            case SSH_FILEXFER_TYPE_DIRECTORY:
            {
                if (MP_FILEOPS.create_directory(local_file_path, err); err)
                    throw SFTPError{"cannot create local directory {}: {}", local_file_path, err.message()};

                subdirectory_perms.emplace_back(local_file_path, entry->permissions);
                break;
            }
            case SSH_FILEXFER_TYPE_SYMLINK:
            {
                auto link_target = mp_sftp_readlink(sftp.get(), entry->name);
                if (!link_target)
                    throw SFTPError{"cannot read remote link \"{}\": {}", entry->name, ssh_get_error(sftp->session)};

                if (MP_FILEOPS.is_directory(local_file_path, err))
                    throw SFTPError{"cannot overwrite local directory {} with non-directory", local_file_path};

                if (MP_FILEOPS.remove(local_file_path, err); !err)
                    if (MP_FILEOPS.create_symlink(link_target.get(), local_file_path, err); !err)
                        break;

                throw SFTPError{"cannot create local symlink {}: {}", local_file_path, err.message()};
            }
            default:
                throw SFTPError{"cannot copy \"{}\": not a regular file", entry->name};
            }
        }
        catch (const SFTPError& e)
        {
            mpl::log(mpl::Level::error, log_category, e.what());
            success = false;
        }
    }

    for (auto it = subdirectory_perms.crbegin(); it != subdirectory_perms.crend(); ++it)
    {
        const auto& [path, perms] = *it;
        MP_FILEOPS.permissions(path, static_cast<fs::perms>(perms), err);
        if (err)
        {
            mpl::log(mpl::Level::error, log_category,
                     fmt::format("cannot set permissions for local directory {}: {}", path, err.message()));
            success = false;
        }
    }

    return success;
}

void SFTPClient::from_cin(std::istream& cin, const fs::path& target_path, bool make_parent)
{
    auto full_target_path = MP_SFTPUTILS.get_remote_file_target(sftp.get(), stream_file_name, target_path, make_parent);
    do_push_file(cin, full_target_path);
}

void SFTPClient::to_cout(const fs::path& source_path, std::ostream& cout)
{
    do_pull_file(source_path, cout);
}

void SFTPClient::do_push_file(std::istream& source, const fs::path& target_path)
{
    auto remote_file =
        mp_sftp_open(sftp.get(), target_path.u8string().c_str(), O_WRONLY | O_CREAT | O_TRUNC, file_mode);
    if (!remote_file)
        throw SFTPError{"cannot open remote file {}: {}", target_path, ssh_get_error(sftp->session)};

    std::array<char, max_transfer> buffer{};
    while (auto r = source.read(buffer.data(), buffer.size()).gcount())
        if (sftp_write(remote_file.get(), buffer.data(), r) < 0)
            throw SFTPError{"cannot write to remote file {}: {}", target_path, ssh_get_error(sftp->session)};
}

void SFTPClient::do_pull_file(const fs::path& source_path, std::ostream& target)
{
    auto remote_file = mp_sftp_open(sftp.get(), source_path.u8string().c_str(), O_RDONLY, 0);
    if (!remote_file)
        throw SFTPError{"cannot open remote file {}: {}", source_path, ssh_get_error(sftp->session)};

    std::array<char, max_transfer> buffer{};
    while (auto r = sftp_read(remote_file.get(), buffer.data(), buffer.size()))
    {
        if (r < 0)
            throw SFTPError{"cannot read from remote file {}: {}", source_path, ssh_get_error(sftp->session)};

        target.write(buffer.data(), r);
    }
}

} // namespace multipass
