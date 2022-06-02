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
#include <multipass/ssh/throw_on_error.h>
#include <multipass/utils.h>

#include "ssh_client_key_provider.h"

#include <multipass/format.h>

#include <array>
#include <cassert>
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
using SFTPAttributesUPtr = std::unique_ptr<sftp_attributes_struct, void (*)(sftp_attributes)>;

mp::SFTPSessionUPtr make_sftp_session(ssh_session session)
{
    mp::SFTPSessionUPtr sftp{sftp_new(session), sftp_free};

    if (sftp == nullptr)
        throw std::runtime_error(fmt::format("could not create new sftp session: {}", ssh_get_error(session)));

    return sftp;
}

std::string full_destination(const std::string& destination_path, const std::string& source_path)
{
    const auto source_file_name = mp::utils::filename_for(source_path);

    if (!QFileInfo::exists(QString::fromStdString(destination_path)))
    {
        const auto parent_path = QFileInfo{QString::fromStdString(destination_path)}.dir();
        return parent_path.exists() ? destination_path : throw std::runtime_error{"[sftp] local target does not exist"};
    }

    if (!mp::utils::is_dir(destination_path))
        return destination_path;

    auto destination_full_path = fmt::format("{}/{}", destination_path, source_file_name);
    if (mp::utils::is_dir(destination_full_path))
        throw std::runtime_error{
            fmt::format("[sftp] cannot overwrite local directory '{}' with non-directory", destination_full_path)};

    return destination_full_path;
}

std::string full_destination(sftp_session sftp, const std::string& destination_path, const std::string& source_path)
{
    const auto source_file_name = mp::utils::filename_for(source_path);
    auto destination_full_path = destination_path.empty() ? source_file_name : destination_path;

    SFTPAttributesUPtr destination_attr{sftp_stat(sftp, destination_full_path.c_str()), sftp_attributes_free};
    if (!destination_attr)
    {
        const auto parent_path = QFileInfo{QString::fromStdString(destination_full_path)}.dir().path().toStdString();
        destination_attr.reset(sftp_stat(sftp, parent_path.c_str()));
        return destination_attr ? destination_full_path : throw mp::SSHException{"[sftp] remote target does not exist"};
    }

    if (destination_attr->type != SSH_FILEXFER_TYPE_DIRECTORY)
        return destination_full_path;

    destination_full_path = fmt::format("{}/{}", destination_full_path, source_file_name);
    destination_attr.reset(sftp_stat(sftp, destination_full_path.c_str()));
    if (destination_attr && destination_attr->type == SSH_FILEXFER_TYPE_DIRECTORY)
        throw mp::SSHException{
            fmt::format("[sftp] cannot overwrite remote directory '{}' with non-directory", destination_full_path)};

    return destination_full_path;
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
    auto full_destination_path = full_destination(sftp.get(), destination_path, source_path);
    SFTPFileUPtr file_handle{
        sftp_open(sftp.get(), full_destination_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, file_mode), sftp_close};
    if (!file_handle)
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
    auto full_destination_path = full_destination(destination_path, source_path);
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
    auto full_destination_path = full_destination(sftp.get(), destination_path, stream_file_name);
    SFTPFileUPtr file_handle{
        sftp_open(sftp.get(), full_destination_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, file_mode), sftp_close};
    if (!file_handle)
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

        assert(r <= max_transfer);
        cout.write(data.data(), r);
    }
}
