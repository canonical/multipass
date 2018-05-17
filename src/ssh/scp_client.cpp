/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#include <multipass/ssh/scp_client.h>
#include <multipass/ssh/throw_on_error.h>

#include "ssh_client_key_provider.h"

#include <QFile>
#include <QFileInfo>

namespace mp = multipass;

namespace
{
using SCPUPtr = std::unique_ptr<ssh_scp_struct, void (*)(ssh_scp)>;

SCPUPtr make_scp_session(ssh_session session, int mode, const char* path)
{
    SCPUPtr scp{ssh_scp_new(session, mode, path), ssh_scp_free};

    if (scp == nullptr)
        throw std::runtime_error("Could not create new scp session.");

    return scp;
}
}

mp::SCPClient::SCPClient(const std::string& host, int port, const std::string& username,
                         const std::string& priv_key_blob)
    : SCPClient{std::make_unique<mp::SSHSession>(host, port, username, mp::SSHClientKeyProvider(priv_key_blob))}
{
}

mp::SCPClient::SCPClient(SSHSessionUPtr ssh_session) : ssh_session{std::move(ssh_session)}
{
}

void mp::SCPClient::push_file(const std::string& source_path, const std::string& destination_path)
{
    SCPUPtr scp{make_scp_session(*ssh_session, SSH_SCP_WRITE, destination_path.c_str())};
    SSH::throw_on_error(ssh_scp_init, scp);

    QFile source(QString::fromStdString(source_path));
    const auto size{source.size()};
    int mode = 0664;
    SSH::throw_on_error(ssh_scp_push_file, scp, source_path.c_str(), size, mode);

    int total{0};
    const auto len{65536u};
    std::vector<char> data;
    data.reserve(len);

    if (!source.open(QIODevice::ReadOnly))
        throw std::runtime_error("Error opening file for reading: " + source.errorString().toStdString());

    do
    {
        auto r = source.read(data.data(), len);

        if (r == -1)
            throw std::runtime_error("Error reading file: " + source.errorString().toStdString());
        if (r == 0)
            break;

        SSH::throw_on_error(ssh_scp_write, scp, data.data(), r);

        total += r;
    } while (total < size);

    SSH::throw_on_error(ssh_scp_close, scp);
}

void mp::SCPClient::pull_file(const std::string& source_path, const std::string& destination_path)
{
    SCPUPtr scp{make_scp_session(*ssh_session, SSH_SCP_READ, source_path.c_str())};
    SSH::throw_on_error(ssh_scp_init, scp);

    auto r = ssh_scp_pull_request(scp.get());

    if (r != SSH_SCP_REQUEST_NEWFILE)
        throw std::runtime_error("Error receiving information for file");

    auto size = ssh_scp_request_get_size(scp.get());
    std::string filename{ssh_scp_request_get_filename(scp.get())};

    auto total{0u};
    const auto len{65536u};
    std::vector<char> data;
    data.reserve(len);

    QFile destination(QString::fromStdString(destination_path));
    if (!destination.open(QIODevice::WriteOnly))
        throw std::runtime_error("Error opening file for writing: " + destination.errorString().toStdString());

    SSH::throw_on_error(ssh_scp_accept_request, scp);

    do
    {
        r = ssh_scp_read(scp.get(), data.data(), len);

        if (r == 0)
            break;

        if (destination.write(data.data(), r) == -1)
            throw std::runtime_error("Error writing to file: " + destination.errorString().toStdString());

        total += r;
    } while (total < size);

    SSH::throw_on_error(ssh_scp_close, scp);
}
