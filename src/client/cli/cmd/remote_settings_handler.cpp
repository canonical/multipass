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

#include "remote_settings_handler.h"
#include "animated_spinner.h"
#include "common_callbacks.h"

#include <multipass/cli/command.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/logging/log.h>

#include <cassert>
#include <stdexcept>
#include <utility>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "remote settings";

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
    RemoteGet(const QString& key, mp::Rpc::StubInterface& stub, mp::Terminal* term, int verbosity)
        : RemoteSettingsCmd{stub, term} // need to ensure refs outlive this
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
    RemoteSet(const QString& key,
              const QString& val,
              mp::Rpc::StubInterface& stub,
              mp::Terminal* term,
              int verbosity,
              bool user_authorized)
        : RemoteSettingsCmd{stub, term} // need to ensure refs outlive this
    {
        mp::SetRequest set_request;
        set_request.set_verbosity_level(verbosity);
        set_request.set_key(key.toStdString());
        set_request.set_val(val.toStdString());
        set_request.set_authorized(user_authorized);

        mp::AnimatedSpinner spinner{cout};

        auto streaming_confirmation_callback = mp::make_confirmation_callback<mp::SetRequest, mp::SetReply>(*term, key);

        [[maybe_unused]] auto ret = dispatch(&RpcMethod::set,
                                             set_request,
                                             on_success<mp::SetReply>,
                                             on_failure,
                                             streaming_confirmation_callback);
        assert(ret == mp::ReturnCode::Ok && "should have thrown otherwise");
    }
};

class RemoteKeys : public RemoteSettingsCmd
{
public:
    RemoteKeys(mp::Rpc::StubInterface& stub, mp::Terminal* term, int verbosity) : RemoteSettingsCmd{stub, term}
    {
        mp::KeysRequest keys_request;
        keys_request.set_verbosity_level(verbosity);

        auto custom_on_success = [this](mp::KeysReply& reply) {
            for (auto& key : *reply.mutable_settings_keys())
                keys.insert(QString::fromStdString(std::move(key))); // no actual move until QString supports it

            return mp::ReturnCode::Ok;
        };

        auto custom_on_failure = [](grpc::Status& status) {
            if (auto code = status.error_code(); code == grpc::NOT_FOUND)
            {
                mpl::log(mpl::Level::error, category, "Could not reach daemon.");
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

mp::RemoteSettingsHandler::RemoteSettingsHandler(QString key_prefix, mp::Rpc::StubInterface& stub,
                                                 multipass::Terminal* term, int verbosity)
    : key_prefix{std::move(key_prefix)}, stub{stub}, term{term}, verbosity{verbosity}
{
    assert(term);
}

QString mp::RemoteSettingsHandler::get(const QString& key) const
{
    if (key.startsWith(key_prefix))
    {
        assert(term);
        return RemoteGet{key, stub, term, verbosity}.got;
    }

    throw mp::UnrecognizedSettingException{key};
}

void mp::RemoteSettingsHandler::set(const QString& key, const QString& val)
{
    if (key.startsWith(key_prefix))
    {
        assert(term);
        RemoteSet(key, val, stub, term, verbosity, false);
    }
    else
        throw mp::UnrecognizedSettingException{key};
}

std::set<QString> mp::RemoteSettingsHandler::keys() const
{
    assert(term);
    return RemoteKeys{stub, term, verbosity}.keys;
}

mp::RemoteHandlerException::RemoteHandlerException(grpc::Status status)
    : std::runtime_error{"Error accessing remote setting"}, status{std::move(status)}
{
}

grpc::Status mp::RemoteHandlerException::get_status() const
{
    return status;
}
