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

#include "hyperv_hcs_api_wrapper.h"
#include "../hyperv_api_common.h"
#include "hyperv_api/hyperv_api_operation_result.h"
#include "hyperv_hcs_add_endpoint_params.h"
#include "hyperv_hcs_api_table.h"
#include "hyperv_hcs_create_compute_system_params.h"

#include <multipass/logging/log.h>

#include <ComputeDefs.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <fmt/xchar.h>

#include <chrono>
#include <memory>

namespace multipass::hyperv::hcs
{

namespace
{

using UniqueHcsSystem = std::unique_ptr<std::remove_pointer_t<HCS_SYSTEM>, decltype(HCSAPITable::CloseComputeSystem)>;
using UniqueHcsOperation = std::unique_ptr<std::remove_pointer_t<HCS_OPERATION>, decltype(HCSAPITable::CloseOperation)>;
using UniqueHlocalString = std::unique_ptr<wchar_t, decltype(HCSAPITable::LocalFree)>;

namespace mpl = logging;
using lvl = mpl::Level;

/**
 * Category for the log messages.
 */
constexpr auto kLogCategory = "HyperV-HCS-Wrapper";

/**
 * Default timeout value for HCS API operations
 */
constexpr auto kDefaultOperationTimeout = std::chrono::seconds{240};

// ---------------------------------------------------------

/**
 * Create a new HCS operation.
 *
 * @param api The HCS API table
 *
 * @return UniqueHcsOperation The new operation
 */
UniqueHcsOperation create_operation(const HCSAPITable& api)
{
    mpl::log(lvl::trace, kLogCategory, fmt::format("create_operation(...)"));
    return UniqueHcsOperation{api.CreateOperation(nullptr, nullptr), api.CloseOperation};
}

// ---------------------------------------------------------

/**
 * Wait until given operation completes, or the timeout has reached.
 *
 * @param api The HCS API table
 * @param op Operation to wait for
 * @param timeout Maximum amount of time to wait
 * @return Operation result
 */
OperationResult wait_for_operation_result(const HCSAPITable& api,
                                          UniqueHcsOperation op,
                                          std::chrono::milliseconds timeout = kDefaultOperationTimeout)
{
    mpl::log(lvl::trace,
             kLogCategory,
             fmt::format("wait_for_operation_result(...) > ({}), timeout: {} ms", fmt::ptr(op.get()), timeout.count()));

    wchar_t* result_msg_out{nullptr};
    const auto result = api.WaitForOperationResult(op.get(), timeout.count(), &result_msg_out);
    UniqueHlocalString result_msg{result_msg_out, api.LocalFree};

    if (result_msg)
    {
        // TODO: Convert from wstring to ascii and log this
        // mpl::log(lvl::trace,
        //     kLogCategory,
        //     fmt::format("wait_for_operation_result(...): ({}), result: {}, result_msg: {}", fmt::ptr(op.get()),
        //     result, result_msg));
        return OperationResult{result, result_msg.get()};
    }
    return OperationResult{result, L""};
}

// ---------------------------------------------------------

/**
 * Open an existing Host Compute System
 *
 * @param api The HCS API table
 * @param name Target Host Compute System's name
 *
 * @return auto UniqueHcsSystem non-nullptr on success.
 */
UniqueHcsSystem open_host_compute_system(const HCSAPITable& api, const std::string& name)
{
    mpl::log(lvl::trace, kLogCategory, fmt::format("open_host_compute_system(...) > name: ({})", name));

    // Windows API uses wide strings.
    const auto name_w = string_to_wstring(name);
    constexpr auto kRequestedAccessLevel = GENERIC_ALL;

    HCS_SYSTEM system{nullptr};
    const auto result = ResultCode{api.OpenComputeSystem(name_w.c_str(), kRequestedAccessLevel, &system)};

    if (!result)
    {
        mpl::log(lvl::error,
                 kLogCategory,
                 fmt::format("open_host_compute_system(...) > failed to open ({}), result code: ({})", name, result));
    }
    return UniqueHcsSystem{system, api.CloseComputeSystem};
}

// ---------------------------------------------------------

/**
 * Perform a Host Compute System API operation.
 *
 * Host Compute System operation functions have a common
 * signature, where `system` and `operation` are always
 * the first two arguments. This functions is a common
 * shorthand for invoking any of those.
 *
 * @param [in] api The API function table
 * @param [in] fn The API function pointer
 * @param [in] target_hcs_system_name HCS system to operate on
 * @param [in] args The arguments to the function
 *
 * @return HCNOperationResult Result of the performed operation
 */
template <typename FnType, typename... Args>
OperationResult perform_hcs_operation(const HCSAPITable& api,
                                      const FnType& fn,
                                      const std::string& target_hcs_system_name,
                                      Args&&... args)
{
    // Ensure that function to call is set.
    if (nullptr == fn)
    {
        assert(0);
        // E_POINTER means "invalid pointer", seems to be appropriate.
        return {E_POINTER, L"Operation function is unbound!"};
    }

    const auto system = open_host_compute_system(api, target_hcs_system_name);

    if (nullptr == system)
    {
        mpl::log(lvl::error,
                 kLogCategory,
                 fmt::format("perform_hcs_operation(...) > HcsOpenComputeSystem failed! {}", target_hcs_system_name));
        return OperationResult{E_POINTER, L"HcsOpenComputeSystem failed!"};
    }

    auto operation = create_operation(api);

    if (nullptr == operation)
    {
        mpl::log(lvl::error,
                 kLogCategory,
                 fmt::format("perform_hcs_operation(...) > HcsCreateOperation failed! {}", target_hcs_system_name));
        return OperationResult{E_POINTER, L"HcsCreateOperation failed!"};
    }

    const auto result = ResultCode{fn(system.get(), operation.get(), std::forward<Args>(args)...)};

    if (!result)
    {
        mpl::log(lvl::error,
                 kLogCategory,
                 fmt::format("perform_hcs_operation(...) > Operation failed! {} Result code {}",
                             target_hcs_system_name,
                             result));
        return OperationResult{result, L"HCS operation failed!"};
    }

    mpl::log(lvl::trace,
             kLogCategory,
             fmt::format("perform_hcs_operation(...) > fn: {}, result: {}",
                         fmt::ptr(fn.template target<void*>()),
                         static_cast<bool>(result)));

    return wait_for_operation_result(api, std::move(operation));
}

} // namespace

// ---------------------------------------------------------

HCSWrapper::HCSWrapper(const HCSAPITable& api_table) : api{api_table}
{
    mpl::log(lvl::trace, kLogCategory, fmt::format("HCSWrapper::HCSWrapper(...) > api_table: {}", api));
}

// ---------------------------------------------------------

OperationResult HCSWrapper::create_compute_system(const CreateComputeSystemParameters& params) const
{
    mpl::log(lvl::trace, kLogCategory, fmt::format("HCSWrapper::create_compute_system(...) > params: {} ", params));

    // Fill the SCSI devices template depending on
    // available drives.
    const auto scsi_devices = [&]() {
        constexpr auto scsi_device_template = LR"(
            "{0}": {{
                "Attachments": {{
                    "0": {{
                        "Type": "{1}",
                        "Path": "{2}",
                        "ReadOnly": {3}
                    }}
                }}
            }},
        )";
        std::wstring result = {};
        if (!params.cloudinit_iso_path.empty())
        {
            result += fmt::format(scsi_device_template,
                                  L"cloud-init iso file",
                                  L"Iso",
                                  string_to_wstring(params.cloudinit_iso_path),
                                  true);
        }

        if (!params.vhdx_path.empty())
        {
            result += fmt::format(scsi_device_template,
                                  L"Primary disk",
                                  L"VirtualDisk",
                                  string_to_wstring(params.vhdx_path),
                                  false);
        }
        return result;
    }();

