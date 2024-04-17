// CustomAction.cpp : Defines the entry point for the custom action.
#include "pch.h"
#include <stdlib.h>

UINT __stdcall AskRemoveData(__in MSIHANDLE hInstall)
{
    HRESULT hr = S_OK;
    DWORD er = ERROR_SUCCESS;

    hr = WcaInitialize(hInstall, "AskRemoveData");

    WcaLog(LOGMSG_STANDARD, "Begin AskRemoveData.");

    WCHAR szBuf[1024];
    DWORD cchBuf = _countof(szBuf);
    DWORD dwErr = MsiGetProperty(hInstall, L"RemoveDataText", szBuf, &cchBuf);
    if (dwErr != ERROR_SUCCESS)
        return ERROR_UNKNOWN_PROPERTY;

    PMSIHANDLE hRec = MsiCreateRecord(1);
    MsiRecordSetString(hRec, 0, szBuf);
    int result = MsiProcessMessage(hInstall, INSTALLMESSAGE(MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2), hRec);

    if (result == IDYES)
        MsiSetProperty(hInstall, L"REMOVE_DATA", L"YES");
    else if (result == IDNO)
        MsiSetProperty(hInstall, L"REMOVE_DATA", L"NO");

    er = SUCCEEDED(hr) ? ERROR_SUCCESS : ERROR_UNKNOWN_PROPERTY;
    return WcaFinalize(er);
}
