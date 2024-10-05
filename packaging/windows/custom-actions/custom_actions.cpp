// CustomAction.cpp : Defines the entry point for the custom action.
#include "pch.h"
#include <dismapi.h>
#include <msi.h>
#include <stdlib.h>
#include <string>
#include <tchar.h>
#pragma comment(lib, "dismapi.lib")

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
    removeData = MsiProcessMessage(hInstall, INSTALLMESSAGE(MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2), hRec);

    if (removeData == IDYES)
        hr = MsiSetProperty(hInstall, L"REMOVE_DATA", L"yes");
    else if (removeData == IDNO)
        hr = MsiSetProperty(hInstall, L"REMOVE_DATA", L"no");
    ExitOnFailure(hr, "Failed setting property '%hs'", L"REMOVE_DATA")

LExit:
    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_UNKNOWN_PROPERTY;
    return WcaFinalize(er);
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

    hr = WcaInitialize(hInstall, __FUNCTION__);
    ExitOnFailure(hr, "Failed to initialize");
    WcaLog(LOGMSG_STANDARD, "Begin EnableHyperV.");

    hr = MsiGetProperty(hInstall, TEXT("CustomActionData"), szBuf, &cchBuf);
    ExitOnFailure(hr, "Failed getting CustomActionData");

    uiLevel = _ttoi(szBuf);
    WcaLog(LOGMSG_STANDARD, std::to_string(uiLevel).c_str());

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

    WcaLog(LOGMSG_STANDARD, "Enabling feature Microsoft-Hyper-V");

    hActionData = MsiCreateRecord(1);
    if (hActionData && SUCCEEDED(WcaSetRecordString(hActionData, 1, L"Microsoft-Hyper-V")))
        WcaProcessMessage(INSTALLMESSAGE::INSTALLMESSAGE_ACTIONDATA, hActionData);

    hr = DismEnableFeature(hSession,
                           L"Microsoft-Hyper-V",
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
        WcaLog(LOGMSG_STANDARD, "Enabled feature Microsoft-Hyper-V. However, it requires reboot to complete");
        if (uiLevel > 3)
            WcaDeferredActionRequiresReboot();
        else
            WcaLog(LOGMSG_STANDARD, "Silent install. Skipping reboot");
    }

    if (FAILED(hr))
    {
        DismGetLastErrorMessage(&pErrorString);
        WcaLogError(hr,
                    "Failed enabling feature Microsoft-Hyper-V. %ls",
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
