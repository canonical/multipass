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

#include <multipass/cli/argparser.h>
#include <multipass/constants.h>
#include <multipass/settings/settings.h>

#include <yaml-cpp/yaml.h>

#include <QDir>
#include <QProcess>
#include <QTemporaryFile>
#include <QtGlobal>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

namespace mp = multipass;
namespace cmd = multipass::cmd;

mp::ReturnCodeVariant cmd::Config::run(mp::ArgParser* parser)
{
    auto parse_code = parse_args(parser);
    auto ret = parser->returnCodeFrom(parse_code);
    if (parse_code != ParseCode::Ok)
        return ret;

    YAML::Node root(YAML::NodeType::Map);
    for (const auto& key : MP_SETTINGS.keys())
    {
        if (key == mp::passphrase_key)
            continue;

        auto value = MP_SETTINGS.get(key).toStdString();
        const auto parts = key.split('.');

        YAML::Node node(root);
        for (int i = 0; i < parts.size() - 1; ++i)
        {
            YAML::Node child = node[parts[i].toStdString()];
            node.reset(child);
        }
        node[parts.last().toStdString()] = value;
    }

    YAML::Emitter emitter;
    emitter << YAML::Block << YAML::Indent(2) << root;
    std::string yaml_str = emitter.c_str();

    QTemporaryFile tmp{QDir::tempPath() + "/multipass-config-XXXXXX.yaml"};
    tmp.setAutoRemove(true);
    if (!tmp.open())
    {
        cerr << "Failed to create temporary file\n";
        return mp::ReturnCode::CommandFail;
    }
    tmp.write(yaml_str.c_str());
    tmp.flush();
    tmp.close();

    auto editor = qEnvironmentVariable("VISUAL");
    if (editor.isEmpty())
        editor = qEnvironmentVariable("EDITOR");
    if (editor.isEmpty())
    {
#if defined(Q_OS_LINUX)
        editor = "xdg-open";
#elif defined(Q_OS_MACOS)
        editor = "open";
#elif defined(Q_OS_WIN)
        editor = "cmd.exe /c start";
#else
        editor = "xdg-open";
#endif
    }

    bool detaches = (editor == "xdg-open" || editor == "open" || editor.startsWith("cmd.exe"));
    if (detaches)
    {
        QProcess::startDetached(editor, {tmp.fileName()});
        cout << "Press Enter when you have finished editing...\n";
        std::string dummy;
        std::getline(std::cin, dummy);
    }
    else
    {
        auto cmd_str = (editor + " " + tmp.fileName()).toStdString();
        std::system(cmd_str.c_str());
    }

    if (!tmp.open())
    {
        cerr << "Failed to read temporary file\n";
        return mp::ReturnCode::CommandFail;
    }
    tmp.seek(0);
    auto edited_content = tmp.readAll().toStdString();
    tmp.close();

    auto edited_node = YAML::Load(edited_content);

    std::vector<std::pair<QString, QString>> pairs;
    std::function<void(const YAML::Node&, const std::string&)> flatten =
        [&](const YAML::Node& node, const std::string& prefix) {
            if (node.IsMap())
            {
                for (auto it = node.begin(); it != node.end(); ++it)
                {
                    auto key = it->first.as<std::string>();
                    auto next_prefix = prefix.empty() ? key : prefix + "." + key;
                    flatten(it->second, next_prefix);
                }
            }
            else if (node.IsScalar())
            {
                pairs.emplace_back(QString::fromStdString(prefix),
                                   QString::fromStdString(node.as<std::string>()));
            }
        };
    flatten(edited_node, "");

    // Process all local.* keys before any client.* keys, so the single allowed reload
    // (on the last local.* set) fires at the end of all daemon-side changes.
    std::stable_partition(pairs.begin(), pairs.end(), [](const auto& kv) {
        return kv.first.startsWith("local.");
    });

    // Find the index of the last local.* key (if any).
    int last_local_idx = -1;
    for (int i = 0; i < static_cast<int>(pairs.size()); ++i)
    {
        if (pairs[i].first.startsWith("local."))
            last_local_idx = i;
    }

    for (int i = 0; i < static_cast<int>(pairs.size()); ++i)
    {
        const auto& [key, value] = pairs[i];
        const bool is_local = key.startsWith("local.");
        const bool allow_reload = !is_local || (i == last_local_idx);
        MP_SETTINGS.set(key, value, allow_reload);
    }

    return mp::ReturnCode::Ok;
}

std::string cmd::Config::name() const
{
    return "config";
}

QString cmd::Config::short_help() const
{
    return QStringLiteral("Open a settings editor");
}

QString cmd::Config::description() const
{
    return QStringLiteral("Open all Multipass settings in the system's default YAML editor.");
}

mp::ParseCode cmd::Config::parse_args(mp::ArgParser* parser)
{
    return parser->commandParse(this);
}
