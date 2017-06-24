
#include "..\..\..\Include\ImgGrbIt.h"

HINSTANCE		hInstance;
WCHAR			g_wszCWD[MAX_PATH + 1];
WCHAR			wszConfigPath[MAX_PATH + 1];
WCHAR			wszSGEventName[MAX_PATH + 1];
HANDLE			hRTThread = NULL;
HANDLE			hTimer = NULL;
DWORD			dwThreadId;
long 			nRTInterval;
BOOL			bRealTime = FALSE;

// Pointer to the PDATA structure which holds data to be sent to CPM.
static PDATA	*pMyPData = NULL;

BOOL	RetrieveSnapShots(const WCHAR *wszDirPath, const WCHAR *wszAppName,
    const SYSTEMTIME *stFromDate, const SYSTEMTIME *stToDate,
    SGFILEDATA *pSnapShotsList);
BOOL	GetNextDate(WORD *wDate, WORD *wMonth, WORD *wYear);
BOOL	FindNextDayDir(const WCHAR *wszPath);
int		FindNextAppDir(const WCHAR *wszPath, const WCHAR *wszEXEName, WCHAR __out *wszCurAppDir);
BOOL	FindFilesInAppDir(const WCHAR *wszPath, SGFILEDATA *pSnapShotsList);
DWORD	WINAPI TimerThread(LPVOID lpParam);

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD nReasons, LPVOID Reserved) {
    switch (nReasons) {

        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:


        case DLL_PROCESS_DETACH:
        case DLL_THREAD_DETACH:

            break;

    }

    hInstance = hInst;
    return TRUE;
}

