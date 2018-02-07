/*
 * Copyright (C) 2017-2018 Canonical, Ltd.
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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 *
 */

#include "list.h"

#include <multipass/cli/argparser.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <iomanip>
#include <iostream>
#include <sstream>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

namespace
{
std::string status_string(const multipass::ListVMInstance_Status& status)
{
    std::string status_val;

    switch (status)
    {
    case mp::ListVMInstance_Status_RUNNING:
        status_val = "RUNNING";
        break;
    case mp::ListVMInstance_Status_STOPPED:
        status_val = "STOPPED";
        break;
    case mp::ListVMInstance_Status_TRASHED:
        status_val = "DELETED";
        break;
    default:
        status_val = "UNKOWN";
        break;
    }
    return status_val;
}

std::ostream& operator<<(std::ostream& out, const multipass::ListVMInstance& instance)
{
    std::string ipv4{instance.ipv4()};
    std::string ipv6{instance.ipv6()};
    if (ipv4.empty())
        ipv4.append("--");
    if (ipv6.empty())
        ipv6.append("--");
    out << std::setw(24) << std::left << instance.name();
    out << std::setw(9) << std::left << status_string(instance.status());
    out << std::setw(17) << std::left << ipv4;
    out << instance.current_release();

    return out;
}

QString toJson(mp::ListReply& reply)
{
    QJsonObject list_json;
    QJsonArray instances;

    for (const auto& instance : reply.instances())
    {
        QJsonObject instance_obj;
        instance_obj.insert("name", QString::fromStdString(instance.name()));
        instance_obj.insert("state", QString::fromStdString(status_string(instance.status())));

        QJsonArray ipv4_addrs;
        if (!instance.ipv4().empty())
            ipv4_addrs.append(QString::fromStdString(instance.ipv4()));
        instance_obj.insert("ipv4", ipv4_addrs);

        instances.append(instance_obj);
    }

    list_json.insert("list", instances);

    return QString(QJsonDocument(list_json).toJson());
}
}

mp::ReturnCode cmd::List::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](ListReply& reply) {
        if (format_type == "json")
        {
            cout << toJson(reply).toStdString();
        }
        else
        {
            const auto size = reply.instances_size();
            if (size < 1)
            {
                cout << "No instances found."
                     << "\n";
            }
            else
            {
                std::stringstream out;

                out << std::setw(24) << std::left << "Name";
                out << std::setw(9) << std::left << "State";
                out << std::setw(17) << std::left << "IPv4";
                out << std::left << "Release";
                out << "\n";

                for (const auto& instance : reply.instances())
                {
                    out << instance << "\n";
                }
                cout << out.str();
            }
        }
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "list failed: " << status.error_message() << "\n";
        return ReturnCode::CommandFail;
    };

    ListRequest request;
    return dispatch(&RpcMethod::list, request, on_success, on_failure);
}

std::string cmd::List::name() const
{
    return "list";
}

std::vector<std::string> cmd::List::aliases() const
{
    return {name(), "ls"};
}

QString cmd::List::short_help() const
{
    return QStringLiteral("List all available instances");
}

QString cmd::List::description() const
{
    return QStringLiteral("List all instances which have been created.");
}

mp::ParseCode cmd::List::parse_args(mp::ArgParser* parser)
{
    QCommandLineOption formatOption("format",
                                    "Output list in the requested format.\nValid formats are: table (default) and json",
                                    "format", "table");

    parser->addOption(formatOption);

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() > 0)
    {
        cerr << "This command takes no arguments\n";
        return ParseCode::CommandLineError;
    }

    if (parser->isSet(formatOption))
    {
        QString format_value{parser->value(formatOption)};
        QStringList valid_formats{"table", "json"};

        if (!valid_formats.contains(format_value))
        {
            cerr << "Invalid format type given.\n";
            status = ParseCode::CommandLineError;
        }
        else
            format_type = format_value;
    }

    return status;
}
