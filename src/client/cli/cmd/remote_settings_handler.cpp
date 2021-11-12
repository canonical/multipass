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

#include "remote_settings_handler.h"

#include <multipass/cli/command.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/optional.h>

#include <cassert>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace mp = multipass;

namespace
{
class RemoteGet : public mp::cmd::Command // TODO@ricab feels hacky - revisit
{
public:
    RemoteGet(const QString& key, grpc::Channel& channel, mp::Rpc::Stub& stub, mp::Terminal* term, int verbosity)
        : mp::cmd::Command{channel, stub, term}, key{key}, verbosity{verbosity} // need to ensure refs outlive this
    {
    }

    mp::ReturnCode run()
    {
        return run(nullptr);
    }

private: // demote visibility of the following methods
    mp::ReturnCode run(mp::ArgParser*) override
    {
        mp::GetRequest get_request;
        get_request.set_verbosity_level(verbosity);
        get_request.set_key(key.toStdString());

        auto on_success = [this](mp::GetReply& reply) {
            got = QString::fromStdString(reply.value());
            return mp::ReturnCode::Ok;
        };

        auto on_failure = [this](grpc::Status& status) {
            std::stringstream grab_error; // TODO@ricab separate the part we need of the failure handler instead
            if (auto ret = mp::cmd::standard_failure_handler_for("internal", grab_error, status);
                ret == mp::ReturnCode::Ok)
                return ret;

            throw std::runtime_error{grab_error.str()}; // TODO@ricab throw and catch specific exception w/ ret code
        };

        return dispatch(&RpcMethod::get, get_request, on_success, on_failure);
    }

    std::string name() const override
    {
        return fail();
    }

    QString short_help() const override
    {
        return fail();
    }

    QString description() const override
    {
        return fail();
    }

    static const char* fail()
    {
        assert(false);
        throw std::logic_error{"shouldn't be here"};
    }

public:
    mp::optional<QString> got = mp::nullopt;

private:
    const QString& key; // careful, reference here
    int verbosity;
};
} // namespace

mp::RemoteSettingsHandler::RemoteSettingsHandler(QString key_prefix, grpc::Channel& channel, mp::Rpc::Stub& stub,
                                                 multipass::Terminal* term, int verbosity)
    : key_prefix{std::move(key_prefix)}, rpc_channel{channel}, stub{stub}, term{term}, verbosity{verbosity}
{
    assert(term);
}

QString mp::RemoteSettingsHandler::get(const QString& key) const
{
    if (key.startsWith(key_prefix))
    {
        assert(term);
        auto remote_get = RemoteGet(key, rpc_channel, stub, term, verbosity);
        auto result = remote_get.run();

        assert(result == ReturnCode::Ok && "should have thrown otherwise");
        assert(remote_get.got && "should have thrown otherwise");
        return *remote_get.got;
    }

    throw mp::UnrecognizedSettingException{key};
}

void mp::RemoteSettingsHandler::set(const QString& key, const QString& val) const
{
    if (key.startsWith(key_prefix))
    {
        ; // TODO@ricab
    }

    throw mp::UnrecognizedSettingException{key};
}

std::set<QString> mp::RemoteSettingsHandler::keys() const
{
    return std::set<QString>(); // TODO@ricab
}
