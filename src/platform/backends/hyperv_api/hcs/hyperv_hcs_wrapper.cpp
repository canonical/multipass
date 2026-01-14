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
#include <hyperv_api/hcs/hyperv_hcs_wrapper.h>

#include <hyperv_api/hcs/hyperv_hcs_api.h>
#include <hyperv_api/hcs/hyperv_hcs_create_compute_system_params.h>
#include <hyperv_api/hyperv_api_operation_result.h>
#include <hyperv_api/hyperv_api_string_conversion.h>

#include <multipass/logging/log.h>

#include <shared/windows/wchar_conversion.h>

#include <ComputeDefs.h>

#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <memory>

#include <boost/json.hpp>
#include <fmt/xchar.h>
#include <ztd/out_ptr.hpp>

using ztd::out_ptr::out_ptr;

namespace multipass::hyperv::hcs
{

namespace
{

inline const HCSAPI& API()
{
    return HCSAPI::instance();
}

struct HcsSystemCloser
{
    void operator()(HCS_SYSTEM p) const noexcept
    {
        API().HcsCloseComputeSystem(p);
    }
};

using UniqueHcsSystem = std::unique_ptr<std::remove_pointer_t<HCS_SYSTEM>, HcsSystemCloser>;

struct HcsOperationCloser
{
    void operator()(HCS_OPERATION p) const noexcept
    {
        API().HcsCloseOperation(p);
    }
};

using UniqueHcsOperation =
    std::unique_ptr<std::remove_pointer_t<HCS_OPERATION>, HcsOperationCloser>;

struct HlocalStringDeleter
{
    void operator()(void* p) const noexcept
    {
        API().LocalFree(p);
    }
};
using UniqueHlocalString = std::unique_ptr<wchar_t, HlocalStringDeleter>;

namespace mpl = logging;
using lvl = mpl::Level;

constexpr auto log_category = "HyperV-HCS-Wrapper";
constexpr auto default_operation_timeout = std::chrono::seconds{240};

// ---------------------------------------------------------

UniqueHcsOperation create_operation()
{
    mpl::trace(log_category, "create_operation(...)");
    return UniqueHcsOperation{API().HcsCreateOperation(nullptr, nullptr)};
}

// ---------------------------------------------------------

OperationResult wait_for_operation_result(
    UniqueHcsOperation op,
    std::chrono::milliseconds timeout = default_operation_timeout)
{
    mpl::debug(log_category,
               "wait_for_operation_result(...) > ({}), timeout: {} ms",
               fmt::ptr(op.get()),
               timeout.count());

    UniqueHlocalString result_msg{};
    const auto hresult_code =
        ResultCode{API().HcsWaitForOperationResult(op.get(), timeout.count(), out_ptr(result_msg))};
    mpl::debug(log_category,
               "wait_for_operation_result(...) > finished ({}), result_code: {}",
               fmt::ptr(op.get()),
               hresult_code);

    const auto result = OperationResult{hresult_code, result_msg ? result_msg.get() : L""};
    // FIXME: Replace with unicode logging
    fmt::print(L"{}{}{}",
               result.status_msg.empty() ? L"" : L"Result document: ",
               result.status_msg,
               result.status_msg.empty() ? L"" : L"\n");
    return result;
}

// ---------------------------------------------------------

template <typename FnType>
OperationResult perform_hcs_operation(const FnType& fn, const HcsSystemHandle& system)
{

    if (nullptr == system)
    {
        mpl::error(log_category,
                   "perform_hcs_operation(...) > Host Compute System handle is null!");
        return OperationResult{E_POINTER, L"HcsCreateOperation failed!"};
    }

    auto operation = create_operation();

    if (nullptr == operation)
    {
        mpl::error(log_category, "perform_hcs_operation(...) > HcsCreateOperation failed!");
        return OperationResult{E_POINTER, L"HcsCreateOperation failed!"};
    }

    // Perform the operation.
    const auto result = ResultCode{fn(operation.get())};

    if (!result)
    {
        mpl::error(log_category,
                   "perform_hcs_operation(...) > Operation failed! Result code {}",
                   result);
        return OperationResult{result, L"HCS operation failed!"};
    }

    mpl::debug(log_category, "perform_hcs_operation(...) > result: {}", static_cast<bool>(result));

    return wait_for_operation_result(std::move(operation));
}

} // namespace

// ---------------------------------------------------------

HCSWrapper::HCSWrapper(const Singleton<HCSWrapper>::PrivatePass& pass) noexcept
    : Singleton<HCSWrapper>::Singleton{pass}
{
}

// ---------------------------------------------------------

OperationResult HCSWrapper::open_compute_system(const std::string& name,
                                                HcsSystemHandle& out_hcs_system) const
{
    mpl::debug(log_category, "open_compute_system(...) > name: ({})", name);

    // Windows API uses wide strings.
    const std::wstring name_w = to_wstring(name);
    constexpr auto requested_access_level = GENERIC_ALL;

    UniqueHcsSystem system{};
    const ResultCode result =
        API().HcsOpenComputeSystem(name_w.c_str(), requested_access_level, out_ptr(system));
    if (!result)
    {
        mpl::debug(log_category,
                   "open_compute_system(...) > failed to open ({}), result code: ({})",
                   name,
                   result);
    }

    out_hcs_system = std::move(system);

    return {result, L""};
}

// ---------------------------------------------------------

OperationResult HCSWrapper::create_compute_system(const CreateComputeSystemParameters& params,
                                                  HcsSystemHandle& out_hcs_system) const
{
    mpl::debug(log_category, "HCSWrapper::create_compute_system(...) > params: {} ", params);

    auto operation = create_operation();

    if (nullptr == operation)
    {
        return OperationResult{E_POINTER, L"HcsCreateOperation failed."};
    }

    // Initialize guest state files if they does not exist
    {
        auto&& vmgs = params.guest_state.guest_state_file_path;
        auto&& vmrs = params.guest_state.runtime_state_file_path;

        if (vmgs && !std::filesystem::exists(vmgs->get()))
        {
            (void)HCS().create_empty_guest_state_file(params.name, vmgs->get());
        }

        if (vmrs && !std::filesystem::exists(vmrs->get()))
        {
            (void)HCS().create_empty_runtime_state_file(params.name, vmrs->get());
        }
    }

    const std::wstring name_w = to_wstring(params.name);
    // Render the template
    const auto vm_settings = fmt::to_wstring(params);

    UniqueHcsSystem system{};
    const auto result = ResultCode{API().HcsCreateComputeSystem(name_w.c_str(),
                                                                vm_settings.c_str(),
                                                                operation.get(),
                                                                nullptr,
                                                                out_ptr(system))};

    if (!result)
    {
        return OperationResult{result, L"HcsCreateComputeSystem failed."};
    }

    const auto op_result =
        wait_for_operation_result(std::move(operation), std::chrono::seconds{240});

    if (op_result)
    {
        out_hcs_system = std::move(system);
    }

    return op_result;
}

// ---------------------------------------------------------

OperationResult HCSWrapper::start_compute_system(const HcsSystemHandle& target_hcs_system) const
{
    mpl::debug(log_category,
               "start_compute_system(...) > handle: ({})",
               fmt::ptr(target_hcs_system.get()));

    return perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsStartComputeSystem(static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                                               op,
                                               nullptr);
        },
        target_hcs_system);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::shutdown_compute_system(const HcsSystemHandle& target_hcs_system) const
{
    mpl::debug(log_category,
               "shutdown_compute_system(...) > handle: ({})",
               fmt::ptr(target_hcs_system.get()));

    static constexpr wchar_t shutdown_option[] = LR"(
        {
            "Mechanism": "IntegrationService",
            "Type": "Shutdown"
        })";

    return perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsShutDownComputeSystem(static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                                                  op,
                                                  shutdown_option);
        },
        target_hcs_system);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::terminate_compute_system(const HcsSystemHandle& target_hcs_system) const
{
    mpl::debug(log_category,
               "terminate_compute_system(...) > handle: ({})",
               fmt::ptr(target_hcs_system.get()));

    return perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsTerminateComputeSystem(static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                                                   op,
                                                   nullptr);
        },
        target_hcs_system);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::pause_compute_system(const HcsSystemHandle& target_hcs_system) const
{
    mpl::debug(log_category,
               "pause_compute_system(...) > handle: ({})",
               fmt::ptr(target_hcs_system.get()));
    static constexpr wchar_t pause_option[] = LR"(
        {
            "SuspensionLevel": "Suspend",
            "HostedNotification": {
                "Reason": "Save"
            }
        })";

    return perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsPauseComputeSystem(static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                                               op,
                                               pause_option);
        },
        target_hcs_system);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::resume_compute_system(const HcsSystemHandle& target_hcs_system) const
{
    mpl::debug(log_category,
               "resume_compute_system(...) > handle: ({})",
               fmt::ptr(target_hcs_system.get()));

    return perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsResumeComputeSystem(static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                                                op,
                                                nullptr);
        },
        target_hcs_system);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::get_compute_system_properties(
    const HcsSystemHandle& target_hcs_system) const
{
    mpl::debug(log_category,
               "get_compute_system_properties(...) > handle: ({})",
               fmt::ptr(target_hcs_system.get()));

    // https://learn.microsoft.com/en-us/virtualization/api/hcs/schemareference#System_PropertyType
    static constexpr wchar_t vm_query[] = LR"(
        {
            "PropertyTypes":[]
        })";

    return perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsGetComputeSystemProperties(
                static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                op,
                vm_query);
        },
        target_hcs_system);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::grant_vm_access(const std::string& compute_system_name,
                                            const std::filesystem::path& file_path) const
{
    mpl::debug(log_category,
               "grant_vm_access(...) > name: ({}), file_path: ({})",
               compute_system_name,
               file_path.string());

    // The file/folder needs to exists because HcsGrantVmAccess will modify the
    // ACLs of the target file or folder.
    assert(std::filesystem::exists(file_path));

    const auto path_as_wstring = file_path.generic_wstring();
    const std::wstring csname_as_wstring = to_wstring(compute_system_name);
    const auto result = API().HcsGrantVmAccess(csname_as_wstring.c_str(), path_as_wstring.c_str());
    return {result, FAILED(result) ? L"GrantVmAccess failed!" : L""};
}

