/*
 * Copyright (C) 2018 Canonical, Ltd.
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

#ifndef MULTIPASS_METRICS_DATA_H
#define MULTIPASS_METRICS_DATA_H

#include <string>
#include <vector>

namespace multipass
{
struct InstanceMetricsData
{
    int mem_size; // In MB's
    int num_cpus;
    int disk_size; // In GB's
    int uptime;    // In seconds
    int lifetime;  // In seconds
    int num_mounts;
    std::string alias;
    std::string image_file_name;
};

struct MetricsData
{
    std::string OS;
    std::string OS_version;
    std::string cpu_model;
    std::vector<int> disk_sizes; // In GB's
    int host_mem_size;           // In MB's
    int running_instances;
    int total_launches;
    std::vector<InstanceMetricsData> instances;
};
} // namespace multipass
#endif // MULTIPASS_METRICS_DATA_H
