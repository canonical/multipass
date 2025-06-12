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
#include <hyperv_api/hcs/hyperv_hcs_create_compute_system_params.h>

#include <hyperv_api/hyperv_api_string_conversion.h>

using multipass::hyperv::maybe_widen;
using multipass::hyperv::hcs::CreateComputeSystemParameters;

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<CreateComputeSystemParameters, Char>::format(
    const CreateComputeSystemParameters& params,
    FormatContext& ctx) const -> typename FormatContext::iterator
{
    constexpr static auto json_template = MULTIPASS_UNIVERSAL_LITERAL(R"json(
    {{
        "SchemaVersion": {{
            "Major": 2,
            "Minor": 1
        }},
        "Owner": "Multipass",
        "ShouldTerminateOnLastHandleClosed": false,
        "VirtualMachine": {{
            "Chipset": {{
                "Uefi": {{
                    "BootThis": {{
                        "DevicePath": "Primary disk",
                        "DiskNumber": 0,
                        "DeviceType": "ScsiDrive"
                    }},
                    "Console": "ComPort1"
                }}
            }},
            "ComputeTopology": {{
                "Memory": {{
                    "Backing": "Virtual",
                    "SizeInMB": {1}
                }},
                "Processor": {{
                    "Count": {0}
                }}
            }},
            "Devices": {{
                "ComPorts": {{
                    "0": {{
                        "NamedPipe": "\\\\.\\pipe\\{2}"
                    }}
                }},
                "Scsi": {{
                    {3}
                }},
                "NetworkAdapters": {{
                    {4}
                }},
                "Plan9": {{
                    "Shares": [
                        {5}
                    ]
                }}
            }},
            "Services": {{
                "Shutdown": {{}},
                "Heartbeat": {{}}
            }}
        }}
    }}
    )json");

    constexpr static auto comma = MULTIPASS_UNIVERSAL_LITERAL(",");

    return format_to(ctx.out(),
                     json_template.as<Char>(),
                     params.processor_count,
                     params.memory_size_mb,
                     maybe_widen{params.name},
                     fmt::join(params.scsi_devices, comma.as<Char>()),
                     fmt::join(params.network_adapters, comma.as<Char>()),
                     fmt::join(params.shares, comma.as<Char>()));
}

template auto fmt::formatter<CreateComputeSystemParameters, char>::format<fmt::format_context>(
    const CreateComputeSystemParameters&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<CreateComputeSystemParameters, wchar_t>::format<fmt::wformat_context>(
    const CreateComputeSystemParameters&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
