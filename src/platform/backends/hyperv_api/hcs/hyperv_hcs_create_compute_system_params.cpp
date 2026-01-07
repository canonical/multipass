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
#include <hyperv_api/hcs/hyperv_hcs_schema_version.h>

#include <hyperv_api/hyperv_api_string_conversion.h>

using namespace multipass::hyperv;
using namespace multipass::hyperv::hcs;

namespace
{
void append_if_supported(auto& schema_version_dependent_vm_sections,
                         auto section,
                         HcsSchemaVersion target_schema_version)
{
    const auto schema_version = SchemaUtils::instance().get_os_supported_schema_version();
    if (schema_version >= target_schema_version)
    {
        if (schema_version_dependent_vm_sections.empty())
        {
            // To emit an initial comma.
            schema_version_dependent_vm_sections.push_back({});
        }
        schema_version_dependent_vm_sections.emplace_back(section);
    }
}
} // namespace

template <typename Char>
template <typename FormatContext>
auto fmt::formatter<CreateComputeSystemParameters, Char>::format(
    const CreateComputeSystemParameters& params,
    FormatContext& ctx) const -> FormatContext::iterator
{
    static constexpr auto json_template = string_literal<Char>(R"json(
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
                    "SizeInMB": {0}
                }},
                "Processor": {{
                    "Count": {1}
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
            }}
            {6}
        }}
    }}
    )json");
    static constexpr auto requested_services = string_literal<Char>(R"json(
            "Services": {
                "Shutdown": {},
                "Heartbeat": {}
            })json");

    std::vector<std::basic_string<Char>> schema_version_dependent_vm_sections{};
    append_if_supported(schema_version_dependent_vm_sections,
                        static_cast<std::basic_string_view<Char>>(requested_services),
                        HcsSchemaVersion::v25);

    return json_template.format_to(
        ctx,
        params.memory_size_mb,
        params.processor_count,
        params.name,
        fmt::join(params.scsi_devices, string_literal<Char>(",")),
        fmt::join(params.network_adapters, string_literal<Char>(",")),
        fmt::join(params.shares, string_literal<Char>(",")),
        fmt::join(schema_version_dependent_vm_sections, string_literal<Char>(",")));
}

template auto fmt::formatter<CreateComputeSystemParameters, char>::format<fmt::format_context>(
    const CreateComputeSystemParameters&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<CreateComputeSystemParameters, wchar_t>::format<fmt::wformat_context>(
    const CreateComputeSystemParameters&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
