/*
 * Copyright (C) 2021-2022 Canonical, Ltd.
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

#include <cassert>
#include <stdexcept>
#include <utility>

namespace mp = multipass;

namespace
{
class InternalCmd : public mp::cmd::Command // TODO feels hacky, better untangle dispatch from commands
{
public:
    using mp::cmd::Command::Command;

private: // demote visibility of the following methods
    [[noreturn]] mp::ReturnCode run(mp::ArgParser*) override
    {
        fail();
    }

    [[noreturn]] std::string name() const override
    {
        fail();
    }

    [[noreturn]] QString short_help() const override
    {
        fail();
    }

    [[noreturn]] QString description() const override
    {
        fail();
    }

    [[noreturn]] static void fail()
    {
        assert(false);
        throw std::logic_error{"shouldn't be here"};
    }
};

class RemoteSettingsCmd : public InternalCmd
{
public:
    using InternalCmd::InternalCmd;

protected:
    [[noreturn]] static mp::ReturnCode on_failure(grpc::Status& status)
    {
        throw mp::RemoteHandlerException{status};
    }

    template <typename ReplyType>
    static mp::ReturnCode on_success(ReplyType&)
    {
        return mp::ReturnCode::Ok;
    }
};

class RemoteGet : public RemoteSettingsCmd
{
public:
    RemoteGet(const QString& key, grpc::Channel& channel, mp::Rpc::Stub& stub, mp::Terminal* term, int verbosity)
        : RemoteSettingsCmd{channel, stub, term} // need to ensure refs outlive this
    {
        mp::GetRequest get_request;
        get_request.set_verbosity_level(verbosity);
        get_request.set_key(key.toStdString());

        auto custom_on_success = [this](mp::GetReply& reply) {
            got = QString::fromStdString(reply.value());
            return mp::ReturnCode::Ok;
        };

        [[maybe_unused]] auto ret = dispatch(&RpcMethod::get, get_request, custom_on_success, on_failure);
        assert(ret == mp::ReturnCode::Ok && "should have thrown otherwise");
    }

public:
    QString got = {};
};

class RemoteSet : public RemoteSettingsCmd
{
public:
    RemoteSet(const QString& key, const QString& val, grpc::Channel& channel, mp::Rpc::Stub& stub, mp::Terminal* term,
              int verbosity)
        : RemoteSettingsCmd{channel, stub, term} // need to ensure refs outlive this
    {
        mp::SetRequest set_request;
        set_request.set_verbosity_level(verbosity);
        set_request.set_key(key.toStdString());
        set_request.set_val(val.toStdString());

        [[maybe_unused]] auto ret = dispatch(&RpcMethod::set, set_request, on_success<mp::SetReply>, on_failure);
        assert(ret == mp::ReturnCode::Ok && "should have thrown otherwise");
    }
};

class RemoteKeys : public RemoteSettingsCmd
{
public:
    RemoteKeys(QString fallback, grpc::Channel& channel, mp::Rpc::Stub& stub, mp::Terminal* term, int verbosity)
        : RemoteSettingsCmd{channel, stub, term}
    {
        mp::KeysRequest keys_request;
        keys_request.set_verbosity_level(verbosity);

        auto custom_on_success = [this](mp::KeysReply& reply) {
            for (auto& key : *reply.mutable_settings_keys())
                keys.insert(QString::fromStdString(std::move(key)));

            return mp::ReturnCode::Ok;
        };

        auto custom_on_failure = [this, fallback = std::move(fallback)](grpc::Status& status) {
            if (status.error_code() == grpc::StatusCode::NOT_FOUND)
            {
                keys.insert(std::move(fallback));
                return mp::ReturnCode::Ok;
            }

            return on_failure(status);
        };

        [[maybe_unused]] auto ret = dispatch(&RpcMethod::keys, keys_request, custom_on_success, custom_on_failure);
        assert(ret == mp::ReturnCode::Ok && "should have thrown otherwise");
    }

public:
    std::set<QString> keys = {};
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
        return RemoteGet{key, rpc_channel, stub, term, verbosity}.got;
    }

    throw mp::UnrecognizedSettingException{key};
}

void mp::RemoteSettingsHandler::set(const QString& key, const QString& val)
{
    if (key.startsWith(key_prefix))
    {
        assert(term);
        RemoteSet(key, val, rpc_channel, stub, term, verbosity);
    }
    else
        throw mp::UnrecognizedSettingException{key};
}

std::set<QString> mp::RemoteSettingsHandler::keys() const
{
    assert(term);

    auto fallback = QStringLiteral("%1* \t (need daemon to find out actual keys)").arg(key_prefix);
    return RemoteKeys{std::move(fallback), rpc_channel, stub, term, verbosity}.keys;
}

mp::RemoteHandlerException::RemoteHandlerException(grpc::Status status)
    : std::runtime_error{"Error reaching remote setting"}, status{std::move(status)}
{
}

grpc::Status mp::RemoteHandlerException::get_status() const
{
    return status;
}
