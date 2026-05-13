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

#include "config.h"
#include "common_cli.h"

#include <multipass/cli/argparser.h>
#include <multipass/constants.h>
#include <multipass/exceptions/settings_exceptions.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/settings/settings.h>

#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QFile>
#include <QString>
#include <QStringList>
#include <QTemporaryFile>
#include <QtGlobal>

#include <cstdlib>
#include <exception>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace mp = multipass;
namespace cmd = multipass::cmd;

namespace
{
QString resolve_editor()
{
    auto visual = qEnvironmentVariable("VISUAL");
    if (!visual.isEmpty())
        return visual;

    auto editor = qEnvironmentVariable("EDITOR");
    if (!editor.isEmpty())
        return editor;

#if defined(Q_OS_WIN)
    return QStringLiteral("notepad");
#elif defined(Q_OS_MAC)
    return QStringLiteral("open -W -t");
#else
    // Known-blocking terminal editor — xdg-open detaches and would race the apply step.
    return QStringLiteral("vi");
#endif
}

int launch_editor(const QString& editor, const QString& path)
{
    // std::system so terminal editors (vim, nano) inherit the TTY.
    const auto cmd = (editor + " \"" + path + "\"").toStdString();
    return std::system(cmd.c_str());
}

// Insert val at the nested path described by `parts` into `node`, materialising
// intermediate Maps as needed. Recursive so the alias chain stays intact at
// variable depth (yaml-cpp's reassignment-by-`=` would otherwise collapse it).
void set_nested(YAML::Node node, int idx, const QStringList& parts, const std::string& val)
{
    const auto key = parts[idx].toStdString();
    if (idx + 1 == parts.size())
        node[key] = val;
    else
        set_nested(node[key], idx + 1, parts, val);
}

// Walk a YAML tree, emitting (dotted-key, value) pairs for every scalar leaf.
void flatten(const YAML::Node& node,
             const QString& prefix,
             std::vector<std::pair<QString, QString>>& out)
{
    for (const auto& kv : node)
    {
        const auto child = prefix.isEmpty()
                               ? QString::fromStdString(kv.first.as<std::string>())
                               : prefix + "." + QString::fromStdString(kv.first.as<std::string>());
        if (kv.second.IsMap())
            flatten(kv.second, child, out);
        else
            out.emplace_back(child, QString::fromStdString(kv.second.as<std::string>()));
    }
}
} // namespace