// ---------------------------------------------------------

OperationResult HCSWrapper::revoke_vm_access(const std::string& compute_system_name,
                                             const std::filesystem::path& file_path) const
{
    mpl::debug(log_category,
               "revoke_vm_access(...) > name: ({}), file_path: ({}) ",
               compute_system_name,
               file_path.string());

    const auto path_as_wstring = file_path.wstring();
    const std::wstring csname_as_wstring = to_wstring(compute_system_name);
    const auto result = API().HcsRevokeVmAccess(csname_as_wstring.c_str(), path_as_wstring.c_str());
    return {result, FAILED(result) ? L"RevokeVmAccess failed!" : L""};
}

// ---------------------------------------------------------

OperationResult HCSWrapper::get_compute_system_state(const HcsSystemHandle& target_hcs_system,
                                                     ComputeSystemState& state_out) const
{
    mpl::debug(log_category,
               "get_compute_system_state(...) > handle: ({})",
               fmt::ptr(target_hcs_system.get()));

    const auto result = perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsGetComputeSystemProperties(
                static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                op,
                nullptr);
        },
        target_hcs_system);

    if (!result)
        return result;

    state_out = [json = result.status_msg]() {
        QString qstr{QString::fromStdWString(json)};
        const auto doc = QJsonDocument::fromJson(qstr.toUtf8());
        const auto obj = doc.object();
        if (obj.contains("State"))
        {
            const auto state = obj["State"];
            const auto state_str = state.toString();
            const auto ccs = compute_system_state_from_string(state_str.toStdString());
            if (ccs)
            {
                return ccs.value();
            }
            return ComputeSystemState::unknown;
        }
        return ComputeSystemState::stopped;
    }();

    return {result.code, L""};
}

