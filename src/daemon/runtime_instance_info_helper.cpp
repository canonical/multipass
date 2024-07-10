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

#include "runtime_instance_info_helper.h"

#include <multipass/format.h>
#include <multipass/rpc/multipass.grpc.pb.h>
#include <multipass/utils.h>
#include <multipass/virtual_machine.h>

#include <yaml-cpp/yaml.h>

#include <array>

namespace mp = multipass;

namespace
{

struct Keys
{
public:
    static constexpr auto loadavg_key = "loadavg";
    static constexpr auto mem_usage_key = "mem_usage";
    static constexpr auto mem_total_key = "mem_total";
    static constexpr auto disk_usage_key = "disk_usage";
    static constexpr auto disk_total_key = "disk_total";
    static constexpr auto cpus_key = "cpus";
    static constexpr auto cpu_times_key = "cpu_times";
    static constexpr auto uptime_key = "uptime";
    static constexpr auto current_release_key = "current_release";
};

struct Cmds
{
private:
    static constexpr auto key_val_cmd = R"-(echo {}: "$(eval "{}")")-";
    static constexpr std::array key_cmds_pairs{
        std::pair{Keys::loadavg_key, "cat /proc/loadavg | cut -d ' ' -f1-3"},
        std::pair{Keys::mem_usage_key, R"(free -b | grep 'Mem:' | awk '{printf \$3}')"},
        std::pair{Keys::mem_total_key, R"(free -b | grep 'Mem:' | awk '{printf \$2}')"},
        std::pair{Keys::disk_usage_key, "df -t ext4 -t vfat --total -B1 --output=used | tail -n 1"},
        std::pair{Keys::disk_total_key, "df -t ext4 -t vfat --total -B1 --output=size | tail -n 1"},
        std::pair{Keys::cpus_key, "nproc"},
        std::pair{Keys::cpu_times_key, "head -n1 /proc/stat"},
        std::pair{Keys::uptime_key, "uptime -p | tail -c+4"},
        std::pair{Keys::current_release_key, R"(cat /etc/os-release | grep 'PRETTY_NAME' | cut -d \\\" -f2)"}};

    inline static const std::array cmds = [] {
        constexpr auto n = key_cmds_pairs.size();
        std::array<std::string, key_cmds_pairs.size()> ret;
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto [key, cmd] = key_cmds_pairs[i];
            ret[i] = fmt::format(key_val_cmd, key, cmd);
        }

        return ret;
    }();

public:
    inline static const std::string sequential_composite_cmd = fmt::to_string(fmt::join(cmds, "; "));
    inline static const std::string parallel_composite_cmd = fmt::format("{} & wait", fmt::join(cmds, "& "));
};
} // namespace

void mp::RuntimeInstanceInfoHelper::populate_runtime_info(mp::VirtualMachine& vm,
                                                          mp::DetailedInfoItem* info,
                                                          mp::InstanceDetails* instance_info,
                                                          const std::string& original_release,
                                                          bool parallelize)
{
    const auto& cmd = parallelize ? Cmds::parallel_composite_cmd : Cmds::sequential_composite_cmd;
    auto results = YAML::Load(vm.ssh_exec(cmd, /* whisper = */ true));

    instance_info->set_load(results[Keys::loadavg_key].as<std::string>());
    instance_info->set_memory_usage(results[Keys::mem_usage_key].as<std::string>());
    info->set_memory_total(results[Keys::mem_total_key].as<std::string>());
    instance_info->set_disk_usage(results[Keys::disk_usage_key].as<std::string>());
    info->set_disk_total(results[Keys::disk_total_key].as<std::string>());
    info->set_cpu_count(results[Keys::cpus_key].as<std::string>());
    instance_info->set_cpu_times(results[Keys::cpu_times_key].as<std::string>());
    // In some older versions of Ubuntu, "uptime -p" prints only "up" right after startup. In those cases,
    // results[Keys::uptime_key] is null.
    instance_info->set_uptime(results[Keys::uptime_key].as<std::string>(/* fallback = */ "0 minutes"));

    auto current_release = results[Keys::current_release_key].as<std::string>();
    instance_info->set_current_release(!current_release.empty() ? current_release : original_release);

    std::string management_ip = vm.management_ipv4();
    auto all_ipv4 = vm.get_all_ipv4();

    if (MP_UTILS.is_ipv4_valid(management_ip))
        instance_info->add_ipv4(management_ip);
    else if (all_ipv4.empty())
        instance_info->add_ipv4("N/A");

    for (const auto& extra_ipv4 : all_ipv4)
        if (extra_ipv4 != management_ip)
            instance_info->add_ipv4(extra_ipv4);
}
