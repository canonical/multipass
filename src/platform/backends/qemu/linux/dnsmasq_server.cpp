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

#include "dnsmasq_server.h"
#include "dnsmasq_process_spec.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/process/process.h>
#include <multipass/utils.h>
#include <shared/linux/process_factory.h>

#include <QDir>

#include <algorithm>
#include <chrono>
#include <fstream>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto immediate_wait = 100; // period to wait for immediate dnsmasq failures, in ms
constexpr auto log_category = "dnsmasq";

auto make_dnsmasq_process(const mp::Path& data_dir,
                          const mp::BridgeSubnetList& subnets,
                          const QString& conf_file_path)
{
    auto process_spec = std::make_unique<mp::DNSMasqProcessSpec>(data_dir, subnets, conf_file_path);
    return MP_PROCFACTORY.create_process(std::move(process_spec));
}

bool ip_in_served_subnets(const mp::BridgeSubnetList& subnets, const mp::IPAddress& ip)
{
    return std::ranges::any_of(subnets, [&ip](const auto& bridge_subnet) {
        return bridge_subnet.second.contains(ip);
    });
}
} // namespace

mp::DNSMasqServer::DNSMasqServer(const Path& data_dir, const BridgeSubnetList& subnets)
    : data_dir{data_dir},
      subnets{subnets},
      conf_file{QDir(data_dir).absoluteFilePath("dnsmasq-XXXXXX.conf")}
{
    if (!conf_file.open())
        throw std::runtime_error("unable to create temporary dnsmasq conf file");
    conf_file.close();

    QFile dnsmasq_hosts(QDir(data_dir).filePath("dnsmasq.hosts"));
    if (!dnsmasq_hosts.exists())
    {
        if (!dnsmasq_hosts.open(QIODevice::WriteOnly))
            throw std::runtime_error(
                fmt::format("unable to create file {}", dnsmasq_hosts.filesystemFileName()));
    }

    dnsmasq_cmd = make_dnsmasq_process(data_dir, subnets, conf_file.fileName());
    start_dnsmasq();
}

mp::DNSMasqServer::~DNSMasqServer()
{
    using namespace std::chrono_literals;

    constexpr static auto terminate_timeout = 10000ms;
    constexpr static auto kill_timeout = 5000ms;

    if (dnsmasq_cmd && dnsmasq_cmd->running())
    {
        QObject::disconnect(finish_connection);

        mpl::debug(log_category, "terminating");
        dnsmasq_cmd->terminate();

        if (!dnsmasq_cmd->wait_for_finished(terminate_timeout))
        {
            mpl::info(log_category, "failed to terminate nicely, killing");
            dnsmasq_cmd->kill();

            if (!dnsmasq_cmd->wait_for_finished(kill_timeout))
            {
                mpl::warn(log_category, "failed to kill (timed out)");
            }
        }
    }
}

std::optional<mp::IPAddress> mp::DNSMasqServer::get_ip_for(const std::string& hw_addr)
{
    // DNSMasq leases entries consist of:
    // <lease expiration> <mac addr> <ipv4> <name> * * *
    const auto path = QDir(data_dir).filePath("dnsmasq.leases").toStdString();
    const std::string delimiter{" "};
    const int hw_addr_idx{1};
    const int ipv4_idx{2};
    std::ifstream leases_file{path};
    std::string line;
    while (getline(leases_file, line))
    {
        const auto fields = mp::utils::split(line, delimiter);
        if (fields.size() > 2 && fields[hw_addr_idx] == hw_addr)
        {
            try
            {
                const IPAddress ip{fields[ipv4_idx]};
                // Ignore leases outside the subnets we currently serve. A version upgrade that
                // changes the bridge layout can leave an old lease pointing at an unreachable
                // address; skip it so we resolve the current, reachable one instead.
                if (ip_in_served_subnets(subnets, ip))
                    return ip;
                // TODO: Also check if the lease is still valid
                // TODO: Aggregate all leases for the MAC before filtering
            }
            catch (const std::invalid_argument& ex) // unparseable address -> skip this line
            {
                mpl::debug(log_category,
                           "Could not parse `{}` as IPv4 address: {}, ignoring lease line",
                           fields[ipv4_idx],
                           ex.what());
            }
        }
    }
    return std::nullopt;
}

void mp::DNSMasqServer::release_mac(const std::string& hw_addr, const QString& bridge_name)
{
    auto ip = get_ip_for(hw_addr);
    if (!ip)
    {
        mpl::warn(log_category, "attempting to release non-existent addr: {}", hw_addr);
        return;
    }

    QProcess dhcp_release;
    QObject::connect(&dhcp_release,
                     &QProcess::errorOccurred,
                     [&ip, &hw_addr](QProcess::ProcessError error) {
                         mpl::warn(log_category,
                                   "failed to release ip addr {} with mac {}: {}",
                                   ip.value().as_string(),
                                   hw_addr,
                                   utils::qenum_to_string(error));
                     });

    auto log_exit_status = [&ip, &hw_addr](int exit_code, QProcess::ExitStatus exit_status) {
        if (exit_code == 0 && exit_status == QProcess::NormalExit)
            return;

        mpl::warn(log_category,
                  "failed to release ip addr {} with mac {}, exit_code: {}",
                  ip.value().as_string(),
                  hw_addr,
                  exit_code);
    };
    QObject::connect(
        &dhcp_release,
        static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
        log_exit_status);

    dhcp_release.start("dhcp_release",
                       QStringList()
                           << bridge_name << QString::fromStdString(ip.value().as_string())
                           << QString::fromStdString(hw_addr));

    dhcp_release.waitForFinished();
}

void mp::DNSMasqServer::check_dnsmasq_running()
{
    if (!dnsmasq_cmd->running())
    {
        mpl::warn(log_category, "Not running");
        start_dnsmasq();
    }
}

namespace
{
std::string dnsmasq_failure_msg(std::string err_base, const mp::ProcessState& state)
{
    if (auto err_detail = state.failure_message(); !err_detail.isEmpty())
        err_base += fmt::format(": {}", err_detail);

    return err_base;
}

std::string dnsmasq_failure_msg(const mp::ProcessState& state)
{
    auto err_msg = dnsmasq_failure_msg("dnsmasq died", state);
    if (state.exit_code == 2)
        err_msg += ". Ensure nothing is using port 53.";

    return err_msg;
}
} // namespace

void mp::DNSMasqServer::start_dnsmasq()
{
    mpl::debug(log_category, "Starting dnsmasq");

    finish_connection =
        QObject::connect(dnsmasq_cmd.get(), &mp::Process::finished, [](const ProcessState& state) {
            mpl::log_message(mpl::Level::error, "dnsmasq", dnsmasq_failure_msg(state));
        });

    dnsmasq_cmd->start();
    if (!dnsmasq_cmd->wait_for_started())
    {
        auto err_msg =
            dnsmasq_failure_msg("Multipass dnsmasq failed to start", dnsmasq_cmd->process_state());

        dnsmasq_cmd->kill();
        throw std::runtime_error(err_msg);
    }

    if (dnsmasq_cmd->wait_for_finished(
            immediate_wait)) // detect immediate failures (in the first few milliseconds)
        throw std::runtime_error{dnsmasq_failure_msg(dnsmasq_cmd->process_state())};
}

mp::DNSMasqServer::UPtr mp::DNSMasqServerFactory::make_dnsmasq_server(
    const mp::Path& network_dir,
    const BridgeSubnetList& subnets) const
{
    return std::make_unique<mp::DNSMasqServer>(network_dir, subnets);
}
