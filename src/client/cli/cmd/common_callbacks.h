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
 */

#ifndef MULTIPASS_COMMON_CALLBACKS_H
#define MULTIPASS_COMMON_CALLBACKS_H

#include "animated_spinner.h"

#include <multipass/cli/client_common.h>
#include <multipass/cli/prompters.h>
#include <multipass/constants.h>
#include <multipass/terminal.h>

#include <grpc++/grpc++.h>

namespace multipass
{
template <typename Request, typename Reply>
auto make_logging_spinner_callback(AnimatedSpinner& spinner, std::ostream& stream)
{
    return [&spinner, &stream](const Reply& reply, grpc::ClientReaderWriterInterface<Request, Reply>*) {
        if (!reply.log_line().empty())
            spinner.print(stream, reply.log_line());
    };
}

template <typename Request, typename Reply>
auto make_reply_spinner_callback(AnimatedSpinner& spinner, std::ostream& stream)
{
    return [&spinner, &stream](const Reply& reply, grpc::ClientReaderWriterInterface<Request, Reply>*) {
        if (!reply.log_line().empty())
            spinner.print(stream, reply.log_line());

        if (const auto& msg = reply.reply_message(); !msg.empty())
        {
            spinner.stop();
            spinner.start(msg);
        }
    };
}

template <typename Request, typename Reply>
auto make_iterative_spinner_callback(AnimatedSpinner& spinner, Terminal& term)
{
    return [&spinner, &term](const Reply& reply, grpc::ClientReaderWriterInterface<Request, Reply>* client) {
        if (!reply.log_line().empty())
            spinner.print(term.cerr(), reply.log_line());

        if (reply.password_requested())
        {
            spinner.stop();

            return cmd::handle_password(client, &term);
        }

        if (const auto& msg = reply.reply_message(); !msg.empty())
        {
            spinner.stop();
            spinner.start(msg);
        }
    };
}

template <typename Request, typename Reply>
auto make_confirmation_callback(Terminal& term, const QString& key)
{
    return [&key, &term](Reply& reply, grpc::ClientReaderWriterInterface<Request, Reply>* client) {
        if (key.startsWith(daemon_settings_root) && key.endsWith(bridged_network_name) && reply.needs_authorization())
        {
            auto bridged_network = reply.reply_message();

            std::vector<std::string> nets(1, bridged_network);

            BridgePrompter prompter(&term);

            auto request = Request{};
            auto answer = prompter.bridge_prompt(nets);
            request.set_authorized(answer);

            client->Write(request);
        }
    };
}
} // namespace multipass

#endif // MULTIPASS_COMMON_CALLBACKS_H
