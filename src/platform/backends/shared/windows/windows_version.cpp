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

#include <shared/windows/windows_version.h>

#include <windows.h>
#include <winternl.h>

#include <memory>

namespace multipass
{
std::optional<windows_version> get_windows_version()
{
    struct HModuleDeleter
    {
        void operator()(HMODULE handle)
        {
            if (handle)
            {
                FreeLibrary(handle);
            }
        }
    };

    using unique_hmodule = std::unique_ptr<std::remove_pointer<HMODULE>::type, HModuleDeleter>;
    using RtlGetVersionPtr = NTSTATUS(WINAPI*)(PRTL_OSVERSIONINFOW);

    static std::optional<windows_version> cached_version = []() -> std::optional<windows_version> {
        unique_hmodule hNtdll(LoadLibraryA("ntdll.dll"));
        if (hNtdll)
        {
            RtlGetVersionPtr RtlGetVersion =
                (RtlGetVersionPtr)GetProcAddress(hNtdll.get(), "RtlGetVersion");
            if (RtlGetVersion)
            {
                // Initialize the version info structure
                RTL_OSVERSIONINFOW osVersionInfo = {0};
                osVersionInfo.dwOSVersionInfoSize = sizeof(RTL_OSVERSIONINFOW);
                // Call RtlGetVersion
                if (const auto status = RtlGetVersion(&osVersionInfo); status == 0)
                {
                    windows_version ver{};
                    ver.major = osVersionInfo.dwMajorVersion;
                    ver.minor = osVersionInfo.dwMinorVersion;
                    ver.build = osVersionInfo.dwBuildNumber;
                    return ver;
                }
            }
        }
        return {};
    }();
    return cached_version;
}

} // namespace multipass
