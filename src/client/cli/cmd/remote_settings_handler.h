/*
 * Copyright (C) 2021 Canonical, Ltd.
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

#include "common_cli.h"

#include <multipass/settings/settings_handler.h>

#include <stdexcept>

namespace multipass
{
class RemoteSettingsHandler : public SettingsHandler
{
public:
    // need to ensure refs outlive this
    RemoteSettingsHandler(QString key_prefix, grpc::Channel& channel, Rpc::Stub& stub, Terminal* term, int verbosity);

    QString get(const QString& key) const override;
    void set(const QString& key, const QString& val) const override;
    std::set<QString> keys() const override;

private:
    QString key_prefix;
    grpc::Channel& rpc_channel;
    Rpc::Stub& stub;
    Terminal* term;
    int verbosity;
};

class RemoteSettingsException : public std::runtime_error
{
public:
    RemoteSettingsException(grpc::Status status);
    ReturnCode handle_failure(const std::string& command, std::ostream& cerr) const;

private:
    grpc::Status status;
};
} // namespace multipass

#ifndef MULTIPASS_REMOTE_SETTINGS_HANDLER_H
#define MULTIPASS_REMOTE_SETTINGS_HANDLER_H

#endif // MULTIPASS_REMOTE_SETTINGS_HANDLER_H