    // Ideally, we should codegen from the schema
    // and use that.
    // https://raw.githubusercontent.com/MicrosoftDocs/Virtualization-Documentation/refs/heads/main/hyperv-samples/hcs-samples/JSON_files/HCS_Schema%5BWindows_10_SDK_version_1809%5D.json
    constexpr auto vm_settings_template = LR"(
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
                }}
            }}
        }}
    }})";

    // Render the template
    const auto vm_settings = fmt::format(vm_settings_template,
                                         params.processor_count,
                                         params.memory_size_mb,
                                         string_to_wstring(params.name),
                                         scsi_devices);
    HCS_SYSTEM system{nullptr};

    auto operation = create_operation(api);

    if (nullptr == operation)
    {
        return OperationResult{E_POINTER, L"HcsCreateOperation failed."};
    }

    const auto name_w = string_to_wstring(params.name);
    const auto result =
        ResultCode{api.CreateComputeSystem(name_w.c_str(), vm_settings.c_str(), operation.get(), nullptr, &system)};

    // Auto-release the system handle
    UniqueHcsSystem system_u{system, api.CloseComputeSystem};
    (void)system_u;

    if (!result)
    {
        return OperationResult{result, L"HcsCreateComputeSystem failed."};
    }

    return wait_for_operation_result(api, std::move(operation), std::chrono::seconds{240});
}

