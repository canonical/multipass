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

#include "windows_feature_status.h"

#include <Windows.h>
#include <Wbemidl.h>
#include <comdef.h>

#include <fmt/xchar.h>
#include <ztd/out_ptr.hpp>

#include <memory>

#pragma comment(lib, "wbemuuid.lib")

using ztd::out_ptr::out_ptr;

namespace
{
struct ComDeleter
{
    template <typename T>
    void operator()(T* ptr) const
    {
        if (ptr)
            ptr->Release();
    }
};

template <typename T>
using UniqueComPtr = std::unique_ptr<T, ComDeleter>;

struct CoInitGuard
{
    HRESULT hr;
    CoInitGuard() : hr(CoInitializeEx(nullptr, COINIT_MULTITHREADED))
    {
    }
    ~CoInitGuard()
    {
        if (SUCCEEDED(hr))
            CoUninitialize();
    }
    explicit operator bool() const
    {
        return SUCCEEDED(hr);
    }
};

struct VariantGuard
{
    VARIANT var;
    VariantGuard()
    {
        VariantInit(&var);
    }
    ~VariantGuard()
    {
        VariantClear(&var);
    }
    VARIANT* operator&()
    {
        return &var;
    }
    auto intVal() const
    {
        return var.intVal;
    }
};

UniqueComPtr<IWbemServices> prepare_connection()
{
    UniqueComPtr<IWbemLocator> locator;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator,
                                nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_IWbemLocator,
                                out_ptr(locator))))
        return nullptr;

    UniqueComPtr<IWbemServices> services;
    if (FAILED(locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"),
                                      nullptr,
                                      nullptr,
                                      nullptr,
                                      0,
                                      nullptr,
                                      nullptr,
                                      out_ptr(services))))
        return nullptr;

    CoSetProxyBlanket(services.get(),
                      RPC_C_AUTHN_WINNT,
                      RPC_C_AUTHZ_NONE,
                      nullptr,
                      RPC_C_AUTHN_LEVEL_CALL,
                      RPC_C_IMP_LEVEL_IMPERSONATE,
                      nullptr,
                      EOAC_NONE);
    return services;
}
} // namespace

namespace multipass
{
std::optional<WindowsFeatureState> get_windows_feature_state(std::wstring_view feature_name)
{
    thread_local CoInitGuard com_init{};

    if (!com_init)
        return {};

    const auto query =
        fmt::format(L"SELECT InstallState FROM Win32_OptionalFeature WHERE Name='{0}'",
                    feature_name);

    if (auto conn = prepare_connection())
    {
        UniqueComPtr<IEnumWbemClassObject> enumerator;
        if (FAILED(conn->ExecQuery(_bstr_t(L"WQL"),
                                   _bstr_t(query.c_str()),
                                   WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                   nullptr,
                                   out_ptr(enumerator))))
            return {};

        UniqueComPtr<IWbemClassObject> obj;
        ULONG count = 0;
        if (enumerator->Next(WBEM_INFINITE, 1, out_ptr(obj), &count) != S_OK || !count)
            return {};

        VariantGuard var;
        if (FAILED(obj->Get(L"InstallState", 0, &var, nullptr, nullptr)))
            return {};

        return static_cast<WindowsFeatureState>(var.intVal());
    }

    return {};
}
} // namespace multipass
