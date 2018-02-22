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
 */

#include <multipass/utils.h>

#include <fmt/format.h>

#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include <regex>

namespace mp = multipass;

namespace
{
auto quote_for(const std::string& arg, mp::utils::QuoteType quote_type)
{
    if (quote_type == mp::utils::QuoteType::no_quotes)
        return "";
    return arg.find('\'') == std::string::npos ? "'" : "\"";
}
}

QDir mp::utils::base_dir(const QString& path)
{
    QFileInfo info{path};
    return info.absoluteDir();
}

bool mp::utils::valid_memory_value(const QString& mem_string)
{
    QRegExp matcher("\\d+((K|M|G)(B){0,1}){0,1}$");

    return matcher.exactMatch(mem_string);
}

bool mp::utils::valid_hostname(const QString& name_string)
{
    QRegExp matcher("^([a-zA-Z]|[a-zA-Z][a-zA-Z0-9\\-]*[a-zA-Z0-9])");

    return matcher.exactMatch(name_string);
}

bool mp::utils::invalid_target_path(const QString& target_path)
{
    QString sanitized_path{QDir::cleanPath(target_path)};
    QRegExp matcher("/+|/+(dev|proc|sys)(/.*)*|/+home(/*)(/ubuntu/*)*");

    return matcher.exactMatch(sanitized_path);
}

std::string mp::utils::to_cmd(const std::vector<std::string>& args, QuoteType quote_type)
{
    fmt::MemoryWriter out;
    for (auto const& arg : args)
    {
        out.write("{0}{1}{0} ", quote_for(arg, quote_type), arg);
    }

    // Remove the last space inserted
    auto cmd = out.str();
    cmd.pop_back();
    return cmd;
}

bool mp::utils::run_cmd_for_status(const QString& cmd, const QStringList& args)
{
    QProcess proc;
    proc.setProgram(cmd);
    proc.setArguments(args);

    proc.start();
    proc.waitForFinished();

    return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
}

std::string mp::utils::run_cmd_for_output(const QString& cmd, const QStringList& args)
{
    QProcess proc;
    proc.setProgram(cmd);
    proc.setArguments(args);

    proc.start();
    proc.waitForFinished();

    return proc.readAllStandardOutput().trimmed().toStdString();
}

std::string& mp::utils::trim_end(std::string& s)
{
    auto rev_it = std::find_if(s.rbegin(), s.rend(), [](char ch) { return !std::isspace(ch); });
    s.erase(rev_it.base(), s.end());
    return s;
}

std::string mp::utils::escape_char(const std::string& in, char c)
{
    return std::regex_replace(in, std::regex({c}), fmt::format("\\{}", c));
}