// ---------------------------------------------------------

OperationResult HCSWrapper::start_compute_system(const std::string& compute_system_name) const
{
    mpl::log(lvl::trace, kLogCategory, fmt::format("start_compute_system(...) > name: ({})", compute_system_name));
    return perform_hcs_operation(api, api.StartComputeSystem, compute_system_name, nullptr);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::shutdown_compute_system(const std::string& compute_system_name) const
{
    mpl::log(lvl::trace, kLogCategory, fmt::format("shutdown_compute_system(...) > name: ({})", compute_system_name));
    return perform_hcs_operation(api, api.ShutDownComputeSystem, compute_system_name, nullptr);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::terminate_compute_system(const std::string& compute_system_name) const
{
    mpl::log(lvl::trace, kLogCategory, fmt::format("terminate_compute_system(...) > name: ({})", compute_system_name));
    return perform_hcs_operation(api, api.TerminateComputeSystem, compute_system_name, nullptr);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::pause_compute_system(const std::string& compute_system_name) const
{
    mpl::log(lvl::trace, kLogCategory, fmt::format("pause_compute_system(...) > name: ({})", compute_system_name));
    static constexpr wchar_t c_pauseOption[] = LR"(
        {
            "SuspensionLevel": "Suspend",
            "HostedNotification": {
                "Reason": "Save"
            }
        })";
    return perform_hcs_operation(api, api.PauseComputeSystem, compute_system_name, c_pauseOption);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::resume_compute_system(const std::string& compute_system_name) const
{
    mpl::log(lvl::trace, kLogCategory, fmt::format("resume_compute_system(...) > name: ({})", compute_system_name));
    return perform_hcs_operation(api, api.ResumeComputeSystem, compute_system_name, nullptr);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::add_endpoint(const AddEndpointParameters& params) const
{
    mpl::log(lvl::trace, kLogCategory, fmt::format("add_endpoint(...) > params: {}", params));
    constexpr auto add_endpoint_settings_template = LR"(
        {{
            "ResourcePath": "VirtualMachine/Devices/NetworkAdapters/{{{0}}}",
            "RequestType": "Add",
            "Settings": {{
                "EndpointId": "{0}",
                "MacAddress": "{1}",
                "InstanceId": "{0}"
            }}
        }})";

    const auto settings = fmt::format(add_endpoint_settings_template,
                                      string_to_wstring(params.endpoint_guid),
                                      string_to_wstring(params.nic_mac_address));

    return perform_hcs_operation(api,
                                 api.ModifyComputeSystem,
                                 params.target_compute_system_name,
                                 settings.c_str(),
                                 nullptr);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::remove_endpoint(const std::string& compute_system_name,
                                            const std::string& endpoint_guid) const
{
    mpl::log(lvl::trace,
             kLogCategory,
             fmt::format("remove_endpoint(...) > name: ({}), endpoint_guid: ({})", compute_system_name, endpoint_guid));

    constexpr auto remove_endpoint_settings_template = LR"(
        {{
            "ResourcePath": "VirtualMachine/Devices/NetworkAdapters/{{{0}}}",
            "RequestType": "Remove"
        }})";

    const auto settings = fmt::format(remove_endpoint_settings_template, string_to_wstring(endpoint_guid));

    return perform_hcs_operation(api, api.ModifyComputeSystem, compute_system_name, settings.c_str(), nullptr);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::resize_memory(const std::string& compute_system_name, std::uint32_t new_size_mib) const
{
    // Machine must be booted up.
    mpl::log(lvl::trace,
             kLogCategory,
             fmt::format("resize_memory(...) > name: ({}), new_size_mb: ({})", compute_system_name, new_size_mib));
    // https://learn.microsoft.com/en-us/virtualization/api/hcs/reference/hcsmodifycomputesystem#remarks
    constexpr auto resize_memory_settings_template = LR"(
        {{
            "ResourcePath": "VirtualMachine/ComputeTopology/Memory/SizeInMB",
            "RequestType": "Update",
            "Settings": {0}
        }})";

    const auto settings = fmt::format(resize_memory_settings_template, new_size_mib);

    return perform_hcs_operation(api, api.ModifyComputeSystem, compute_system_name, settings.c_str(), nullptr);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::update_cpu_count(const std::string& compute_system_name, std::uint32_t new_vcpu_count) const
{
    return OperationResult{E_NOTIMPL, L"Not implemented yet!"};
}

// ---------------------------------------------------------

OperationResult HCSWrapper::get_compute_system_properties(const std::string& compute_system_name) const
{

    mpl::log(lvl::trace,
             kLogCategory,
             fmt::format("get_compute_system_properties(...) > name: ({})", compute_system_name));

    // https://learn.microsoft.com/en-us/virtualization/api/hcs/schemareference#System_PropertyType
    static constexpr wchar_t c_VmQuery[] = LR"(
        {
            "PropertyTypes":[]
        })";

    return perform_hcs_operation(api, api.GetComputeSystemProperties, compute_system_name, c_VmQuery);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::grant_vm_access(const std::string& compute_system_name,
                                            const std::filesystem::path& file_path) const
{
    mpl::log(
        lvl::trace,
        kLogCategory,
        fmt::format("grant_vm_access(...) > name: ({}), file_path: ({})", compute_system_name, file_path.string()));

    const auto path_as_wstring = file_path.wstring();
    const auto csname_as_wstring = string_to_wstring(compute_system_name);
    const auto result = api.GrantVmAccess(csname_as_wstring.c_str(), path_as_wstring.c_str());
    return {result, FAILED(result) ? L"GrantVmAccess failed!" : L""};
}

// ---------------------------------------------------------

OperationResult HCSWrapper::revoke_vm_access(const std::string& compute_system_name,
                                             const std::filesystem::path& file_path) const
{
    mpl::log(
        lvl::trace,
        kLogCategory,
        fmt::format("revoke_vm_access(...) > name: ({}), file_path: ({}) ", compute_system_name, file_path.string()));

    const auto path_as_wstring = file_path.wstring();
    const auto csname_as_wstring = string_to_wstring(compute_system_name);
    const auto result = api.RevokeVmAccess(csname_as_wstring.c_str(), path_as_wstring.c_str());
    return {result, FAILED(result) ? L"RevokeVmAccess failed!" : L""};
}

// ---------------------------------------------------------

OperationResult HCSWrapper::get_compute_system_state(const std::string& compute_system_name) const
{
    mpl::log(lvl::trace, kLogCategory, fmt::format("get_compute_system_state(...) > name: ({})", compute_system_name));

    const auto result = perform_hcs_operation(api, api.GetComputeSystemProperties, compute_system_name, nullptr);
    if (!result)
    {
        return {result.code, L"Unknown"};
    }

    QString qstr{QString::fromStdWString(result.status_msg)};
    auto doc = QJsonDocument::fromJson(qstr.toUtf8());
    auto obj = doc.object();
    if (obj.contains("State"))
    {
        auto state = obj["State"];
        auto state_str = state.toString();
        return {result.code, state_str.toStdWString()};
    }

    return {result.code, L"Unknown"};
}

} // namespace multipass::hyperv::hcs
