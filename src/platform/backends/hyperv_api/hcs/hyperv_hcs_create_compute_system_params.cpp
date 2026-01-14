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
template <typename T>
std::string value_or_null(const std::optional<T>& opt)
{
    if (opt)
    {
        return fmt::format("\"{}\"", *opt);
    }
    return "null";
}

void append_if(auto& target, bool condition, auto&& callable)
{
    if (condition)
    {
        if (target.empty())
        {
            // To emit an initial comma.
            target.push_back({});
        }
        target.emplace_back(typename std::decay_t<decltype(target)>::value_type{callable()});
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
                }}
                {5}
            }}
            {6}
        }}
    }}
    )json");

    std::vector<std::basic_string<Char>> optional_sections{}, optional_devices{};

    append_if(optional_sections,
              SchemaUtils::instance().get_os_supported_schema_version() == HcsSchemaVersion::v25,
              [] {
                  return string_literal<Char>(R"json(
                    "Services": {
                        "Shutdown": {},
                        "Heartbeat": {}
                    })json");
              });

    append_if(optional_sections,
              params.guest_state.guest_state_file_path.has_value() ||
                  params.guest_state.runtime_state_file_path.has_value(),
              [&vmgs = params.guest_state.guest_state_file_path,
               &vmrs = params.guest_state.runtime_state_file_path] {
                  return string_literal<Char>(R"json(
                        "GuestState": {{
                            "GuestStateFilePath": {0},
                            "RuntimeStateFilePath": {1}
                        }}
                    )json")
                      .format(value_or_null(vmgs), value_or_null(vmrs));
              });

    append_if(optional_sections,
              params.guest_state.save_state_file_path.has_value(),
              [&save_state = params.guest_state.save_state_file_path] {
                  return string_literal<Char>(R"json(
                        "RestoreState": {{
                            "SaveStateFilePath": "{0}"
                        }}
                    )json")
                      .format(*save_state);
              });

    // append_if(optional_devices, !params.shares.empty(), [&shares = params.shares] {
    //     // Had to extract it bc an empty shares array causes a vmwp.exe crash while saving the VM
    //     return string_literal<Char>(R"json(
    //             "Plan9": {{
    //                 "Shares": [
    //                     {0}
    //                 ]
    //             }})json")
    //         .format(fmt::join(shares, string_literal<Char>(",")));
    // });

    return json_template.format_to(ctx,
                                   params.memory_size_mb,
                                   params.processor_count,
                                   params.name,
                                   fmt::join(params.scsi_devices, string_literal<Char>(",")),
                                   fmt::join(params.network_adapters, string_literal<Char>(",")),
                                   fmt::join(optional_devices, string_literal<Char>(",")),
                                   fmt::join(optional_sections, string_literal<Char>(",")));
}

template auto fmt::formatter<CreateComputeSystemParameters, char>::format<fmt::format_context>(
    const CreateComputeSystemParameters&,
    fmt::format_context&) const -> fmt::format_context::iterator;

template auto fmt::formatter<CreateComputeSystemParameters, wchar_t>::format<fmt::wformat_context>(
    const CreateComputeSystemParameters&,
    fmt::wformat_context&) const -> fmt::wformat_context::iterator;
