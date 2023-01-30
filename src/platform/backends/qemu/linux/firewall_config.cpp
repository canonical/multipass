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

#include "firewall_config.h"

#include <multipass/format.h>
#include <multipass/logging/log.h>
#include <multipass/process/process.h>
#include <multipass/top_catch_all.h>
#include <multipass/utils.h>
#include <shared/linux/process_factory.h>

#include <semver200.h>

#include <stdexcept>

#include <QRegularExpression>

namespace mp = multipass;
namespace mpl = multipass::logging;

namespace
{
constexpr auto category = "firewall";

// QString constants for all of the different firewall calls
const QString iptables{QStringLiteral("iptables-legacy")};
const QString nftables{QStringLiteral("iptables-nft")};
const QString negate{QStringLiteral("!")};

//   Different tables to use
const QString filter{QStringLiteral("filter")};
const QString nat{QStringLiteral("nat")};
const QString mangle{QStringLiteral("mangle")};
const QString raw{QStringLiteral("raw")};

//   List of all tables
const QStringList firewall_tables{filter, nat, mangle, raw};

//   Chain constants
const QString INPUT{QStringLiteral("INPUT")};
const QString OUTPUT{QStringLiteral("OUTPUT")};
const QString POSTROUTING{QStringLiteral("POSTROUTING")};
const QString FORWARD{QStringLiteral("FORWARD")};

//   option constants
const QString destination{QStringLiteral("--destination")};
const QString delete_rule{QStringLiteral("--delete")};
const QString in_interface{QStringLiteral("--in-interface")};
const QString append_rule{QStringLiteral("--append")};
const QString insert_rule{QStringLiteral("--insert")};
const QString jump{QStringLiteral("--jump")};
const QString match{QStringLiteral("--match")};
const QString out_interface{QStringLiteral("--out-interface")};
const QString protocol{QStringLiteral("--protocol")};
const QString source{QStringLiteral("--source")};
const QString list_rules{QStringLiteral("--list-rules")};
const QString dash_t{QStringLiteral("-t")}; // Use short option for specifying table to avoid var conflicts
const QString wait{QStringLiteral("--wait")};

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

class FirewallException : public std::runtime_error
{
public:
    FirewallException(const QString& issue, const QString& table, const QString& failure, const QString& output)
        : runtime_error{fmt::format("{}; Table: {}; Failure: {}; Output: {}", issue, table, failure, output)} {};
};

auto multipass_firewall_comment(const QString& bridge_name)
{
    return QString("generated for Multipass network %1").arg(bridge_name);
}

void add_firewall_rule(const QString& firewall, const QString& table, const QString& chain, const QStringList& rule,
                       bool append = false)
{
    auto process = MP_PROCFACTORY.create_process(
        firewall, QStringList() << wait << dash_t << table << (append ? append_rule : insert_rule) << chain << rule);

    auto exit_state = process->execute();

    if (!exit_state.completed_successfully())
        throw FirewallException("Failed to set firewall rule", table, exit_state.failure_message(),
                                process->read_all_standard_error());
}

void delete_firewall_rule(const QString& firewall, const QString& table, const QStringList& chain_and_rule)
{
    auto args = QStringList() << firewall << wait << dash_t << table << delete_rule << chain_and_rule;

    auto process =
        MP_PROCFACTORY.create_process(QStringLiteral("sh"), QStringList() << QStringLiteral("-c") << args.join(" "));

    auto exit_state = process->execute();

    if (!exit_state.completed_successfully())
        throw FirewallException("Failed to delete firewall rule", table, exit_state.failure_message(),
                                process->read_all_standard_error());
}

auto get_firewall_rules(const QString& firewall, const QString& table)
{
    // TODO: Parse out stderr so as not to log noisy warnings from iptables-nft when legacy iptables are in use
    auto process = MP_PROCFACTORY.create_process(firewall, QStringList() << wait << dash_t << table << list_rules);

    auto exit_state = process->execute();

    if (!exit_state.completed_successfully())
        throw FirewallException("Failed to get firewall list", table, exit_state.failure_message(),
                                process->read_all_standard_error());

    return process->read_all_standard_output();
}

void set_firewall_rules(const QString& firewall, const QString& bridge_name, const QString& cidr,
                        const QString& comment)
{
    const QStringList comment_option{match, QStringLiteral("comment"), QStringLiteral("--comment"), comment};

    // Setup basic firewall overrides for DHCP/DNS
    add_firewall_rule(firewall, filter, INPUT,
                      QStringList() << in_interface << bridge_name << protocol << udp << dport << port_67 << jump
                                    << ACCEPT << comment_option);

    add_firewall_rule(firewall, filter, INPUT,
                      QStringList() << in_interface << bridge_name << protocol << udp << dport << port_53 << jump
                                    << ACCEPT << comment_option);

    add_firewall_rule(firewall, filter, INPUT,
                      QStringList() << in_interface << bridge_name << protocol << tcp << dport << port_53 << jump
                                    << ACCEPT << comment_option);

    add_firewall_rule(firewall, filter, OUTPUT,
                      QStringList() << out_interface << bridge_name << protocol << udp << sport << port_67 << jump
                                    << ACCEPT << comment_option);

    add_firewall_rule(firewall, filter, OUTPUT,
                      QStringList() << out_interface << bridge_name << protocol << udp << sport << port_53 << jump
                                    << ACCEPT << comment_option);

    add_firewall_rule(firewall, filter, OUTPUT,
                      QStringList() << out_interface << bridge_name << protocol << tcp << sport << port_53 << jump
                                    << ACCEPT << comment_option);

    add_firewall_rule(firewall, mangle, POSTROUTING,
                      QStringList() << out_interface << bridge_name << protocol << udp << dport << port_68 << jump
                                    << QStringLiteral("CHECKSUM") << QStringLiteral("--checksum-fill")
                                    << comment_option);

    // Do not masquerade to these reserved address blocks.
    add_firewall_rule(firewall, nat, POSTROUTING,
                      QStringList() << source << cidr << destination << QStringLiteral("224.0.0.0/24") << jump << RETURN
                                    << comment_option);

    add_firewall_rule(firewall, nat, POSTROUTING,
                      QStringList() << source << cidr << destination << QStringLiteral("255.255.255.255/32") << jump
                                    << RETURN << comment_option);

    // Masquerade all packets going from VMs to the LAN/Internet
    add_firewall_rule(firewall, nat, POSTROUTING,
                      QStringList() << source << cidr << negate << destination << cidr << protocol << tcp << jump
                                    << MASQUERADE << to_ports << port_range << comment_option);

    add_firewall_rule(firewall, nat, POSTROUTING,
                      QStringList() << source << cidr << negate << destination << cidr << protocol << udp << jump
                                    << MASQUERADE << to_ports << port_range << comment_option);

    add_firewall_rule(firewall, nat, POSTROUTING,
                      QStringList() << source << cidr << negate << destination << cidr << jump << MASQUERADE
                                    << comment_option);

    // Allow established traffic to the private subnet
    add_firewall_rule(firewall, filter, FORWARD,
                      QStringList() << destination << cidr << out_interface << bridge_name << match
                                    << QStringLiteral("conntrack") << QStringLiteral("--ctstate")
                                    << QStringLiteral("RELATED,ESTABLISHED") << jump << ACCEPT << comment_option);

    // Allow outbound traffic from the private subnet
    add_firewall_rule(firewall, filter, FORWARD,
                      QStringList() << source << cidr << in_interface << bridge_name << jump << ACCEPT
                                    << comment_option);

    // Allow traffic between virtual machines
    add_firewall_rule(firewall, filter, FORWARD,
                      QStringList() << in_interface << bridge_name << out_interface << bridge_name << jump << ACCEPT
                                    << comment_option);

    // Reject everything else
    add_firewall_rule(firewall, filter, FORWARD,
                      QStringList() << in_interface << bridge_name << jump << REJECT << reject_with
                                    << icmp_port_unreachable << comment_option,
                      /*append=*/true);

    add_firewall_rule(firewall, filter, FORWARD,
                      QStringList() << out_interface << bridge_name << jump << REJECT << reject_with
                                    << icmp_port_unreachable << comment_option,
                      /*append=*/true);
}

void clear_firewall_rules_for(const QString& firewall, const QString& table, const QString& bridge_name,
                              const QString& cidr, const QString& comment)
{
    auto rules = QString::fromUtf8(get_firewall_rules(firewall, table));

    for (auto& rule : rules.split('\n'))
    {
        if (rule.contains(comment) || rule.contains(bridge_name) || rule.contains(cidr))
        {
            // Remove the policy type since delete doesn't use that
            rule.remove(0, 3);

            try
            {
                // Pass the chain and rule wholesale since we capture the whole line
                delete_firewall_rule(firewall, table, QStringList() << rule);
            }
            catch (const FirewallException& e)
            {
                mpl::log(mpl::Level::error, category,
                         fmt::format("Error deleting firewall rule '{}': {}", rule, e.what()));
            }
        }
    }
}

bool is_firewall_in_use(const QString& firewall)
{

    return std::any_of(firewall_tables.cbegin(), firewall_tables.cend(), [&firewall](const QString& table) {
        QRegularExpression re{"^-[ARIN]"};
        auto rule_lines = get_firewall_rules(firewall, table).split('\n');

        return std::any_of(rule_lines.cbegin(), rule_lines.cend(),
                           [&re](const QString& line) { return re.match(line).hasMatch(); });
    });
}

// We require a >= 5.2 kernel to avoid weird conflicts with xtables and support for inet table NAT rules.
// Taken from LXD :)
bool kernel_supports_nftables()
{
    const auto kernel_version{MP_UTILS.get_kernel_version()};
    try
    {
        auto kernel_supported{version::Semver200_version(kernel_version) >= version::Semver200_version("5.2.0")};

        if (!kernel_supported)
        {
            mpl::log(mpl::Level::warning, category, "Kernel version does not meet minimum requirement of 5.2");
        }

        return kernel_supported;
    }
    catch (version::Parse_error&)
    {
        mpl::log(mpl::Level::warning, category, fmt::format("Cannot parse kernel version \'{}\'", kernel_version));
        return false;
    }
}

QString detect_firewall()
{
    QString firewall_exec;
    try
    {
        firewall_exec = kernel_supports_nftables() && (is_firewall_in_use(nftables) || !is_firewall_in_use(iptables))
                            ? nftables
                            : iptables;
    }
    catch (const FirewallException& e)
    {
        firewall_exec = iptables;
        mpl::log(mpl::Level::warning, category, e.what());
    }

    mpl::log(mpl::Level::info, category, fmt::format("Using {} for firewall rules.", firewall_exec));

    return firewall_exec;
}
} // namespace