BOOL ProcessCmd(RM_COMMAND Cmd, MNTRDATA *MntrData, PDATA __out **pPDataToCPM) {

    int iRetVal;

    SYSTEMTIME stCurrentDay;
    WCHAR wszShotsSubDir[MAX_PATH + 1];
    WCHAR *pwszCWD = NULL;

    static HMODULE hSGPluginDll = NULL;

    INSTALLPROC pInstallSGHook = NULL;
    REMOVEPROC pRemoveSGHook = NULL;

    static HANDLE hEvent = NULL;

    SGCONFIG SGConfig;


    int fp = 0;
    errno_t err;

    if (hSGPluginDll == NULL) {
        hSGPluginDll = LoadLibrary(L"ImageGrab.dll");
        if (hSGPluginDll == NULL)
            return FALSE;
    }

    switch (Cmd) {
        case IF_INIT_PLUGIN:
        {
            pInstallSGHook = (INSTALLPROC)GetProcAddress(hSGPluginDll, "InstallSGHook");
            if (pInstallSGHook == NULL)
                return FALSE;

            // get current working directory and store in global variable
            if ((pwszCWD = _wgetcwd(pwszCWD, MAX_PATH + 1)) == NULL) {
                //return FALSE;
            }

            // copy the path returned to global variable
            wsprintf(g_wszCWD, L"%s", pwszCWD);

            // free the memory
            if (pwszCWD)
                free(pwszCWD);

            // Create the SnapShots\\thisDay sub-directory
            // get today's date
            GetLocalTime(&stCurrentDay);

            // SG directory
            if ((iRetVal = CreateDirectory(FP_SG_DIR, NULL)) == ERROR_PATH_NOT_FOUND) {
                return FALSE;
            }

            // SG shots directory
            if ((iRetVal = CreateDirectory(FP_SG_SHOTS_DIR, NULL)) == ERROR_PATH_NOT_FOUND) {
                return FALSE;
            }

            // construct the path for current date's shots directory
            wsprintf(wszShotsSubDir, L"%s\\%d-%d-%d", FP_SG_SHOTS_DIR, stCurrentDay.wDay, stCurrentDay.wMonth, stCurrentDay.wYear);
            if ((iRetVal = CreateDirectory(wszShotsSubDir, NULL)) == ERROR_PATH_NOT_FOUND) {
                return FALSE;
            }

            swprintf_s(wszShotsSubDir, MAX_PATH + 1, L"%s", FP_SG_RT_SHOTS_DIR);
            if ((iRetVal = CreateDirectory(wszShotsSubDir, NULL)) == ERROR_PATH_NOT_FOUND) {
                return FALSE;
            }

            // Call ScreenGrabber Hook
            if (pInstallSGHook(g_wszCWD) == FALSE) {
                return FALSE;
                //PrintToStatusWindow(L"SG hook error");
            }
            break;
        }

        case IF_CMD_PLUGIN:
        {
            switch (MntrData->SGMData.cmd) {
                case SGIF_GET_SHOTS:
                {
                    WCHAR		wszSnapShotsPath[MAX_PATH + 1];
                    HANDLE		hIFDataReadyEvent = NULL;
                    SGFILEDATA	SGFileData;

                    // Construct the absolute path of the LogFiles directory
                    wsprintf(wszSnapShotsPath, L"%s\\%s", g_wszCWD, FP_SG_SHOTS_DIR);

                    // search the directory for the required files 
                    // (within FromDate and ToDate, inclusive).
                    if (RetrieveSnapShots(wszSnapShotsPath, MntrData->SGMData.SGMStructs.SGMRetSnaps.wszAppName,
                        &MntrData->SGMData.SGMStructs.SGMRetSnaps.stFromDate,
                        &MntrData->SGMData.SGMStructs.SGMRetSnaps.stToDate, &SGFileData) == FALSE) {
                        // error
                    } else {
                        // allocate memory for PDATA
                        if ((pMyPData = (PDATA*)malloc(sizeof(PDATA))) == NULL)
                            return FALSE;

                        // construct the PDATA structure and send it
                        pMyPData->iMessage = SGM_SHOTS;
                        pMyPData->PluginID = SCREENGRABBER;
                        memcpy(&pMyPData->PDataType.SGFileData, &SGFileData, sizeof(SGFILEDATA));

                        // set event so that CPM can call ProcessCmd() and take the PDATA

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

                }// case SGIF_GET_SHOTS


                case SGIF_RT_START:
                {
                    hEvent = CreateEvent(NULL, TRUE, FALSE, SGCONFIGUPDATE);
                    if (hEvent == NULL)
                        return FALSE;

                    wsprintf(wszConfigPath, L"%s\\%s\\ScreenGrabberConfig.conf", g_wszCWD, FP_SG_DIR);

                    err = _wsopen_s(&fp, wszConfigPath, _O_RDONLY | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
                    if (fp == -1)
                        return FALSE;

                    err = _read(fp, &SGConfig, sizeof(SGCONFIG));
                    if (err <= 0) {
                        if (fp) {
                            _close(fp);
                            fp = 0;
                        }
                        return FALSE;
                    }

                    if (fp) {
                        _close(fp);
                        fp = 0;
                    }


                    err = _wsopen_s(&fp, wszConfigPath, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
                        _SH_DENYNO, _S_IREAD | _S_IWRITE);
                    if (fp == -1)
                        return FALSE;

                    err = _write(fp, &SGConfig, sizeof(SGCONFIG));
                    if (err <= 0) {
                        if (fp) {
                            _close(fp);
                            fp = 0;
                        }
                        return FALSE;
                    }
                    nRTInterval = SGConfig.ulRTInterval;

                    err = _write(fp, &MntrData->SGMData.SGMStructs.SGRealApp, sizeof(SGREALAPP));
                    if (err <= 0) {
                        if (fp) {
                            _close(fp);
                            fp = 0;
                        }
                        return FALSE;
                    }

                    if (fp) {
                        _close(fp);
                        fp = 0;
                    }

                    bRealTime = TRUE;
                    iRetVal = SetEvent(hEvent);
                    if (iRetVal == 0)
                        return FALSE;

                    hRTThread = CreateThread(NULL, 0, TimerThread, NULL, 0, &dwThreadId);
                    if (hRTThread == NULL) {
                        return FALSE;
                    }
                    break;
                }

                case SGIF_RT_STOP:
                {
                    bRealTime = FALSE;
                    iRetVal = SetEvent(hEvent);
                    if (iRetVal == 0)
                        return FALSE;

                    if (hEvent)
                        CloseHandle(hEvent);

                    WaitForSingleObject(hRTThread, INFINITE);
                    break;
                }

                case SGIF_UPDATE_CONFIG:
                {

                    hEvent = CreateEvent(NULL, TRUE, FALSE, SGCONFIGUPDATE);
                    if (hEvent == NULL)
                        return FALSE;

                    wsprintf(wszConfigPath, L"%s\\%s\\ScreenGrabberConfig.conf", g_wszCWD, FP_SG_DIR);
                    err = _wsopen_s(&fp, wszConfigPath, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
                        _SH_DENYNO, _S_IREAD | _S_IWRITE);
                    if (err != 0)
                        return FALSE;

                    err = _write(fp, &MntrData->SGMData.SGMStructs.SGConfig, sizeof(SGCONFIG));
                    if (err == -1)
                        return FALSE;

                    err = _write(fp, &MntrData->SGMData.SGMStructs.SGRealApp, sizeof(SGREALAPP));
                    if (err == -1)
                        return FALSE;

                    if (fp)
                        _close(fp);

                    iRetVal = SetEvent(hEvent);
                    if (iRetVal == 0)
                        return FALSE;

                    CloseHandle(hEvent);

                    break;
                }
                break;
            }// switch(MntrData->SGMData.cmd)			
            break;
        }

        case IF_TERM_PLUGIN:
        {
            pRemoveSGHook = (REMOVEPROC)GetProcAddress(hSGPluginDll, "RemoveSGHook");
            if (pRemoveSGHook == NULL)
                return FALSE;
            pRemoveSGHook();
            FreeLibrary(hSGPluginDll);

            if (bRealTime == TRUE) {
                bRealTime = FALSE;
                WaitForSingleObject(hRTThread, INFINITE);
            }

            if (hEvent) {
                CloseHandle(hEvent);
                hEvent = NULL;
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

    }// switch(Cmd)

    return TRUE;
}


BOOL RetrieveSnapShots(const WCHAR *wszDirPath, const WCHAR *wszAppName,
    const SYSTEMTIME *stFromDate, const SYSTEMTIME *stToDate,
    SGFILEDATA *pSnapShotsList) {

    int	nFiles;
    int	iRetVal;

    WCHAR wszDayDirPath[MAX_PATH + 1] = L"";
    WCHAR wszAppDirPath[MAX_PATH + 1] = L"";
    WCHAR wszCurAppDirPath[MAX_PATH + 1] = L"";

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

    memset(pSnapShotsList, 0, sizeof(SGFILEDATA));
    nFiles = pSnapShotsList->nFiles = 0;

    // loop for scanning per day directories
    while (1) {
        // construct the search path for the day directory
        wsprintf(wszDayDirPath, L"%s\\%d-%d-%d", wszDirPath, wDate, wMonth, wYear);

        if (FindNextDayDir(wszDayDirPath) == TRUE) {
            while (1) {
                iRetVal = FindNextAppDir(wszDayDirPath, wszAppName, wszAppDirPath);
                if (iRetVal == TRUE) {
                    wcscpy_s(wszCurAppDirPath, MAX_PATH, wszDayDirPath);
                    wcscat_s(wszCurAppDirPath, MAX_PATH + 1, L"\\");
                    wcscat_s(wszCurAppDirPath, MAX_PATH + 1, wszAppDirPath);
                    FindFilesInAppDir(wszCurAppDirPath, pSnapShotsList);
                } else if (iRetVal == FALSE)
                    continue;
                else if (iRetVal == ERROR_NO_MORE_FILES)
                    break;
            }
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

}

BOOL FindNextDayDir(const WCHAR *wszPath) {
    HANDLE			hDayDir = NULL;
    WIN32_FIND_DATA	fdDirData;

    if ((hDayDir = FindFirstFile(wszPath, &fdDirData)) != INVALID_HANDLE_VALUE)
        return TRUE;

    return FALSE;
}

int FindNextAppDir(const WCHAR *wszPath, const WCHAR *wszEXEName, WCHAR __out *wszCurAppDir) {
    int	iRetVal;

    static BOOL		fFirstTime = TRUE;
    static HANDLE	hAppDir = NULL;
    WIN32_FIND_DATA	fdDirData;

    WCHAR wszDayDir[MAX_PATH + 1];
    WCHAR *pwsz = NULL;

    wsprintf(wszDayDir, L"%s\\*.*", wszPath);

    if (fFirstTime == TRUE) {
        if ((hAppDir = FindFirstFile(wszDayDir, &fdDirData)) == INVALID_HANDLE_VALUE) {
            iRetVal = GetLastError();
            fFirstTime = TRUE;
            return TRUE;
        } else {
            fFirstTime = FALSE;

            // determine whether it is the required directory or not
            if ((pwsz = wcsstr(fdDirData.cFileName, wszEXEName)) == NULL)
                return FALSE;
            else {
                // copy the directory name to the __out parameter
                wcscpy_s(wszCurAppDir, MAX_PATH, fdDirData.cFileName);
                return TRUE;
            }
        }
    } else {
        if (FindNextFile(hAppDir, &fdDirData) != 0) {
            // determine whether it is the required directory or not
            if ((pwsz = wcsstr(fdDirData.cFileName, wszEXEName)) == NULL)
                return FALSE;

            // copy the directory name to the __out parameter
            wcscpy_s(wszCurAppDir, MAX_PATH, fdDirData.cFileName);
            return TRUE;
        }
        if (GetLastError() == ERROR_NO_MORE_FILES) {
            fFirstTime = TRUE;
            return ERROR_NO_MORE_FILES;
        }
    }
    return FALSE;
}

BOOL FindFilesInAppDir(const WCHAR *wszPath, SGFILEDATA *pSnapShotsList) {
    WCHAR			wszFilePath[MAX_PATH + 1];
    HANDLE			hFile;
    BOOL			fNextFile;
    WIN32_FIND_DATA	fdFileData;
    int				nFiles = 0;

    // application directory found, now search for files
    wsprintf(wszFilePath, L"%s\\*.bmp", wszPath);

    // find the first .bmp file
    if ((hFile = FindFirstFile(wszFilePath, &fdFileData)) != INVALID_HANDLE_VALUE) {
        // fill FILE_TRANSFER_DATA
        wsprintf(pSnapShotsList->ListOfFiles[nFiles].wszFileName,
            L"%s\\%s", wszPath, fdFileData.cFileName);
        pSnapShotsList->nFiles += 1;
        ++nFiles;

        // search for more files
        while ((fNextFile = FindNextFile(hFile, &fdFileData))) {
            wsprintf(pSnapShotsList->ListOfFiles[nFiles].wszFileName,
                L"%s\\%s", wszPath, fdFileData.cFileName);
            pSnapShotsList->nFiles += 1;
            ++nFiles;
        }
    }

    if (nFiles > 0)
        return TRUE;

    return FALSE;
}

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

DWORD WINAPI TimerThread(LPVOID lpParam) {

    int nRetVal;
    DWORD dwRetVal;
    WCHAR wszRTShotsDir[MAX_PATH + 1];
    HANDLE		hIFDataReadyEvent = NULL;
    LARGE_INTEGER liDueTime;
    SGFILEDATA	*pSnapShots = NULL;


    liDueTime.QuadPart = -(nRTInterval * 10000);
    wsprintf(wszRTShotsDir, L"%s\\%s", g_wszCWD, FP_SG_RT_SHOTS_DIR);

    hTimer = CreateWaitableTimer(NULL, TRUE, L"RTSHOTSSEND");
    if (hTimer == NULL) {
        return FALSE;
    }

    nRetVal = SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, 0);
    if (nRetVal == 0) {
        if (hTimer != NULL)
            CloseHandle(hTimer);
        return FALSE;
    }

    __try {

        if ((pSnapShots = (SGFILEDATA*)malloc(sizeof(SGFILEDATA))) == NULL) {
            // WriteToLogFile()
            return 0;
        }
        memset(pSnapShots, 0, sizeof(SGFILEDATA));

        while (bRealTime) {
            dwRetVal = WaitForSingleObject(hTimer, nRTInterval);

            if (dwRetVal == WAIT_OBJECT_0) {
                pSnapShots->nFiles = 0;
                FindFilesInAppDir(wszRTShotsDir, pSnapShots);
                if ((pMyPData = (PDATA*)malloc(sizeof(PDATA))) == NULL)
                    return FALSE;
                memset(pMyPData, 0, sizeof(PDATA));

                // construct the PDATA structure and send it
                pMyPData->iMessage = SGM_RT_SHOT;
                pMyPData->PluginID = SCREENGRABBER;
                memcpy(&pMyPData->PDataType.SGFileData, pSnapShots, sizeof(SGFILEDATA));

                // set event so that CPM can call ProcessCmd() and take the PDATA

                // first, obtain handle to the event
                hIFDataReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_IF_DATA_READY);
                if (hIFDataReadyEvent == NULL) {
                    nRetVal = GetLastError();
                    return FALSE;
                } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
                    SetEvent(hIFDataReadyEvent);
                }
            }

            if (bRealTime) {
                nRetVal = SetWaitableTimer(hTimer, &liDueTime, 0, NULL, NULL, 0);
                if (nRetVal == 0) {
                    if (hTimer != NULL)
                        CloseHandle(hTimer);

                }
            }
        }

        if (hTimer != NULL)
            CloseHandle(hTimer);

        return TRUE;
    }
    __finally {
        if (pSnapShots) {
            free(pSnapShots);
            pSnapShots = NULL;
        }
    }
}