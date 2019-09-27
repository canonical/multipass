/*
 * Copyright (C) 2019 Canonical, Ltd.
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
#include <multipass/ssh/throw_on_error.h>
#include <multipass/utils.h>

#include "ssh_client_key_provider.h"

#include <multipass/format.h>

#include <array>
#include <fcntl.h>

#include <QFile>

namespace mp = multipass;

namespace
{
// TODO: For push/pull, use actual file permissions
constexpr int file_mode = 0664;
constexpr auto max_transfer = 65536u;
const std::string stream_file_name{"stream_output.dat"};

using SFTPFileUPtr = std::unique_ptr<sftp_file_struct, int (*)(sftp_file)>;

mp::SFTPSessionUPtr make_sftp_session(ssh_session session)
{
    mp::SFTPSessionUPtr sftp{sftp_new(session), sftp_free};

    if (sftp == nullptr)
        throw std::runtime_error(fmt::format("could not create new sftp session: {}", ssh_get_error(session)));

    return sftp;
}

std::string full_destination(const std::string& destination_path, const std::string& filename)
{
    if (destination_path.empty())
    {
        return filename;
    }
    else if (mp::utils::is_dir(destination_path))
    {
        return fmt::format("{}/{}", destination_path, filename);
    }

    return destination_path;
}
} // namespace

mp::SFTPClient::SFTPClient(const std::string& host, int port, const std::string& username,
                           const std::string& priv_key_blob)
    : SFTPClient{std::make_unique<mp::SSHSession>(host, port, username, mp::SSHClientKeyProvider(priv_key_blob))}
{
}

mp::SFTPClient::SFTPClient(SSHSessionUPtr ssh_session)
    : ssh_session{std::move(ssh_session)}, sftp{make_sftp_session(*this->ssh_session)}
{
    SSH::throw_on_error(sftp, *this->ssh_session, "[sftp pull] init failed", sftp_init);
}

void mp::SFTPClient::push_file(const std::string& source_path, const std::string& destination_path)
{
    auto full_destination_path = full_destination(destination_path, mp::utils::filename_for(source_path));
    SFTPFileUPtr file_handle{
        sftp_open(sftp.get(), full_destination_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, file_mode), sftp_close};
    SSH::throw_on_error(sftp, *ssh_session, "[sftp push] open failed", sftp_get_error);

    QFile source(QString::fromStdString(source_path));
    if (!source.open(QIODevice::ReadOnly))
        throw std::runtime_error(fmt::format("[sftp push] error opening file for reading: {}", source.errorString()));

    std::array<char, max_transfer> data;
    while (true)
    {
        auto r = source.read(data.data(), data.size());

        if (r == -1)
            throw std::runtime_error(fmt::format("[sftp push] error reading file: {}", source.errorString()));

        if (r == 0)
            break;

        sftp_write(file_handle.get(), data.data(), r);
        SSH::throw_on_error(sftp, *ssh_session, "[sftp push] remote write failed", sftp_get_error);
    }
}

void mp::SFTPClient::pull_file(const std::string& source_path, const std::string& destination_path)
{
    auto full_destination_path = full_destination(destination_path, mp::utils::filename_for(source_path));
    QFile destination(QString::fromStdString(full_destination_path));
    if (!destination.open(QIODevice::WriteOnly))
        throw std::runtime_error(
            fmt::format("[sftp pull] error opening file for writing: {}", destination.errorString()));

    SFTPFileUPtr file_handle{sftp_open(sftp.get(), source_path.c_str(), O_RDONLY, file_mode), sftp_close};
    SSH::throw_on_error(sftp, *ssh_session, "[sftp pull] open failed", sftp_get_error);

    std::array<char, max_transfer> data;
    while (true)
    {
        auto r = sftp_read(file_handle.get(), data.data(), data.size());

        if (r == 0)
            break;

        if (r < 0)
            SSH::throw_on_error(sftp, *ssh_session, "[sftp pull] read failed", sftp_get_error);

        if (destination.write(data.data(), r) == -1)
            throw std::runtime_error(fmt::format("[sftp pull] error writing to file: {}", destination.errorString()));
    }
}

void mp::SFTPClient::stream_file(const std::string& destination_path, std::istream& cin)
{
    auto full_destination_path = full_destination(destination_path, stream_file_name);
    SFTPFileUPtr file_handle{
        sftp_open(sftp.get(), full_destination_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, file_mode), sftp_close};
    SSH::throw_on_error(sftp, *ssh_session, "[sftp stream] open failed", sftp_get_error);

    std::array<char, max_transfer> data;
    while (!cin.eof())
    {
        cin.read(data.data(), data.size());
        sftp_write(file_handle.get(), data.data(), cin.gcount());
        SSH::throw_on_error(sftp, *ssh_session, "[sftp push] remote write failed", sftp_get_error);
    }
}

void mp::SFTPClient::stream_file(const std::string& source_path, std::ostream& cout)
{
    SFTPFileUPtr file_handle{sftp_open(sftp.get(), source_path.c_str(), O_RDONLY, file_mode), sftp_close};
    SSH::throw_on_error(sftp, *ssh_session, "[sftp pull] open failed", sftp_get_error);

    std::array<char, max_transfer> data;
    while (true)
    {
        auto r = sftp_read(file_handle.get(), data.data(), data.size());

        if (r == 0)
            break;

        if (r < 0)
            SSH::throw_on_error(sftp, *ssh_session, "[sftp pull] read failed", sftp_get_error);

        cout << data.data();
    }
}
