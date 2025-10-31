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
#include <hyperv_api/hcs/hyperv_hcs_scsi_device.h>

#include <hyperv_api/hyperv_api_string_conversion.h>

#include <fmt/std.h>

using multipass::hyperv::maybe_widen;
using multipass::hyperv::hcs::HcsScsiDevice;
using multipass::hyperv::hcs::HcsScsiDeviceType;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<HcsScsiDevice, Char>::format(const HcsScsiDevice& scsi_device,
                                                 FormatContext& ctx) const ->
    typename FormatContext::iterator
{
    constexpr static auto scsi_device_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
        "{0}": {{
            "Attachments": {{
                "0": {{
                    "Type": "{1}",
                    "Path": "{2}",
                    "ReadOnly": {3}
                }}
            }}
        }}
    )json");

    return fmt::format_to(ctx.out(),
                          scsi_device_template.as<Char>(),
                          maybe_widen{scsi_device.name},
                          maybe_widen{scsi_device.type},
                          scsi_device.path,
                          scsi_device.read_only);
}

template auto fmt::formatter<HcsScsiDevice, char>::format<fmt::format_context>(
    const HcsScsiDevice&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<HcsScsiDevice, wchar_t>::format<fmt::wformat_context>(
    const HcsScsiDevice&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
