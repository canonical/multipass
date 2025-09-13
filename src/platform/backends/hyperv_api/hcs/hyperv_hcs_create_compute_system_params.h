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

#pragma once

#include <hyperv_api/hcs/hyperv_hcs_network_adapter.h>
#include <hyperv_api/hcs/hyperv_hcs_plan9_share_params.h>
#include <hyperv_api/hcs/hyperv_hcs_scsi_device.h>

#include <fmt/format.h>
#include <fmt/std.h>
#include <string>

namespace multipass::hyperv::hcs
{

/**
 * Parameters for creating a network endpoint.
 */
struct CreateComputeSystemParameters
{
    /**
     * Unique name for the compute system
     */
    std::string name{};

    /**
     * Memory size, in megabytes
     */
    std::uint32_t memory_size_mb{};

    /**
     * vCPU count
     */
    std::uint32_t processor_count{};

    /**
     * List of SCSI devices that are attached on boot
     */
    std::vector<HcsScsiDevice> scsi_devices;

    /**
     * List of endpoints that'll be added to the compute system
     * by default at creation time.
     */
    std::vector<HcsNetworkAdapter> network_adapters{};

    /**
     * List of Plan9 shares that'll be added to the compute system
     * by default at creation time.
     */
    std::vector<HcsAddPlan9ShareParameters> shares{};
};

} // namespace multipass::hyperv::hcs

/**
 * Formatter type specialization for CreateComputeSystemParameters
 */
template <typename Char>
struct fmt::formatter<multipass::hyperv::hcs::CreateComputeSystemParameters, Char>
    : formatter<basic_string_view<Char>, Char>
{
    template <typename FormatContext>
    auto format(const multipass::hyperv::hcs::CreateComputeSystemParameters& policy,
                FormatContext& ctx) const -> typename FormatContext::iterator;
};
