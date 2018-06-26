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

#include <multipass/ip_address_pool.h>
#include <multipass/utils.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace mp = multipass;

namespace
{
constexpr auto ip_db_name = "multipassd-vm-ips.json";

std::unordered_map<std::string, mp::IPAddress> load_db(const QDir& data_dir)
{
    QFile db_file{data_dir.filePath(ip_db_name)};
    auto opened = db_file.open(QIODevice::ReadOnly);
    if (!opened)
        return {};

    QJsonParseError parse_error;
    auto doc = QJsonDocument::fromJson(db_file.readAll(), &parse_error);
    if (doc.isNull())
        return {};

    auto records = doc.object();
    if (records.isEmpty())
        return {};

    std::unordered_map<std::string, mp::IPAddress> reconstructed_records;
    for (auto it = records.constBegin(); it != records.constEnd(); ++it)
    {
        auto key = it.key().toStdString();
        auto record = it.value().toString();
        if (record.isEmpty())
            return {};

        reconstructed_records.emplace(key, mp::IPAddress(record.toStdString()));
    }
    return reconstructed_records;
}

template <typename T>
auto ips_in(T& ip_map)
{
    std::set<mp::IPAddress> ips_in_use;
    for (const auto& record : ip_map)
    {
        ips_in_use.emplace(record.second);
    }
    return ips_in_use;
}
}

mp::IPAddressPool::IPAddressPool(const Path& path, const IPAddress& start, const IPAddress& end)
    : start_ip{start},
      end_ip{end},
      data_dir{mp::utils::make_dir(path, "vm-ips")},
      ip_map{load_db(data_dir)},
      ips_in_use{ips_in(ip_map)}
{
    if (start > end)
        throw std::invalid_argument("the ip range is invalid");
}

mp::IPAddress mp::IPAddressPool::obtain_ip_for(const std::string& name)
{
    const auto it = ip_map.find(name);
    if (it == ip_map.end())
    {
        auto ip = obtain_free_ip();
        auto insertion = ips_in_use.emplace(ip);
        if (!insertion.second)
            throw std::runtime_error("Failed to record allocated ip");

        ip_map.emplace(name, ip);
        persist_ips();
        return ip;
    }
    return it->second;
}

mp::optional<mp::IPAddress> mp::IPAddressPool::check_ip_for(const std::string& name)
{
    const auto it = ip_map.find(name);
    if (it == ip_map.end())
    {
        return {};
    }

    return mp::optional<mp::IPAddress>{it->second};
}

mp::optional<mp::IPAddress> mp::IPAddressPool::first_free_ip()
{
    if (ips_in_use.empty())
        return {};

    return mp::optional<mp::IPAddress>{*ips_in_use.crbegin() + 1};
}

void mp::IPAddressPool::remove_ip_for(const std::string& name)
{
    const auto it = ip_map.find(name);
    if (it == ip_map.end())
        return;

    auto ip = it->second;
    ips_in_use.erase(ip);
    ip_map.erase(it);

    persist_ips();
}

mp::IPAddress mp::IPAddressPool::obtain_free_ip()
{
    if (ips_in_use.empty())
        return start_ip;

    // Naively take the highest ip, try to allocate the next one.
    // Naive because there may be holes in the set.
    const auto& prev_ip = ips_in_use.crbegin();
    auto new_ip = (*prev_ip) + 1;
    if (new_ip <= end_ip)
        return new_ip;

    auto max_ips_in_pool = end_ip.as_uint32() - start_ip.as_uint32() + 1u;
    if (ips_in_use.size() == max_ips_in_pool)
        throw std::runtime_error("Maximum number of ips reached");

    // There are still IPs available but there's holes in the set
    // Linearly walk through the range to check which IP is free.
    for (uint32_t i = 0; i < max_ips_in_pool; i++)
    {
        auto free_ip = start_ip + i;
        if (ips_in_use.find(free_ip) == ips_in_use.end())
            return free_ip;
    }

    // This should never happen
    throw std::logic_error("Could not find a free ip");
}

void mp::IPAddressPool::persist_ips()
{
    QJsonObject vm_ip_records_json;
    for (const auto& record : ip_map)
    {
        auto key = QString::fromStdString(record.first);
        vm_ip_records_json.insert(key, QString::fromStdString(record.second.as_string()));
    }

    QJsonDocument doc{vm_ip_records_json};
    auto raw_json = doc.toJson();
    QFile db_file{data_dir.filePath(ip_db_name)};
    db_file.open(QIODevice::ReadWrite | QIODevice::Truncate);
    db_file.write(raw_json);
}
