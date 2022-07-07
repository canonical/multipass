/*
 * Copyright (C) 2019-2022 Canonical, Ltd.
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
#include <multipass/format.h>
#include <multipass/ssh/sftp_utils.h>
#include <multipass/ssh/throw_on_error.h>

#include <array>
#include <fcntl.h>

constexpr int file_mode = 0664;
constexpr auto max_transfer = 65536u;
const char* stream_file_name = "stream_output.dat";

namespace multipass
{
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

bool SFTPClient::is_dir(const fs::path& path)
{
    auto attr = mp_sftp_stat(sftp.get(), path.c_str());
    return attr && attr->type == SSH_FILEXFER_TYPE_DIRECTORY;
}

bool SFTPClient::push(const fs::path& source_path, const fs::path& target_path, QFlags<TransferFlags> flags,
                      std::ostream& err_sink)
{
    std::error_code err;

    if (MP_FILEOPS.is_directory(source_path, err) && !err)
    {
        if (!flags.testFlag(TransferFlags::Recursive))
        {
            err_sink << fmt::format("[sftp] omitting local directory {}: -r not specified", source_path) << '\n';
            return false;
        }

        auto full_target_path = MP_SFTPUTILS.get_full_remote_dir_target(sftp.get(), source_path, target_path);
        return push_dir(source_path, full_target_path, err_sink);
    }
    else if (err)
        throw std::runtime_error{fmt::format("[sftp] cannot access {}: {}", source_path, err.message())};

    try
    {
        auto full_target_path = MP_SFTPUTILS.get_full_remote_file_target(sftp.get(), source_path, target_path);
        push_file(source_path, full_target_path);
        return true;
    }
    catch (const std::exception& e)
    {
        err_sink << e.what() << '\n';
        return false;
    }
}

bool SFTPClient::pull(const fs::path& source_path, const fs::path& target_path, QFlags<TransferFlags> flags,
                      std::ostream& err_sink)
{
    if (is_dir(source_path))
    {
        if (!flags.testFlag(TransferFlags::Recursive))
        {
            err_sink << fmt::format("[sftp] omitting remote directory {}: -r not specified", source_path) << '\n';
            return false;
        }

        auto full_target_path = MP_SFTPUTILS.get_full_local_dir_target(source_path, target_path);
        return pull_dir(source_path, full_target_path, err_sink);
    }

    try
    {
        auto full_target_path = MP_SFTPUTILS.get_full_local_file_target(source_path, target_path);
        pull_file(source_path, full_target_path);
        return true;
    }
    catch (const std::exception& e)
    {
        err_sink << e.what() << '\n';
        return false;
    }
}

void SFTPClient::push_file(const fs::path& source_path, const fs::path& target_path)
{
    auto local_file = MP_FILEOPS.open_read(source_path);
    if (local_file->fail())
        throw std::runtime_error{fmt::format("[sftp] cannot open local file {}: {}", source_path, strerror(errno))};

    do_push_file(*local_file, target_path);

    auto target_attr = mp_sftp_stat(sftp.get(), target_path.c_str());
    std::error_code _;
    auto status = MP_FILEOPS.status(source_path, _);

    target_attr->permissions = static_cast<mode_t>(status.permissions());
    if (sftp_setstat(sftp.get(), target_path.c_str(), target_attr.get()) != SSH_FX_OK)
        throw std::runtime_error{fmt::format("[sftp] cannot set permissions for remote file {}: {}", target_path,
                                             ssh_get_error(sftp->session))};

    if (local_file->fail() && !local_file->eof())
        throw std::runtime_error{
            fmt::format("[sftp] cannot read from local file {}: {}", source_path, strerror(errno))};
}

void SFTPClient::pull_file(const fs::path& source_path, const fs::path& target_path)
{
    auto local_file = MP_FILEOPS.open_write(target_path);
    if (local_file->fail())
        throw std::runtime_error{fmt::format("[sftp] cannot open local file {}: {}", target_path, strerror(errno))};

    do_pull_file(source_path, *local_file);

    auto source_perms = mp_sftp_stat(sftp.get(), source_path.c_str())->permissions;
    std::error_code err;
    MP_FILEOPS.permissions(target_path, static_cast<fs::perms>(source_perms), err);
    if (err)
        throw std::runtime_error{
            fmt::format("[sftp] cannot set permissions for local file {}: {}", target_path, err.message())};

    if (local_file->fail())
        throw std::runtime_error{fmt::format("[sftp] cannot write to local file {}: {}", target_path, strerror(errno))};
}

bool SFTPClient::push_dir(const fs::path& source_path, const fs::path& target_path, std::ostream& err_sink)
{
    auto success = true;
    std::error_code err;

    auto local_iter = MP_FILEOPS.recursive_dir_iterator(source_path, err);
    if (err)
        throw std::runtime_error{fmt::format("[sftp] cannot open local directory {}: {}", source_path, err.message())};
    while (local_iter->hasNext())
    {
        try
        {
            auto entry = local_iter->next();
            auto remote_file_path = target_path / (entry.path().c_str() + source_path.string().size() + 1);

            switch (entry.symlink_status().type())
            {
            case fs::file_type::regular:
            {
                push_file(entry.path(), remote_file_path);
                break;
            }
            case fs::file_type::directory:
            {
                if (sftp_mkdir(sftp.get(), remote_file_path.c_str(), 0777) != SSH_OK &&
                    sftp_get_error(sftp.get()) != SSH_FX_FILE_ALREADY_EXISTS)
                    throw std::runtime_error{fmt::format("[sftp] cannot create remote directory {}: {}",
                                                         remote_file_path, ssh_get_error(sftp->session))};
                break;
            }
            case fs::file_type::symlink:
            {
                auto link_target = MP_FILEOPS.read_symlink(entry.path(), err);
                if (err)
                    throw std::runtime_error{
                        fmt::format("[sftp] cannot read local link {}: {}", entry.path(), err.message())};

                auto remote_file_info = mp_sftp_lstat(sftp.get(), remote_file_path.c_str());
                if (remote_file_info && remote_file_info->type == SSH_FILEXFER_TYPE_DIRECTORY)
                    throw std::runtime_error{fmt::format(
                        "[sftp] cannot overwrite remote directory {} with non-directory", remote_file_path)};

                if (sftp_unlink(sftp.get(), remote_file_path.c_str()) != SSH_FX_OK &&
                    sftp_get_error(sftp.get()) != SSH_FX_NO_SUCH_FILE)
                    throw std::runtime_error{fmt::format("[sftp] cannot create remote symlink {}: {}", remote_file_path,
                                                         ssh_get_error(sftp->session))};

                if (sftp_symlink(sftp.get(), link_target.c_str(), remote_file_path.c_str()) != SSH_FX_OK)
                    throw std::runtime_error{fmt::format("[sftp] cannot create remote symlink {}: {}", remote_file_path,
                                                         ssh_get_error(sftp->session))};
                break;
            }
            default:
                throw std::runtime_error{fmt::format("[sftp] cannot copy {}: not a regular file", entry.path())};
            }
        }
        catch (std::exception& e)
        {
            err_sink << e.what() << '\n';
            success = false;
        }
    }

    return success;
}

bool SFTPClient::pull_dir(const fs::path& source_path, const fs::path& target_path, std::ostream& err_sink)
{
    auto success = true;

    auto remote_iter = MP_SFTPUTILS.make_SFTPDirIterator(sftp.get(), source_path);
    while (remote_iter->hasNext())
    {
        try
        {
            auto entry = remote_iter->next();
            auto local_file_path = target_path / (entry->name + source_path.string().size() + 1);

            switch (entry->type)
            {
            case SSH_FILEXFER_TYPE_REGULAR:
            {
                pull_file(entry->name, local_file_path);
                break;
            }
            case SSH_FILEXFER_TYPE_DIRECTORY:
            {
                std::error_code err;
                MP_FILEOPS.create_directory(local_file_path, err);
                if (err)
                    throw std::runtime_error{
                        fmt::format("[sftp] cannot create local directory {}: {}", local_file_path, err.message())};
                break;
            }
            case SSH_FILEXFER_TYPE_SYMLINK:
            {
                auto link_target = mp_sftp_readlink(sftp.get(), entry->name);
                if (!link_target)
                    throw std::runtime_error{fmt::format("[sftp] cannot read remote link \"{}\": {}", entry->name,
                                                         ssh_get_error(sftp->session))};

                std::error_code err;
                if (MP_FILEOPS.is_directory(local_file_path, err) && !err)
                    throw std::runtime_error{
                        fmt::format("[sftp] cannot overwrite local directory {} with non-directory", local_file_path)};
                else if (err)
                    throw std::runtime_error{
                        fmt::format("[sftp] cannot access {}: {}", local_file_path, err.message())};

                MP_FILEOPS.remove(local_file_path, err);
                if (err)
                    throw std::runtime_error{
                        fmt::format("[sftp] cannot create local symlink {}: {}", local_file_path, err.message())};

                MP_FILEOPS.create_symlink(link_target.get(), local_file_path, err);
                if (err && err != std::errc::file_exists)
                    throw std::runtime_error{
                        fmt::format("[sftp] cannot create local symlink {}: {}", local_file_path, err.message())};
                break;
            }
            default:
                throw std::runtime_error{fmt::format("[sftp] cannot copy \"{}\": not a regular file", entry->name)};
            }
        }
        catch (std::exception& e)
        {
            err_sink << e.what() << '\n';
            success = false;
        }
    }

    return success;
}

void SFTPClient::from_cin(std::istream& cin, const fs::path& target_path)
{
    auto full_target_path = MP_SFTPUTILS.get_full_remote_file_target(sftp.get(), stream_file_name, target_path);
    do_push_file(cin, full_target_path);
}

void SFTPClient::to_cout(const fs::path& source_path, std::ostream& cout)
{
    do_pull_file(source_path, cout);
}

void SFTPClient::do_push_file(std::istream& source, const fs::path& target_path)
{
    auto remote_file = mp_sftp_open(sftp.get(), target_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, file_mode);
    if (!remote_file)
        throw std::runtime_error{
            fmt::format("[sftp] cannot open remote file {}: {}", target_path, ssh_get_error(sftp->session))};

    std::array<char, max_transfer> buffer{};
    while (auto r = source.read(buffer.data(), buffer.size()).gcount())
        if (sftp_write(remote_file.get(), buffer.data(), r) < 0)
            throw std::runtime_error{
                fmt::format("[sftp] cannot write to remote file {}: {}", target_path, ssh_get_error(sftp->session))};
}

void SFTPClient::do_pull_file(const fs::path& source_path, std::ostream& target)
{
    auto remote_file = mp_sftp_open(sftp.get(), source_path.c_str(), O_RDONLY, 0);
    if (!remote_file)
        throw std::runtime_error{
            fmt::format("[sftp] cannot open remote file {}: {}", source_path, ssh_get_error(sftp->session))};

    std::array<char, max_transfer> buffer{};
    while (auto r = sftp_read(remote_file.get(), buffer.data(), buffer.size()))
    {
        if (r < 0)
            throw std::runtime_error{
                fmt::format("[sftp] cannot read from remote file {}: {}", source_path, ssh_get_error(sftp->session))};

        target.write(buffer.data(), r);
    }
}

} // namespace multipass