// ---------------------------------------------------------
OperationResult HCSWrapper::get_compute_system_guid(const HcsSystemHandle& target_hcs_system,
                                                    std::string& guid_out) const
{
    mpl::debug(log_category,
               "get_compute_system_guid(...) > handle: ({})",
               fmt::ptr(target_hcs_system.get()));

    const auto result = perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsGetComputeSystemProperties(
                static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                op,
                nullptr);
        },
        target_hcs_system);

    if (!result)
        return result;

    const std::string result_msg_str = wchar_to_utf8(result.status_msg);

    std::error_code ec;
    const auto parsed = boost::json::parse(result_msg_str, ec);
    if (ec)
    {
        return {E_FAIL, L"Json parse error"};
    }

    const auto json_object = parsed.as_object();

    if (const auto it = json_object.find("RuntimeId"); it != json_object.end())
    {
        guid_out = it->value().as_string();
        return result;
    }
    return {E_FAIL, L"GUID not found in compute system properties"};
}

// ---------------------------------------------------------

OperationResult HCSWrapper::modify_compute_system(const HcsSystemHandle& target_hcs_system,
                                                  const HcsRequest& params) const
{
    mpl::debug(log_category,
               "modify_compute_system(...) > handle: ({}), params: {}",
               fmt::ptr(target_hcs_system.get()),
               params);

    const auto json = fmt::to_wstring(params);

    return perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsModifyComputeSystem(static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                                                op,
                                                json.c_str(),
                                                nullptr);
        },
        target_hcs_system);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::set_compute_system_callback(const HcsSystemHandle& target_hcs_system,
                                                        void* context,
                                                        void (*callback)(HCS_EVENT* hcs_event,
                                                                         void* context)) const
{
    mpl::debug(log_category,
               "set_compute_system_callback(...) > handle: {}, context: {}, callback: {}",
               fmt::ptr(target_hcs_system.get()),
               fmt::ptr(context),
               fmt::ptr(callback));

    const ResultCode result =
        API().HcsSetComputeSystemCallback(static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                                          HCS_EVENT_OPTIONS::HcsEventOptionNone,
                                          context,
                                          reinterpret_cast<HCS_EVENT_CALLBACK>(callback));
    return {result, L""};
}

