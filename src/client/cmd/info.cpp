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
 * Authored by: Chris Townsend <christopher.townsend@canonical.com>
 *
 */

#include "info.h"

#include <multipass/cli/argparser.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <iomanip>
#include <sstream>

namespace mp = multipass;
namespace cmd = multipass::cmd;
using RpcMethod = mp::Rpc::Stub;

namespace
{
std::string status_string(const multipass::InfoReply_Status& status)
{
    std::string status_val;

    switch(status)
    {
    case mp::InfoReply::RUNNING:
        status_val = "RUNNING";
        break;
    case mp::InfoReply::STOPPED:
        status_val = "STOPPED";
        break;
    case mp::InfoReply::TRASHED:
        status_val = "DELETED";
        break;
    default:
        status_val = "UNKOWN";
        break;
    }
    return status_val;
}

std::ostream& operator<<(std::ostream& out, const multipass::InfoReply::Info& info)
{
    std::string ipv4{info.ipv4()};
    std::string ipv6{info.ipv6()};
    if (ipv4.empty())
        ipv4.append("--");
    out << std::setw(16) << std::left << "Name:";
    out << info.name() << "\n";

    out << std::setw(16) << std::left << "State:";
    out << status_string(info.status()) << "\n";

    out << std::setw(16) << std::left << "IPv4:";
    out << ipv4 << "\n";

    if (!ipv6.empty())
    {
        out << std::setw(16) << std::left << "IPV6:";
        out << ipv6 << "\n";
    }

    out << std::setw(16) << std::left << "Release:";
    out << (info.current_release().empty() ? "--" : info.current_release()) << "\n";

    out << std::setw(16) << std::left << "Image hash:";
    out << info.id().substr(0, 12) << " (Ubuntu " << info.image_release() << ")\n";

    out << std::setw(16) << std::left << "Load:";
    out << (info.load().empty() ? "--" : info.load()) << "\n";

    out << std::setw(16) << std::left << "Disk usage:";
    out << (info.disk_usage().empty() ? "--" : info.disk_usage()) << "\n";

    out << std::setw(16) << std::left << "Memory usage:";
    out << (info.memory_usage().empty() ? "--" : info.memory_usage()) << "\n";

    auto mount_paths = info.mount_info().mount_paths();
    for (auto mount = mount_paths.cbegin(); mount != mount_paths.cend(); ++mount)
    {
        out << std::setw(16) << std::left << ((mount == mount_paths.cbegin()) ? "Mounts:" : " ")
            << std::setw(info.mount_info().longest_path_len()) << mount->source_path() << "  => "
            << mount->target_path() << "\n";
    }
    return out;
}

QString toJson(mp::InfoReply& reply)
{
    QJsonObject info_json;
    QJsonObject info_obj;

    info_json.insert("errors", QJsonArray());

    for (const auto& info : reply.info())
    {
        QJsonObject instance_info;
        instance_info.insert("state", QString::fromStdString(status_string(info.status())));
        instance_info.insert("image_hash", QString::fromStdString(info.id().substr(0, 12)));

        QJsonArray ipv4_addrs;
        if (!info.ipv4().empty())
            ipv4_addrs.append(QString::fromStdString(info.ipv4()));
        instance_info.insert("ipv4", ipv4_addrs);

        QJsonObject mounts;
        for (const auto& mount : info.mount_info().mount_paths())
        {
            QJsonObject entry;
            entry.insert("gid_mappings", QJsonArray());
            entry.insert("uid_mappings", QJsonArray());
            entry.insert("source_path", QString::fromStdString(mount.source_path()));

            mounts.insert(QString::fromStdString(mount.target_path()), entry);
        }
        instance_info.insert("mounts", mounts);

        info_obj.insert(QString::fromStdString(info.name()), instance_info);
    }
    info_json.insert("info", info_obj);

    return QString(QJsonDocument(info_json).toJson());
}
}

mp::ReturnCode cmd::Info::run(mp::ArgParser* parser)
{
    auto ret = parse_args(parser);
    if (ret != ParseCode::Ok)
    {
        return parser->returnCodeFrom(ret);
    }

    auto on_success = [this](mp::InfoReply& reply) {
        if (format_type == "json")
        {
            cout << toJson(reply).toStdString();
        }
        else
        {
            std::stringstream out;
            for (const auto& info : reply.info())
            {
                out << info << "\n";
            }
            auto output = out.str();
            if (!reply.info().empty())
                output.pop_back();
            cout << output;
        }
        return ReturnCode::Ok;
    };

    auto on_failure = [this](grpc::Status& status) {
        cerr << "info failed: " << status.error_message() << "\n";
        return ReturnCode::CommandFail;
    };

    return dispatch(&RpcMethod::info, request, on_success, on_failure);
}

std::string cmd::Info::name() const { return "info"; }

QString cmd::Info::short_help() const
{
    return QStringLiteral("Display information about instances");
}

QString cmd::Info::description() const
{
    return QStringLiteral("Display information about instances");
}

mp::ParseCode cmd::Info::parse_args(mp::ArgParser* parser)
{
    parser->addPositionalArgument("name", "Names of instances to display information about", "<name> [<name> ...]");

    QCommandLineOption formatOption("format",
                                    "Output info in the requested format.\nValid formats are: table (default) and json",
                                    "format", "table");

    parser->addOption(formatOption);

    auto status = parser->commandParse(this);

    if (status != ParseCode::Ok)
    {
        return status;
    }

    if (parser->positionalArguments().count() == 0)
    {
        cerr << "Name argument is required" << std::endl;
        status = ParseCode::CommandLineError;
    }
    else
    {
        for (const auto& arg : parser->positionalArguments())
        {
            auto entry = request.add_instance_name();
            entry->append(arg.toStdString());
        }
    }

    if (parser->isSet(formatOption))
    {
        QString format_value{parser->value(formatOption)};
        QStringList valid_formats{"table", "json"};

        if (!valid_formats.contains(format_value))
        {
            cerr << "Invalid format type given." << std::endl;
            status = ParseCode::CommandLineError;
        }
        else
            format_type = format_value;
    }

    return status;
}