mp::ReturnCodeVariant cmd::Config::run(mp::ArgParser* parser)
{
    auto parse_code = parse_args(parser);
    auto ret = parser->returnCodeFrom(parse_code);
    if (parse_code != ParseCode::Ok)
        return ret;

    // 1. Fetch daemon-side YAML.
    std::string daemon_yaml;
    {
        GetConfigRequest request;
        request.set_verbosity_level(parser->verbosityLevel());

        auto on_success = [&daemon_yaml](GetConfigReply& reply) -> ReturnCodeVariant {
            daemon_yaml = reply.yaml_content();
            return ReturnCode::Ok;
        };
        auto on_failure = [this](grpc::Status& status) -> ReturnCodeVariant {
            cerr << "Failed to fetch configuration: " << status.error_message() << "\n";
            if (!status.error_details().empty())
                cerr << status.error_details() << "\n";
            return ReturnCode::CommandFail;
        };

        auto result = dispatch(&RpcMethod::get_config, request, on_success, on_failure);
        if (std::holds_alternative<ReturnCode>(result) &&
            std::get<ReturnCode>(result) != ReturnCode::Ok)
            return result;
    }

    // 2. Merge client-only keys (anything not under `local.`) into the daemon YAML.
    YAML::Node root = daemon_yaml.empty() ? YAML::Node(YAML::NodeType::Map) : YAML::Load(daemon_yaml);
    const auto daemon_prefix = QString{daemon_settings_root} + ".";
    for (const auto& qkey : MP_SETTINGS.keys())
    {
        if (qkey.startsWith(daemon_prefix) || qkey == passphrase_key)
            continue; // daemon already covered these
        try
        {
            const auto val = MP_SETTINGS.get(qkey).toStdString();
            set_nested(root, 0, qkey.split('.'), val);
        }
        catch (const std::exception& e)
        {
            cerr << "Warning: could not read '" << qkey.toStdString() << "': " << e.what() << "\n";
        }
    }

    // Re-emit the merged YAML — this is what the user actually sees and edits.
    std::string original_yaml;
    {
        YAML::Emitter emitter;
        emitter << root;
        if (emitter.c_str())
            original_yaml.assign(emitter.c_str(), emitter.size());
    }

    // 3. Write to temp file and open the editor.
    QTemporaryFile tmp{QDir::tempPath() + "/multipass-config-XXXXXX.yaml"};
    tmp.setAutoRemove(true);
    if (!tmp.open())
    {
        cerr << "Failed to create temporary file: " << tmp.errorString().toStdString() << "\n";
        return ReturnCode::CommandFail;
    }
    tmp.write(original_yaml.data(), static_cast<qint64>(original_yaml.size()));
    tmp.flush();
    const auto path = tmp.fileName();
    tmp.close();

    const auto rc = launch_editor(resolve_editor(), path);
    if (rc != 0)
    {
        cerr << "Editor exited with status " << rc << "\n";
        return ReturnCode::CommandFail;
    }

    QFile reread{path};
    if (!reread.open(QIODevice::ReadOnly))
    {
        cerr << "Failed to read edited file: " << reread.errorString().toStdString() << "\n";
        return ReturnCode::CommandFail;
    }
    const auto edited_bytes = reread.readAll();
    reread.close();
    const std::string edited_yaml{edited_bytes.constData(),
                                  static_cast<std::size_t>(edited_bytes.size())};

    if (edited_yaml.empty() || edited_yaml == original_yaml)
    {
        cout << "No changes; configuration unchanged.\n";
        return ReturnCode::Ok;
    }

    // 4. Parse both, diff, apply only changed keys via MP_SETTINGS. Routing happens
    //    automatically: client-side keys are handled locally, `local.*` keys go to
    //    the daemon (including bridge-auth flow).
    YAML::Node edited_root;
    try
    {
        edited_root = YAML::Load(edited_yaml);
    }
    catch (const std::exception& e)
    {
        cerr << "Edited file is not valid YAML: " << e.what() << "\n";
        return ReturnCode::CommandFail;
    }

    std::vector<std::pair<QString, QString>> edited_pairs;
    flatten(edited_root, {}, edited_pairs);

    std::vector<std::pair<QString, QString>> original_pairs;
    flatten(root, {}, original_pairs);
    std::map<QString, QString> original_lookup;
    for (auto& [k, v] : original_pairs)
        original_lookup.emplace(k, v);

    for (const auto& [key, val] : edited_pairs)
    {
        const auto it = original_lookup.find(key);
        if (it != original_lookup.end() && it->second == val)
            continue; // unchanged; skip to avoid side-effects on no-op writes

        try
        {
            MP_SETTINGS.set(key, val);
        }
        catch (const std::exception& e)
        {
            cerr << "Error applying '" << key.toStdString() << "': " << e.what() << "\n";
            return ReturnCode::CommandFail;
        }
    }

    return ReturnCode::Ok;
}

std::string cmd::Config::name() const
{
    return "config";
}

QString cmd::Config::short_help() const
{
    return QStringLiteral("Edit Multipass configuration in your editor");
}

QString cmd::Config::description() const
{
    return QStringLiteral(
        "Open all Multipass settings in your default text editor as a nested YAML file. "
        "When the editor exits, any changes are applied in order. On a validation error, "
        "the error is printed and the command exits; previously-applied keys stay applied.\n"
        "The editor is selected via $VISUAL, then $EDITOR, then a platform default.");
}

mp::ParseCode cmd::Config::parse_args(mp::ArgParser* parser)
{
    auto status = parser->commandParse(this);
    if (status != ParseCode::Ok)
        return status;

    if (!parser->positionalArguments().isEmpty())
    {
        cerr << "This command does not take positional arguments.\n";
        return ParseCode::CommandLineError;
    }

    return ParseCode::Ok;
}
