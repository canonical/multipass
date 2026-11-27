// CustomAction.cpp : Defines the entry point for the custom action.
#include "pch.h"
#include <dismapi.h>
#include <fstream>
#include <msi.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#pragma comment(lib, "dismapi.lib")

namespace
{
// Returns the `local.driver` value from the daemon settings file, or an empty string.
// The installer writes it without a section header, so GetPrivateProfileString can't read it.
std::wstring read_existing_driver(const std::wstring& mp_data_dir)
{
    // Mirrors Platform::daemon_config_home(): the legacy location wins if it exists
    WCHAR system_dir[MAX_PATH] = {};
    GetSystemDirectoryW(system_dir, _countof(system_dir));
    std::wstring dir =
        std::wstring{system_dir} + L"\\config\\systemprofile\\AppData\\Local\\multipassd";
    if (GetFileAttributesW(dir.c_str()) == INVALID_FILE_ATTRIBUTES)
        dir = mp_data_dir;

    std::wifstream file{dir + L"\\multipassd.conf"};
    const std::wstring key = L"local.driver=";
    for (std::wstring line; std::getline(file, line);)
        if (line.rfind(key, 0) == 0)
            return line.substr(key.size(), line.find_last_not_of(L" \t\r") + 1 - key.size());
    return {};
}
} // namespace

UINT __stdcall AskRemoveData(__in MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    DWORD er = ERROR_SUCCESS;
    WCHAR szBuf[1024];
    DWORD cchBuf = _countof(szBuf);
    PMSIHANDLE hRec;
    UINT removeData;

    hr = WcaInitialize(hInstall, __FUNCTION__);
    ExitOnFailure(hr, "Failed to initialize");

    WcaLog(LOGMSG_STANDARD, "Begin AskRemoveData.");

    hr = MsiGetProperty(hInstall, L"RemoveDataText", szBuf, &cchBuf);
    ExitOnFailure(hr, "Failed getting RemoveDataText");

    hRec = MsiCreateRecord(1);
    ExitOnNullWithLastError(hRec, hr, "Failed to create record");

    MsiRecordSetString(hRec, 0, szBuf);
    removeData = MsiProcessMessage(hInstall,
                                   INSTALLMESSAGE(MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2),
                                   hRec);

    if (removeData == IDYES)
        hr = MsiSetProperty(hInstall, L"REMOVE_DATA", L"yes");
    else if (removeData == IDNO)
        hr = MsiSetProperty(hInstall, L"REMOVE_DATA", L"no");
    ExitOnFailure(hr, "Failed setting property '%hs'", L"REMOVE_DATA")

        // clang-format off
LExit:
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_UNKNOWN_PROPERTY;
    return WcaFinalize(er);
    // clang-format on
}

