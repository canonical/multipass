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

#include <hyperv_api/hcs/hyperv_hcs_api_table.h>
#include <hyperv_api/hcs/hyperv_hcs_create_compute_system_params.h>
#include <hyperv_api/hyperv_api_operation_result.h>
#include <hyperv_api/hyperv_api_string_conversion.h>

#include <multipass/logging/log.h>
#include <platform/platform_win.h>

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

/**
 * Category for the log messages.
 */
constexpr static auto log_category = "HyperV-HCS-Wrapper";

/**
 * Default timeout value for HCS API operations
 */
constexpr static auto default_operation_timeout = std::chrono::seconds{240};

// ---------------------------------------------------------

/**
 * Create a new HCS operation.
 *
 * @param api The HCS API table
 *
 * @return UniqueHcsOperation The new operation
 */
UniqueHcsOperation create_operation()
{
    mpl::trace(log_category, "create_operation(...)");
    return UniqueHcsOperation{API().HcsCreateOperation(nullptr, nullptr)};
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
    mpl::debug(log_category, "open_host_compute_system(...) > name: ({})", name);

    // Windows API uses wide strings.
    const std::wstring name_w = maybe_widen{name};
    constexpr static auto kRequestedAccessLevel = GENERIC_ALL;

    UniqueHcsSystem system{};
    const ResultCode result =
        API().HcsOpenComputeSystem(name_w.c_str(), kRequestedAccessLevel, out_ptr(system));
    if (!result)
    {
        mpl::debug(log_category,
                   "open_host_compute_system(...) > failed to open ({}), result code: ({})",
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

    const std::wstring name_w = maybe_widen{params.name};
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
        out_hcs_system = std::move(system);
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

    static constexpr wchar_t c_shutdownOption[] = LR"(
        {
            "Mechanism": "IntegrationService",
            "Type": "Shutdown"
        })";

    return perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsShutDownComputeSystem(static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                                                  op,
                                                  c_shutdownOption);
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
    static constexpr wchar_t c_pauseOption[] = LR"(
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
                                               c_pauseOption);
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
    static constexpr wchar_t c_VmQuery[] = LR"(
        {
            "PropertyTypes":[]
        })";

    return perform_hcs_operation(
        [&](auto&& op) {
            return API().HcsGetComputeSystemProperties(
                static_cast<HCS_SYSTEM>(target_hcs_system.get()),
                op,
                c_VmQuery);
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

    const auto path_as_wstring = file_path.generic_wstring();
    const std::wstring csname_as_wstring = maybe_widen{compute_system_name};
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
    const std::wstring csname_as_wstring = maybe_widen{compute_system_name};
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
                                                        void (*callback)(void* hcs_event,
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

} // namespace multipass::hyperv::hcs
