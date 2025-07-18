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
#include <hyperv_api/hcs/hyperv_hcs_api_wrapper.h>

#include <hyperv_api/hcs/hyperv_hcs_api_table.h>
#include <hyperv_api/hcs/hyperv_hcs_create_compute_system_params.h>
#include <hyperv_api/hyperv_api_operation_result.h>
#include <hyperv_api/hyperv_api_string_conversion.h>

#include <multipass/logging/log.h>
#include <multipass/platform_win.h>

#include <ComputeDefs.h>

#include <QJsonDocument>
#include <QJsonObject>

#include <chrono>
#include <memory>

#include <fmt/xchar.h>
#include <ztd/out_ptr.hpp>

using ztd::out_ptr::out_ptr;

namespace multipass::hyperv::hcs
{

namespace
{

using UniqueHcsSystem =
    std::unique_ptr<std::remove_pointer_t<HCS_SYSTEM>, decltype(HCSAPITable::CloseComputeSystem)>;
using UniqueHcsOperation =
    std::unique_ptr<std::remove_pointer_t<HCS_OPERATION>, decltype(HCSAPITable::CloseOperation)>;
using UniqueHlocalString = std::unique_ptr<wchar_t, decltype(HCSAPITable::LocalFree)>;

namespace mpl = logging;
using lvl = mpl::Level;

/**
 * Category for the log messages.
 */
constexpr static auto kLogCategory = "HyperV-HCS-Wrapper";

/**
 * Default timeout value for HCS API operations
 */
constexpr static auto kDefaultOperationTimeout = std::chrono::seconds{240};

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
    mpl::trace(kLogCategory, "create_operation(...)");
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
OperationResult wait_for_operation_result(
    const HCSAPITable& api,
    UniqueHcsOperation op,
    std::chrono::milliseconds timeout = kDefaultOperationTimeout)
{
    mpl::debug(kLogCategory,
               "wait_for_operation_result(...) > ({}), timeout: {} ms",
               fmt::ptr(op.get()),
               timeout.count());

    UniqueHlocalString result_msg{};
    const auto hresult_code = ResultCode{
        api.WaitForOperationResult(op.get(), timeout.count(), out_ptr(result_msg, api.LocalFree))};
    mpl::debug(kLogCategory,
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
    mpl::debug(kLogCategory, "open_host_compute_system(...) > name: ({})", name);

    // Windows API uses wide strings.
    const std::wstring name_w = maybe_widen{name};
    constexpr static auto kRequestedAccessLevel = GENERIC_ALL;

    UniqueHcsSystem system{};
    const ResultCode result = api.OpenComputeSystem(name_w.c_str(),
                                                    kRequestedAccessLevel,
                                                    out_ptr(system, api.CloseComputeSystem));
    if (!result)
    {
        mpl::debug(kLogCategory,
                   "open_host_compute_system(...) > failed to open ({}), result code: ({})",
                   name,
                   result);
    }
    return system;
}

// ---------------------------------------------------------

template <typename FnType, typename... Args>
OperationResult perform_hcs_operation(const HCSAPITable& api,
                                      const FnType& fn,
                                      UniqueHcsSystem system,
                                      Args&&... args)
{
    auto operation = create_operation(api);

    if (nullptr == operation)
    {
        mpl::error(kLogCategory, "perform_hcs_operation(...) > HcsCreateOperation failed! ");
        return OperationResult{E_POINTER, L"HcsCreateOperation failed!"};
    }

    // Perform the operation.
    const auto result = ResultCode{fn(system.get(), operation.get(), std::forward<Args>(args)...)};

    if (!result)
    {
        mpl::error(kLogCategory,
                   "perform_hcs_operation(...) > Operation failed! Result code {}",
                   result);
        return OperationResult{result, L"HCS operation failed!"};
    }

    mpl::debug(kLogCategory, "perform_hcs_operation(...) > result: {}", static_cast<bool>(result));

    return wait_for_operation_result(api, std::move(operation));
}

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

    auto system = open_host_compute_system(api, target_hcs_system_name);

    if (nullptr == system)
    {
        mpl::debug(kLogCategory,
                   "perform_hcs_operation(...) > HcsOpenComputeSystem failed! {}",
                   target_hcs_system_name);
        return OperationResult{E_INVALIDARG, L"HcsOpenComputeSystem failed!"};
    }

    return perform_hcs_operation(api, fn, std::move(system), std::forward<Args>(args)...);
}

} // namespace

// ---------------------------------------------------------

HCSWrapper::HCSWrapper(const HCSAPITable& api_table) : api{api_table}
{
    mpl::debug(kLogCategory,
               "HCSWrapper::HCSWrapper(...) > Schema Version: {}, API table: {}",
               get_os_supported_schema_version(),
               api);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::create_compute_system(const CreateComputeSystemParameters& params) const
{
    mpl::debug(kLogCategory, "HCSWrapper::create_compute_system(...) > params: {} ", params);

    auto operation = create_operation(api);

    if (nullptr == operation)
    {
        return OperationResult{E_POINTER, L"HcsCreateOperation failed."};
    }

    const std::wstring name_w = maybe_widen{params.name};
    // Render the template
    const auto vm_settings = fmt::to_wstring(params);

    UniqueHcsSystem system{};
    const auto result =
        ResultCode{api.CreateComputeSystem(name_w.c_str(),
                                           vm_settings.c_str(),
                                           operation.get(),
                                           nullptr,
                                           out_ptr(system, api.CloseComputeSystem))};

    if (!result)
    {
        return OperationResult{result, L"HcsCreateComputeSystem failed."};
    }

    return wait_for_operation_result(api, std::move(operation), std::chrono::seconds{240});
}

// ---------------------------------------------------------

OperationResult HCSWrapper::start_compute_system(const std::string& compute_system_name) const
{
    mpl::debug(kLogCategory, "start_compute_system(...) > name: ({})", compute_system_name);
    return perform_hcs_operation(api, api.StartComputeSystem, compute_system_name, nullptr);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::shutdown_compute_system(const std::string& compute_system_name) const
{
    mpl::debug(kLogCategory, "shutdown_compute_system(...) > name: ({})", compute_system_name);

    static constexpr wchar_t c_shutdownOption[] = LR"(
        {
            "Mechanism": "IntegrationService",
            "Type": "Shutdown"
        })";

    return perform_hcs_operation(api,
                                 api.ShutDownComputeSystem,
                                 compute_system_name,
                                 c_shutdownOption);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::terminate_compute_system(const std::string& compute_system_name) const
{
    mpl::debug(kLogCategory, "terminate_compute_system(...) > name: ({})", compute_system_name);
    return perform_hcs_operation(api, api.TerminateComputeSystem, compute_system_name, nullptr);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::pause_compute_system(const std::string& compute_system_name) const
{
    mpl::debug(kLogCategory, "pause_compute_system(...) > name: ({})", compute_system_name);
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
    mpl::debug(kLogCategory, "resume_compute_system(...) > name: ({})", compute_system_name);
    return perform_hcs_operation(api, api.ResumeComputeSystem, compute_system_name, nullptr);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::get_compute_system_properties(
    const std::string& compute_system_name) const
{

    mpl::debug(kLogCategory,
               "get_compute_system_properties(...) > name: ({})",
               compute_system_name);

    // https://learn.microsoft.com/en-us/virtualization/api/hcs/schemareference#System_PropertyType
    static constexpr wchar_t c_VmQuery[] = LR"(
        {
            "PropertyTypes":[]
        })";

    return perform_hcs_operation(api,
                                 api.GetComputeSystemProperties,
                                 compute_system_name,
                                 c_VmQuery);
}

// ---------------------------------------------------------

OperationResult HCSWrapper::grant_vm_access(const std::string& compute_system_name,
                                            const std::filesystem::path& file_path) const
{
    mpl::debug(kLogCategory,
               "grant_vm_access(...) > name: ({}), file_path: ({})",
               compute_system_name,
               file_path.string());

    const auto path_as_wstring = file_path.generic_wstring();
    const std::wstring csname_as_wstring = maybe_widen{compute_system_name};
    const auto result = api.GrantVmAccess(csname_as_wstring.c_str(), path_as_wstring.c_str());
    return {result, FAILED(result) ? L"GrantVmAccess failed!" : L""};
}

// ---------------------------------------------------------

OperationResult HCSWrapper::revoke_vm_access(const std::string& compute_system_name,
                                             const std::filesystem::path& file_path) const
{
    mpl::debug(kLogCategory,
               "revoke_vm_access(...) > name: ({}), file_path: ({}) ",
               compute_system_name,
               file_path.string());

    const auto path_as_wstring = file_path.wstring();
    const std::wstring csname_as_wstring = maybe_widen{compute_system_name};
    const auto result = api.RevokeVmAccess(csname_as_wstring.c_str(), path_as_wstring.c_str());
    return {result, FAILED(result) ? L"RevokeVmAccess failed!" : L""};
}

// ---------------------------------------------------------

OperationResult HCSWrapper::get_compute_system_state(const std::string& compute_system_name,
                                                     ComputeSystemState& state_out) const
{
    mpl::debug(kLogCategory, "get_compute_system_state(...) > name: ({})", compute_system_name);

    const auto result =
        perform_hcs_operation(api, api.GetComputeSystemProperties, compute_system_name, nullptr);

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

OperationResult HCSWrapper::modify_compute_system(const std::string& compute_system_name,
                                                  const HcsRequest& params) const
{
    mpl::debug(kLogCategory, "modify_compute_system(...) > params: {}", params);

    const auto json = fmt::to_wstring(params);
    return perform_hcs_operation(api,
                                 api.ModifyComputeSystem,
                                 compute_system_name,
                                 json.c_str(),
                                 nullptr);
}

} // namespace multipass::hyperv::hcs