// ---------------------------------------------------------

OperationResult HCSWrapper::save_compute_system(const HcsSystemHandle& target_hcs_system,
                                                const HcsPath& save_path) const
{
    mpl::debug(log_category,
               "save_compute_system(...) > handle: {}, save_path: {}",
               fmt::ptr(target_hcs_system.get()),
               save_path);

    static constexpr auto json_template = string_literal<wchar_t>(R"(
    {{
        "SaveType": "ToFile",
        "SaveStateFilePath": "{0}"
    }})");

    const auto save_option = json_template.format(save_path);

    return perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsSaveComputeSystem(static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                                              op,
                                              save_option.c_str());
        },
        target_hcs_system);
}

// ---------------------------------------------------------
OperationResult HCSWrapper::create_empty_guest_state_file(
    const std::string& compute_system_name,
    const std::filesystem::path& vmgs_file_path) const
{
    const std::wstring path_w = vmgs_file_path.generic_wstring();
    const auto result = API().HcsCreateEmptyGuestStateFile(path_w.c_str());
    if (result)
    {
        return grant_vm_access(compute_system_name, vmgs_file_path);
    }

    return {result, L""};
}

// ---------------------------------------------------------

OperationResult HCSWrapper::create_empty_runtime_state_file(
    const std::string& compute_system_name,
    const std::filesystem::path& vmrs_file_path) const
{
    const std::wstring path_w = vmrs_file_path.generic_wstring();
    const auto result = API().HcsCreateEmptyRuntimeStateFile(path_w.c_str());
    if (result)
    {
        return grant_vm_access(compute_system_name, vmrs_file_path);
    }
    return {result, L""};
}

} // namespace multipass::hyperv::hcs
