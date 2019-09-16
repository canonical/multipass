/*
 * Copyright (C) 2019 Canonical, Ltd.
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

#include "iptables_config.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/process.h>
#include <shared/linux/process_factory.h>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
// QString constants for all if the different iptables calls
const QString iptables{QStringLiteral("iptables")};
const QString exclamation{QStringLiteral("!")};

//   Different tables to use
const QString filter{QStringLiteral("filter")};
const QString nat{QStringLiteral("nat")};
const QString mangle{QStringLiteral("mangle")};

//   Chain constants
const QString INPUT{QStringLiteral("INPUT")};
const QString OUTPUT{QStringLiteral("OUTPUT")};
const QString POSTROUTING{QStringLiteral("POSTROUTING")};
const QString FORWARD{QStringLiteral("FORWARD")};

//   option constants
const QString dash_C{QStringLiteral("-C")};
const QString dash_d{QStringLiteral("-d")};
const QString dash_D{QStringLiteral("-D")};
const QString dash_i{QStringLiteral("-i")};
const QString dash_I{QStringLiteral("-I")};
const QString dash_j{QStringLiteral("-j")};
const QString dash_m{QStringLiteral("-m")};
const QString dash_o{QStringLiteral("-o")};
const QString dash_p{QStringLiteral("-p")};
const QString dash_s{QStringLiteral("-s")};
const QString dash_S{QStringLiteral("-S")};
const QString dash_t{QStringLiteral("-t")};
const QString dash_w{QStringLiteral("-w")};

//   protocol constants
const QString udp{QStringLiteral("udp")};
const QString tcp{QStringLiteral("tcp")};

//   port options and constants
const QString dport{QStringLiteral("--dport")};
const QString sport{QStringLiteral("--sport")};
const QString to_ports{QStringLiteral("--to-ports")};
const QString port_53{QStringLiteral("53")};
const QString port_67{QStringLiteral("67")};
const QString port_68{QStringLiteral("68")};
const QString port_range{QStringLiteral("1024-65535")};

//   rule target constants
const QString ACCEPT{QStringLiteral("ACCEPT")};
const QString MASQUERADE{QStringLiteral("MASQUERADE")};
const QString REJECT{QStringLiteral("REJECT")};
const QString RETURN{QStringLiteral("RETURN")};

//   reject rule constants
const QString reject_with{QStringLiteral("--reject-with")};
const QString icmp_port_unreachable{QStringLiteral("icmp-port-unreachable")};

auto multipass_iptables_comment(const QString& bridge_name)
{
    return QString("generated for Multipass network %1").arg(bridge_name);
}

auto iptables_rule_exists(const QString& table, const QStringList& rule)
{
    auto process =
        mp::ProcessFactory::instance().create_process(iptables, QStringList() << dash_t << table << dash_C << rule);

    auto exit_state = process->execute();

    return exit_state.completed_successfully();
}

void insert_iptables_rule(const QString& table, const QStringList& rule)
{
    // Check if rule already exists in the table
    if (iptables_rule_exists(table, rule))
    {
        return;
    }

    auto process = mp::ProcessFactory::instance().create_process(iptables, QStringList() << dash_w << dash_t << table
                                                                                         << dash_I << rule);

    auto exit_state = process->execute();

    if (!exit_state.completed_successfully())
        throw std::runtime_error(
            fmt::format("Failed to set iptables rule for table {}: {}", table, process->read_all_standard_error()));
}

void delete_iptables_rule(const QString& table, const QStringList& rule)
{
    auto args = QStringList() << iptables << dash_w << dash_t << table << dash_D << rule;

    auto process = mp::ProcessFactory::instance().create_process(
        QStringLiteral("sh"), QStringList() << QStringLiteral("-c") << args.join(" "));

    auto exit_state = process->execute();

    if (!exit_state.completed_successfully())
        throw std::runtime_error(
            fmt::format("Failed to delete iptables rule for table {}: {}", table, process->read_all_standard_error()));
}

auto get_iptables_rules(const QString& table)
{
    auto process =
        mp::ProcessFactory::instance().create_process(iptables, QStringList() << dash_w << dash_t << table << dash_S);

    auto exit_state = process->execute();

    if (!exit_state.completed_successfully())
        throw std::runtime_error(
            fmt::format("Failed to get iptables list for table {}: {}", table, process->read_all_standard_error()));

    return process->read_all_standard_output();
}

void set_iptables_rules(const QString& bridge_name, const QString& cidr, const QString& comment)
{
    const QStringList comment_option{dash_m, QStringLiteral("comment"), QStringLiteral("--comment"), comment};

    // Setup basic iptables overrides for DHCP/DNS
    insert_iptables_rule(filter, QStringList() << INPUT << dash_i << bridge_name << dash_p << udp << dport << port_67
                                               << dash_j << ACCEPT << comment_option);

    insert_iptables_rule(filter, QStringList() << INPUT << dash_i << bridge_name << dash_p << udp << dport << port_53
                                               << dash_j << ACCEPT << comment_option);

    insert_iptables_rule(filter, QStringList() << INPUT << dash_i << bridge_name << dash_p << tcp << dport << port_53
                                               << dash_j << ACCEPT << comment_option);

    insert_iptables_rule(filter, QStringList() << OUTPUT << dash_o << bridge_name << dash_p << udp << sport << port_67
                                               << dash_j << ACCEPT << comment_option);

    insert_iptables_rule(filter, QStringList() << OUTPUT << dash_o << bridge_name << dash_p << udp << sport << port_53
                                               << dash_j << ACCEPT << comment_option);

    insert_iptables_rule(filter, QStringList() << OUTPUT << dash_o << bridge_name << dash_p << tcp << sport << port_53
                                               << dash_j << ACCEPT << comment_option);

    insert_iptables_rule(mangle, QStringList() << POSTROUTING << dash_o << bridge_name << dash_p << udp << dport
                                               << port_68 << dash_j << QStringLiteral("CHECKSUM")
                                               << QStringLiteral("--checksum-fill") << comment_option);

    // Do not masquerade to these reserved address blocks.
    insert_iptables_rule(nat, QStringList() << POSTROUTING << dash_s << cidr << dash_d << QStringLiteral("224.0.0.0/24")
                                            << dash_j << RETURN << comment_option);

    insert_iptables_rule(nat, QStringList()
                                  << POSTROUTING << dash_s << cidr << dash_d << QStringLiteral("255.255.255.255/32")
                                  << dash_j << RETURN << comment_option);

    // Masquerade all packets going from VMs to the LAN/Internet
    insert_iptables_rule(nat, QStringList() << POSTROUTING << dash_s << cidr << exclamation << dash_d << cidr << dash_p
                                            << tcp << dash_j << MASQUERADE << to_ports << port_range << comment_option);

    insert_iptables_rule(nat, QStringList() << POSTROUTING << dash_s << cidr << exclamation << dash_d << cidr << dash_p
                                            << udp << dash_j << MASQUERADE << to_ports << port_range << comment_option);

    insert_iptables_rule(nat, QStringList() << POSTROUTING << dash_s << cidr << exclamation << dash_d << cidr << dash_j
                                            << MASQUERADE << comment_option);

    // Allow established traffic to the private subnet
    insert_iptables_rule(filter, QStringList()
                                     << FORWARD << dash_d << cidr << dash_o << bridge_name << dash_m
                                     << QStringLiteral("conntrack") << QStringLiteral("--ctstate")
                                     << QStringLiteral("RELATED,ESTABLISHED") << dash_j << ACCEPT << comment_option);

    // Allow outbound traffic from the private subnet
    insert_iptables_rule(filter, QStringList() << FORWARD << dash_s << cidr << dash_i << bridge_name << dash_j << ACCEPT
                                               << comment_option);

    // Allow traffic between virtual machines
    insert_iptables_rule(filter, QStringList() << FORWARD << dash_i << bridge_name << dash_o << bridge_name << dash_j
                                               << ACCEPT << comment_option);

    // Reject everything else
    insert_iptables_rule(filter, QStringList() << FORWARD << dash_i << bridge_name << dash_j << REJECT << reject_with
                                               << icmp_port_unreachable << comment_option);

    insert_iptables_rule(filter, QStringList() << FORWARD << dash_o << bridge_name << dash_j << REJECT << reject_with
                                               << icmp_port_unreachable << comment_option);
}

void clear_iptables_rules_for(const QString& table, const QString& bridge_name, const QString& cidr,
                              const QString& comment)
{
    auto rules = QString::fromUtf8(get_iptables_rules(table));

    for (auto& rule : rules.split('\n'))
    {
        if (rule.contains(comment) || rule.contains(bridge_name) || rule.contains(cidr))
        {
            rule.remove(0, 3);
            delete_iptables_rule(table, QStringList() << rule);
        }
    }
}
} // namespace

mp::IPTablesConfig::IPTablesConfig(const QString& bridge_name, const std::string& subnet)
    : bridge_name{bridge_name},
      cidr{QString("%1.0/24").arg(QString::fromStdString(subnet))},
      comment{multipass_iptables_comment(bridge_name)}
{
    try
    {
        set_iptables_rules(bridge_name, cidr, comment);
    }
    catch (const std::exception& e)
    {
        mpl::log(mpl::Level::warning, "iptables", e.what());
        iptables_error = true;
        error_string = e.what();
    }
}

mp::IPTablesConfig::~IPTablesConfig()
{
    try
    {
        clear_all_iptables_rules();
    }
    catch (const std::exception& e)
    {
        mpl::log(mpl::Level::warning, "iptables", e.what());
    }
}

void mp::IPTablesConfig::verify_iptables_rules()
{
    if (iptables_error)
    {
        throw std::runtime_error(error_string);
    }
}

void mp::IPTablesConfig::clear_all_iptables_rules()
{
    clear_iptables_rules_for(filter, bridge_name, cidr, comment);
    clear_iptables_rules_for(nat, bridge_name, cidr, comment);
    clear_iptables_rules_for(mangle, bridge_name, cidr, comment);
}