UINT __stdcall EnableHyperV(__in MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    DWORD er = ERROR_SUCCESS;
    HANDLE hCancel_ = NULL;
    WCHAR szBuf[1024];
    DWORD cchBuf = _countof(szBuf);
    DismSession hSession = DISM_SESSION_DEFAULT;
    DismString* pErrorString = nullptr;
    BOOL bDismInit = FALSE;
    PMSIHANDLE hActionData;
    UINT uiLevel;
    DWORD exitCode = 0;
    std::wstring customActionData;
    std::wstring args;
    const wchar_t* feature = nullptr;

    hr = WcaInitialize(hInstall, __FUNCTION__);
    ExitOnFailure(hr, "Failed to initialize");
    WcaLog(LOGMSG_STANDARD, "Begin EnableHyperV.");

    hr = MsiGetProperty(hInstall, TEXT("CustomActionData"), szBuf, &cchBuf);
    ExitOnFailure(hr, "Failed getting CustomActionData");

    // Either "[UILevel]|[DRIVER]" or, on upgrade, "[UILevel]|upgrade|[MP_DATA_DIR]"
    customActionData = szBuf;
    uiLevel = _wtoi(customActionData.c_str());
    args = customActionData.substr(customActionData.find(L'|') + 1);
    // TODO hyperv migration, remove: the dialog is skipped on upgrade, so DRIVER isn't the user's
    // choice. Only legacy hyperv users need VirtualMachinePlatform, to migrate to hcs.
    if (args.rfind(L"upgrade|", 0) == 0)
        feature = read_existing_driver(args.substr(8)) == L"hyperv" ? L"VirtualMachinePlatform"
                                                                    : nullptr;
    // TODO hyperv migration, revert: always enable VirtualMachinePlatform
    else if (args == L"hcs")
        feature = L"VirtualMachinePlatform";
    else if (args == L"hyperv")
        feature = L"Microsoft-Hyper-V";
    WcaLog(LOGMSG_STANDARD, std::to_string(uiLevel).c_str());

    if (!feature)
    {
        WcaLog(LOGMSG_STANDARD, "No Windows feature needs to be enabled.");
        ExitFunction();
    }

    hCancel_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    ExitOnNullWithLastError(hCancel_, hr, "Failed creating event");

    hr = DismInitialize(DismLogErrorsWarningsInfo, NULL, NULL);
    if (FAILED(hr))
    {
        DismGetLastErrorMessage(&pErrorString);
        ExitOnFailure(hr,
                      "Failed initializing DISM. %ls",
                      (pErrorString && pErrorString->Value) ? pErrorString->Value : L"");
    }
    bDismInit = TRUE;

    hr = DismOpenSession(DISM_ONLINE_IMAGE, nullptr, nullptr, &hSession);
    if (FAILED(hr))
    {
        DismGetLastErrorMessage(&pErrorString);
        ExitOnFailure(hr,
                      "Failed opening DISM online session. %ls",
                      (pErrorString && pErrorString->Value) ? pErrorString->Value : L"");
    }

    WcaLog(LOGMSG_STANDARD, "Enabling feature %ls", feature);

    hActionData = MsiCreateRecord(1);
    if (hActionData && SUCCEEDED(WcaSetRecordString(hActionData, 1, feature)))
        WcaProcessMessage(INSTALLMESSAGE::INSTALLMESSAGE_ACTIONDATA, hActionData);

    hr = DismEnableFeature(hSession,
                           feature,
                           nullptr,
                           DismPackageNone,
                           FALSE,
                           nullptr,
                           0,
                           TRUE,
                           hCancel_,
                           nullptr,
                           nullptr);
    if (HRESULT_CODE(hr) == ERROR_SUCCESS_REBOOT_REQUIRED)
    {
        hr = S_OK;
        WcaLog(LOGMSG_STANDARD,
               "Enabled feature %ls. However, it requires reboot to complete",
               feature);
        if (uiLevel > 3)
            WcaDeferredActionRequiresReboot();
        else
            WcaLog(LOGMSG_STANDARD, "Silent install. Skipping reboot");
    }

    if (FAILED(hr))
    {
        DismGetLastErrorMessage(&pErrorString);
        WcaLogError(hr,
                    "Failed enabling feature %ls. %ls",
                    feature,
                    (pErrorString && pErrorString->Value) ? pErrorString->Value : L"");
        ExitOnFailure(hr, "Failed enabling feature");
    }

    // Cancelled?
    if (WaitForSingleObject(hCancel_, 0) == WAIT_OBJECT_0)
    {
        WcaLog(LOGMSG_STANDARD, "Seems like DISM was canceled.");
        hr = S_FALSE;
        ExitFunction();
    }

LExit:
    if (hCancel_)
        CloseHandle(hCancel_);
    if (pErrorString)
        DismDelete(pErrorString);
    if (hSession != DISM_SESSION_DEFAULT)
        DismCloseSession(hSession);
    if (bDismInit)
        DismShutdown();

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_UNKNOWN_PROPERTY;
    return WcaFinalize(er);
}