mp::FirewallConfig::FirewallConfig(const QString& bridge_name, const std::string& subnet)
    : firewall{detect_firewall()},
      bridge_name{bridge_name},
      cidr{QString("%1.0/24").arg(QString::fromStdString(subnet))},
      comment{multipass_firewall_comment(bridge_name)}
{
    try
    {
        clear_all_firewall_rules();
        set_firewall_rules(firewall, bridge_name, cidr, comment);
    }
    catch (const FirewallException& e)
    {
        mpl::log(mpl::Level::warning, category, e.what());
        firewall_error = true;
        error_string = e.what();
    }
}

mp::FirewallConfig::~FirewallConfig()
{
    if (!firewall.isEmpty())
    {
        mp::top_catch_all(category, [this] { clear_all_firewall_rules(); });
    }
}

void mp::FirewallConfig::verify_firewall_rules()
{
    if (firewall_error)
    {
        throw std::runtime_error(error_string);
    }
}

void mp::FirewallConfig::clear_all_firewall_rules()
{
    for (const auto& table : firewall_tables)
    {
        clear_firewall_rules_for(firewall, table, bridge_name, cidr, comment);
    }
}

mp::FirewallConfig::UPtr mp::FirewallConfigFactory::make_firewall_config(const QString& bridge_name,
                                                                         const std::string& subnet) const
{
    return std::make_unique<mp::FirewallConfig>(bridge_name, subnet);
}
