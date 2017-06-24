
#include "..\..\..\Include\ImageGrab.h"

// Handle of the dll
HINSTANCE hInstance;

// Flag to indicate that update thread of that particular application is exited
BOOL bExitUpdateConfigThread = FALSE;

// Handle of the UpdateConfigProc thread(hUpdateThread)
HANDLE hUpdateThread = NULL;

// Thread IDs of the InitHookProc and UpdateConfigProc threads
DWORD dwThreadID, dwUpdateID;

// Timer IDs of Offline timer and Realtime timer
UINT OfflineID = 0, RealTimeID = 0;

// Offline and Realtime timeinterval for taking snaps used to set the timers
UINT OfflineTime, RealTime;

// Handle of window to which timer is set
HWND hRealTimeWnd = NULL, hOfflineTimeWnd = NULL;

// Flag to indicate whether timer is set or not
BOOL bTimerSet = FALSE;

// Flag to indicate whether initialization is done or not
BOOL bInitSet = FALSE;

// Flag to indicate whether the present application should be monitored or not
BOOL bFilterApplication = FALSE;

// Flag to indicate whether application is real time monitored or not
BOOL bRealTime = FALSE;

// Applications .exe name in which dll is hooked
WCHAR wszProcessName[MAX_PATH + 1];

// Path of directory where the snaps are to be stored
WCHAR wszSubDirectory[MAX_PATH + 1];

// Path of the configuration file
WCHAR wszConfigFilePath[MAX_PATH + 1];

// Handle to the root window of the application
HWND hMainWnd = NULL;

// Handle of the Update Event
HANDLE hUpdateEvent = NULL;

// Creation time of the process
SYSTEMTIME stProcessCreateTime;

// Type of the shot to be taken for that application
int iShotType;


#pragma data_seg("SharedData")

// Handle to the MsgProc
HHOOK HookMsg = NULL;

// Handle to the CallWndProc 
HHOOK HookCallWnd = NULL;

// Flag indicating whether dll is hooked to all the applications or not
BOOL bHookFlag = FALSE;

// Path of the working directory ie path of the ClientRM
WCHAR wszSGDirectory[MAX_PATH + 1] = { 0 };

// Flag indicating update thread of every application should be killed 
BOOL bExitAllUpdateConfigThread = FALSE;

#pragma data_seg()

#pragma comment (linker,"/SECTION:SharedData,RWS")




BOOL WINAPI DllMain(HINSTANCE hInst, DWORD nReasons, LPVOID Reserved) {
    switch (nReasons) {

        case DLL_PROCESS_ATTACH:
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_PROCESS_DETACH:
        {
            // Set true while application is closed telling update thread should be closed
            bExitUpdateConfigThread = TRUE;

            // Setting the event so that updateconfig thread stops waiting for the event
            if (hUpdateEvent != NULL) {
                SetEvent(hUpdateEvent);
                Sleep(100);
            }

            if (hUpdateEvent)
                CloseHandle(hUpdateEvent);
            if (hUpdateThread)
                CloseHandle(hUpdateThread);
        }
        break;

        case DLL_THREAD_DETACH:
            break;

    }

    hInstance = hInst;
    return TRUE;
}

BOOL InstallSGHook(WCHAR *pszSGDir) {
    // Setting the present working directory of the ClientRM
    wsprintf(wszSGDirectory, L"%s", pszSGDir);

    HookCallWnd = SetWindowsHookEx(WH_CALLWNDPROC, (HOOKPROC)SGCallWndProc, hInstance, 0);
    HookMsg = SetWindowsHookEx(WH_GETMESSAGE, (HOOKPROC)SGGetMsgProc, hInstance, 0);

    if (HookMsg == NULL || HookCallWnd == NULL)
        return FALSE;

    bHookFlag = TRUE;
    return TRUE;
}



int RemoveSGHook() {
    if (bHookFlag == FALSE)
        return(FALSE);

    // Flag is set indicating that update thread should be closed in all application
    bExitAllUpdateConfigThread = TRUE;

    // Setting event so that any update thread waiting for the event should be closed
    SetEvent(hUpdateEvent);

    // Wait untill update threads are all closed
    WaitForSingleObject(hUpdateThread, INFINITE);

    if (HookMsg != NULL) {
        UnhookWindowsHookEx(HookMsg);
        HookMsg = NULL;
    }

    if (HookCallWnd != NULL) {
        UnhookWindowsHookEx(HookCallWnd);
        HookCallWnd = NULL;
    }

    bHookFlag = FALSE;

    // Unhooking is delayed untill all applications close the update thread
    Sleep(5000);

    return TRUE;
}


LRESULT CALLBACK SGCallWndProc(int nCode, WPARAM wParam, LPARAM lParam) {
    CWPSTRUCT *cwpstruct = (CWPSTRUCT *)lParam;
    int xPos = 0, yPos = 0;

    if (nCode < 0) {
        return CallNextHookEx(HookCallWnd, nCode, wParam, lParam);
    }

    if (nCode == HC_ACTION) {
        switch (cwpstruct->message) {
            case WM_SETFOCUS:
            case WM_NCPAINT:
            case WM_PAINT:
            {
                // This is used in full screen mode for setting the timer to get the root window (not null)
                hMainWnd = cwpstruct->hwnd;
            }
            break;
            case WM_ACTIVATEAPP:
            {
                if (ProcessThisApp() == FALSE)
                    break;

                if (iShotType == ApplicationShot) {

                    ProcessClick(xPos, yPos, (HWND)cwpstruct->hwnd);
                }
            }
            break;
        }
    }
    return CallNextHookEx(HookCallWnd, nCode, wParam, lParam);
}



LRESULT CALLBACK SGGetMsgProc(int nCode, WPARAM wParam, LPARAM lParam) {
    MSG *msg = (MSG *)lParam;
    int xPos = 0, yPos = 0;

    if (nCode < 0) {
        return CallNextHookEx(HookMsg, nCode, wParam, lParam);
    }

    if (nCode == HC_ACTION) {

        switch (msg->message) {
            case WM_LBUTTONDOWN:
            {
                if (ProcessThisApp() == FALSE)
                    break;

                // In case of full screen shot click is used for initialization of timer
                if (iShotType == FullscreenShot && bTimerSet == TRUE)
                    break;

                xPos = msg->pt.x;
                yPos = msg->pt.y;

                ProcessClick(xPos, yPos, msg->hwnd);
                break;
            }
            case WM_TIMER:
            {
                if (ProcessThisApp() == FALSE)
                    break;

                if (iShotType != FullscreenShot)
                    break;

                // Checking whether WM_TIMER message which we recieved is what we have set in timers
                if (msg->wParam != IDT_OFFLINE_TIMER && msg->wParam != IDT_REALTIME_TIMER)
                    break;

                //__asm int 3
                ProcessClick(0, 0, msg->hwnd);
                return(TRUE);
            }
        }
    }

    return CallNextHookEx(HookMsg, nCode, wParam, lParam);
}


// Used to Check whether the application is in the configuration ie application to be monitored or not
BOOL ProcessThisApp() {
    // This condition execute only first time to create a 
    // thread in each process for loading configration
    if (bInitSet != TRUE) {
        bInitSet = TRUE;
        if (InitProcess() == FALSE)
            bInitSet = FALSE;
    }

    // Returning the flag which indicates whether application should be monitored or not
    return(bFilterApplication);
}


// This function is used for making sure that messages of certain applications are never processed
BOOL NeverHookProcessList() {
    WCHAR wszFilePath[MAX_PATH + 1];
    WCHAR *pDestFile = NULL;
    int nRetPath;

    // List of application to which hooking should not be done
    WCHAR *pIgnoreList[10] = { L"devenv.exe", L"serverrm.exe", L"clientrm.exe", NULL };

    // Getting the .exe file path of the application
    nRetPath = GetModuleFileName(NULL, wszFilePath, MAX_PATH);
    if (nRetPath == 0) {
        return FALSE;
    }

    // Converts all the characters in the path name to smaller case
    _wcslwr_s(wszFilePath, MAX_PATH);

    for (int Index = 0; pIgnoreList[Index] != NULL; Index++) {
        // Comparing file path with the each of the application in ignore list
        pDestFile = wcsstr(wszFilePath, pIgnoreList[Index]);
        if (pDestFile)
            return(TRUE);
    }

    return(FALSE);
}

// Used to create update thread and also call InitHook functions which sets flags and variables

BOOL InitProcess() {
    int nRetVal = 0;

    // Checking whether application should be ignored?
    if (NeverHookProcessList())
        return(TRUE);

    nRetVal = InitHook();
    if (nRetVal == 0)
        return FALSE;

    // Create a thread which waits for the UpdateConfig Event
    hUpdateThread = CreateThread(NULL, 0, UpdateConfigProc, NULL, 0, &dwUpdateID);
    if (hUpdateThread == NULL)
        return FALSE;

    return TRUE;
}


// Used to set all the flags and store all process related variables

BOOL InitHook() {

    SYSTEMTIME stSystemTime;

    // Used to get the process creation time 
    FILETIME fp1, fp2, fp3, fp4, fp5;

    WCHAR wszFilePath[MAX_PATH + 1];
    WCHAR *pDest = NULL;

    int i, nRetPath, nRetVal;

    SGCONFIG SGConfig;
    SGREALAPP SGRealApp;

    // Flag indicating that present application name has been found in configuration file																						
    BOOL bProcessFound = FALSE;

    // File descriptor used to reference the configuration file
    int fd = 0;

    // Obtain the error codes in case of exceptions
    errno_t err;

    __try {

        // Getting the .exe file path of the application
        nRetPath = GetModuleFileName(NULL, wszFilePath, MAX_PATH);
        if (nRetPath == 0) {
            return FALSE;
        }

        // Converting the characters in the File path to smaller case
        _wcslwr_s(wszFilePath, MAX_PATH);

        // Constructing the configuration file path
        wsprintf(wszConfigFilePath, L"%s\\%s\\ScreenGrabberConfig.conf", wszSGDirectory, FP_SG_DIR);

        // Open the Configuration file in read mode
        err = _wsopen_s(&fd, wszConfigFilePath, _O_RDONLY | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
        if (fd == -1) {
            return FALSE;
        }

        nRetVal = _read(fd, &SGConfig, sizeof(SGCONFIG));
        if (nRetVal <= 0)
            return FALSE;

        for (i = 0;i < SGConfig.nApps;i++) {

            // Comparing exe name in the configuration file with the file path
            pDest = wcsstr(wszFilePath, SGConfig.SGAppList[i].wszEXEName);
            if (pDest == NULL)
                continue;

            GetLocalTime(&stSystemTime);

            // Gets the process creation time of the present process
            nRetVal = GetProcessTimes(GetCurrentProcess(), &fp1, &fp2, &fp3, &fp4);
            if (nRetVal == 0)
                return FALSE;

            nRetVal = FileTimeToLocalFileTime(&fp1, &fp5);
            if (nRetVal == 0)
                return FALSE;

            nRetVal = FileTimeToSystemTime(&fp5, &stProcessCreateTime);
            if (nRetVal == 0)
                return FALSE;


            StrCpy(wszProcessName, SGConfig.SGAppList[i].wszEXEName);
            iShotType = SGConfig.SGAppList[i].iShotType;
            OfflineTime = SGConfig.ulOffInterval;
            RealTime = SGConfig.ulRTInterval;

            // Flag is to indicate that application is to be monitored in future
            bFilterApplication = TRUE;

            // Flag is set to indicate process is found in the configuration file
            bProcessFound = TRUE;

            // construction of the subdirectory path
            wsprintf(wszSubDirectory, L"%s\\%s\\%d-%d-%d\\%s%02d-%02d-%02d-%03d",
                wszSGDirectory, FP_SG_SHOTS_DIR,
                stSystemTime.wDay, stSystemTime.wMonth, stSystemTime.wYear,
                wszProcessName,
                stProcessCreateTime.wHour, stProcessCreateTime.wMinute, stProcessCreateTime.wSecond, stProcessCreateTime.wMilliseconds);


            if ((nRetVal = CreateDirectory(wszSubDirectory, NULL)) == 0) {
                nRetVal = GetLastError();
            }

            break;
        }

        // If process is not found in the earlier structure do not read further configuration file
        if (bProcessFound != TRUE)
            return(FALSE);


        // Read the structure containing information of application to be monitored in real time
        nRetVal = _read(fd, &SGRealApp, sizeof(SGREALAPP));
        if (nRetVal <= 0)
            return FALSE;

        nRetVal = StrCmp(wszProcessName, SGRealApp.wszEXEName);
        if (nRetVal == 0) {
            bRealTime = TRUE;
            nRetVal = SetTimerFunc();
        }
        return TRUE;
    }
    __finally {
        // Close the configuration file
        if (fd != -1)
            _close(fd);

    }

}



// Function used to set the timer both for real time and offline
BOOL SetTimerFunc() {
    DWORD Errorno;
    HWND hTempWnd = NULL;

    // Timer is set only for the full screen mode, also  
    // If timer is previously is set then donot set the timer again,
    // Window handle to which timer is being set should not be null
    if (iShotType != FullscreenShot || bTimerSet == TRUE || hMainWnd == NULL)
        return(FALSE);

    // Getting the toplevel window of the application
    hTempWnd = GetAncestor(hMainWnd, GA_ROOT);

    if (bRealTime == TRUE) {
        RealTimeID = SetTimer(hTempWnd, IDT_REALTIME_TIMER, RealTime, (TIMERPROC)NULL);
        Errorno = GetLastError();
        if (RealTimeID == 0)
            return FALSE;

        // This handle is used for the killing of timer
        hRealTimeWnd = hTempWnd;
    } else {
        OfflineID = SetTimer(hTempWnd, IDT_OFFLINE_TIMER, OfflineTime, (TIMERPROC)NULL);
        Errorno = GetLastError();
        if (OfflineID == 0)
            return FALSE;

        // This handle is used for the killing of timer
        hOfflineTimeWnd = hTempWnd;
    }

    // Flag indicating timer is set
    bTimerSet = TRUE;

    return TRUE;
}


// Function used to kill the timer
BOOL KillTimerFunc() {
    int nRetVal;

    // kill the timer only if it is already been set, also timer is only set for the full screen only
    // Also root window should not be null
    if (iShotType != FullscreenShot || bTimerSet == FALSE || hMainWnd == NULL)
        return(FALSE);

    if (bRealTime != TRUE) {
        // kill the offline timer if application is monitored offline previously
        nRetVal = KillTimer(hOfflineTimeWnd, IDT_OFFLINE_TIMER);
        if (nRetVal == 0)
            return FALSE;
    } else {
        // kill the real time timer if application is monitored offline previouly
        nRetVal = KillTimer(hRealTimeWnd, IDT_REALTIME_TIMER);
        if (nRetVal == 0)
            return FALSE;
    }

    // Finally after killing timers set the flag indicating that 'no timer is set'
    bTimerSet = FALSE;

    return TRUE;
}


// Thread which creates a Event, used to update the configuration, Event is set by ImgGrbIt.cpp(CPM)
DWORD WINAPI UpdateConfigProc(LPVOID lpParam) {
    DWORD dwRetVal = 0;

    // Creating the event which is set by the imggrbit once the configuration file changes
    hUpdateEvent = CreateEvent(NULL, TRUE, FALSE, SGCONFIGUPDATE);
    if (!hUpdateEvent) {
        return FALSE;
    }

    // Check whether thread should be killed either because of closing of application or due to removehook
    while (bExitUpdateConfigThread != TRUE && bExitAllUpdateConfigThread != TRUE) {

        // Wait till event is generated 
        dwRetVal = WaitForSingleObject(hUpdateEvent, 1000);

        // Check whether event generated is of set event (not the timeout or abandoned)
        if (bExitUpdateConfigThread != TRUE && bExitAllUpdateConfigThread != TRUE && dwRetVal == WAIT_OBJECT_0) {

            // If configuration is changed then do the necessary changes to that process 
            UpdateConfig();
            ResetEvent(hUpdateEvent);
        }
    }

    return(TRUE);
}

// Used to do the necessary updation based on new configuration
BOOL UpdateConfig() {
    int i, nRetVal;

    SYSTEMTIME stSystemTime;
    FILETIME fp1, fp2, fp3, fp4, fp5;

    WCHAR wszFilePath[MAX_PATH + 1];
    WCHAR *pDest = NULL;

    SGCONFIG SGConfig;
    SGREALAPP SGRealApp;

    int fd;
    BOOL bProcessFound = FALSE;
    BOOL bAppWasFiltered = FALSE;

    errno_t err;
    __try {

        // Reseting all the Flags which ever set by the InitHook function, also kill the timers
        bInitSet = FALSE;

        if (bFilterApplication == TRUE)
            bAppWasFiltered = TRUE;

        bFilterApplication = FALSE;
        //nRetVal = KillTimerFunc();
        //bRealTime = FALSE;

        nRetVal = GetModuleFileName(NULL, wszFilePath, MAX_PATH);
        if (nRetVal == 0) {
            return FALSE;
        }

        // Open the Configuration file in read mode
        err = _wsopen_s(&fd, wszConfigFilePath, _O_RDONLY | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
        if (fd == -1) {
            return FALSE;
        }

        // Read the first structure in the configuration file
        nRetVal = _read(fd, &SGConfig, sizeof(SGCONFIG));
        if (nRetVal <= 0)
            return FALSE;

        for (i = 0;i < SGConfig.nApps;i++) {
            pDest = wcsstr(wszFilePath, SGConfig.SGAppList[i].wszEXEName);
            if (pDest == NULL)
                continue;
            if (bAppWasFiltered == FALSE) {
                GetLocalTime(&stSystemTime);

                // Gets the process creation time of the present process
                nRetVal = GetProcessTimes(GetCurrentProcess(), &fp1, &fp2, &fp3, &fp4);
                if (nRetVal == 0)
                    return FALSE;

                nRetVal = FileTimeToLocalFileTime(&fp1, &fp5);
                if (nRetVal == 0)
                    return FALSE;

                nRetVal = FileTimeToSystemTime(&fp5, &stProcessCreateTime);
                if (nRetVal == 0)
                    return FALSE;


                StrCpy(wszProcessName, SGConfig.SGAppList[i].wszEXEName);

                // construction of the subdirectory path
                swprintf_s(wszSubDirectory, MAX_PATH + 1, L"%s\\%s\\%d-%d-%d\\%s%02d-%02d-%02d-%03d",
                    wszSGDirectory, FP_SG_SHOTS_DIR,
                    stSystemTime.wDay, stSystemTime.wMonth, stSystemTime.wYear,
                    wszProcessName,
                    stProcessCreateTime.wHour, stProcessCreateTime.wMinute, stProcessCreateTime.wSecond, stProcessCreateTime.wMilliseconds);

                if ((nRetVal = CreateDirectory(wszSubDirectory, NULL)) == 0) {
                    nRetVal = GetLastError();
                }
            }

            iShotType = SGConfig.SGAppList[i].iShotType;
            OfflineTime = SGConfig.ulOffInterval;
            RealTime = SGConfig.ulRTInterval;

            // Set the flag indicating that application should be monitored
            bFilterApplication = TRUE;

            // Flag indicating that Present process name is found in the configuration file
            bProcessFound = TRUE;

            break;
        }

        // If process is not found in the configuration file then just return ie stop reading the configuration file
        if (bProcessFound != TRUE) {
            nRetVal = KillTimerFunc();
            bRealTime = FALSE;

            bFilterApplication = FALSE;
            return(FALSE);
        }

        // Read the structure indicating the real time application
        nRetVal = _read(fd, &SGRealApp, sizeof(SGREALAPP));
        if (nRetVal <= 0)
            return FALSE;

        nRetVal = StrCmp(wszProcessName, SGRealApp.wszEXEName);
        if (nRetVal == 0) {
            if (bRealTime != TRUE) {

                nRetVal = KillTimerFunc();
            }
            // Set the real time timer if real time application name matches with the process name
            bRealTime = TRUE;
            nRetVal = SetTimerFunc();
        } else {
            if (bRealTime == TRUE) {
                nRetVal = KillTimerFunc();
            }
            bRealTime = FALSE;
            // Set the offline timer if the application is not monitored in real time
            nRetVal = SetTimerFunc();
        }

        bInitSet = TRUE;
        return TRUE;
    }
    __finally {
        // Close the configuration file
        if (fd != -1)
            _close(fd);

    }
}


// This function is used to do the processing based on the shot type like 
// Setting the name of bmp file, snaps width and height and other parameters are set here
BOOL ProcessClick(int xPos, int yPos, HWND hwnd) {
    long ShotWidth = 0, ShotHeight = 0, xStartPt = 0, yStartPt = 0, ScreenWidth = 0, ScreenHeight = 0;

    SYSTEMTIME stSystemTime;

    // Used to store the file name (path of the bmp flie)
    WCHAR wszFileName[MAX_PATH + 1];

    int nRetVal = 0;
    HWND hTempWnd = NULL;

    GetLocalTime(&stSystemTime);

    // Used to get the desktop resolution ie total screens width and height
    ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    ScreenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Used to get the handle of toplevel window from the window where click happened
    hTempWnd = GetAncestor(hwnd, GA_ROOT);

    if (iShotType == ApplicationShot) {
        RECT Rect;
        if (bRealTime == FALSE) {
            // Construct the path of the file if it is the real time application
            wsprintf(wszFileName, L"%s\\%02d-%02d-%02d-%03d.bmp",
                wszSubDirectory,
                stSystemTime.wHour, stSystemTime.wMinute, stSystemTime.wSecond, stSystemTime.wMilliseconds);
        } else {
            // Construct the path of the file if it is offline application
            wsprintf(wszFileName, L"%s\\%s\\%02d-%02d-%02d-%03d.bmp",
                wszSGDirectory,
                FP_SG_RT_SHOTS_DIR,
                stSystemTime.wHour, stSystemTime.wMinute, stSystemTime.wSecond, stSystemTime.wMilliseconds);
        }

        // Get the coordinates of the applications window where click happened
        nRetVal = GetWindowRect(hTempWnd, &Rect);
        if (nRetVal == 0)
            return FALSE;

        ShotWidth = Rect.right - Rect.left;
        ShotHeight = Rect.bottom - Rect.top;

        // If shot width is greater than the screen width then clip the shot to screens width
        if (ShotWidth > ScreenWidth)
            ShotWidth = ScreenWidth;

        // If shot height is greater than the screen height then clip the shot to screens height
        if (ShotHeight > ScreenHeight)
            ShotHeight = ScreenHeight;

        xStartPt = 0;
        yStartPt = 0;

        TakeSnap(hTempWnd, ScreenWidth, ScreenHeight, ShotWidth, ShotHeight, xStartPt, yStartPt, wszFileName);
    }

    else if (iShotType == MouseShot) {

        if (bRealTime == FALSE) {
            // Construct the path of the file if it is the real time application
            wsprintf(wszFileName, L"%s\\%02d-%02d-%02d-%03d.bmp",
                wszSubDirectory,
                stSystemTime.wHour, stSystemTime.wMinute, stSystemTime.wSecond, stSystemTime.wMilliseconds);
        } else {
            // Construct the path of the file if it is the offline application
            wsprintf(wszFileName, L"%s\\%s\\%02d-%02d-%02d-%03d.bmp",
                wszSGDirectory,
                FP_SG_RT_SHOTS_DIR,
                stSystemTime.wHour, stSystemTime.wMinute, stSystemTime.wSecond, stSystemTime.wMilliseconds);
        }

        // Calculate the starting point from where the shot has to be taken
        xStartPt = xPos - MOUSE_SHOT_ADJUST;
        yStartPt = yPos - MOUSE_SHOT_ADJUST;

        if (xStartPt < 0)
            xStartPt = 0;

        if (yStartPt < 0)
            yStartPt = 0;

        ShotWidth = MOUSE_SHOT_AREA;
        ShotHeight = MOUSE_SHOT_AREA;

        // If Shot width or height is greater than screen width or height then clip to the necearry length
        if (xStartPt + ShotWidth > ScreenWidth)
            ShotWidth = ScreenWidth - xStartPt;

        if (yStartPt + ShotHeight > ScreenHeight)
            ShotHeight = ScreenHeight - yStartPt;


        TakeSnap(NULL, ScreenWidth, ScreenHeight, ShotWidth, ShotHeight, xStartPt, yStartPt, wszFileName);
    }

    else if (iShotType == FullscreenShot) {
        // Set the timer of the full screen shot
        nRetVal = SetTimerFunc();

        if (bRealTime == FALSE) {
            // Construct the path of file if it is a real time application
            wsprintf(wszFileName, L"%s\\%02d-%02d-%02d-%03d.bmp",
                wszSubDirectory,
                stSystemTime.wHour, stSystemTime.wMinute, stSystemTime.wSecond, stSystemTime.wMilliseconds);
        } else {
            // Construct the path of file if it is a offline application
            wsprintf(wszFileName, L"%s\\%s\\%02d-%02d-%02d-%03d.bmp",
                wszSGDirectory,
                FP_SG_RT_SHOTS_DIR,
                stSystemTime.wHour, stSystemTime.wMinute, stSystemTime.wSecond, stSystemTime.wMilliseconds);
        }

        ShotWidth = ScreenWidth;
        ShotHeight = ScreenHeight;

        xStartPt = 0;
        yStartPt = 0;

        TakeSnap(NULL, ScreenWidth, ScreenHeight, ShotWidth, ShotHeight, xStartPt, yStartPt, wszFileName);
    }


    return TRUE;
}

// Used to take the snap for any type of shot
BOOL TakeSnap(HWND hWnd, long ScreenWidth, long ScreenHeight, long ShotWidth, long ShotHeight, long xStartPt, long yStartPt, WCHAR wszFileName[MAX_PATH + 1]) {
    DWORD dwRetVal = 0;

    HDC hDesktopDC = NULL, hBitmapDC = NULL;
    HBITMAP hOriginal = NULL;
    HBITMAP hBitmap = NULL;
    HANDLE hFile = NULL, hDIB = NULL;

    BITMAP bmpScreen;
    BITMAPINFOHEADER biHeader;
    BITMAPFILEHEADER bmfHeader;


    __try {

        // Used to obtain the device context of the window pointed by hwnd
        hDesktopDC = GetWindowDC(hWnd);
        if (hDesktopDC == NULL)
            return FALSE;

        // Used to create a bitmap compatible with the device that is associated with the specified device context
        hBitmap = CreateCompatibleBitmap(hDesktopDC, ShotWidth, ShotHeight);
        if (hBitmap == NULL)
            return FALSE;

        // Used to create a memory device context (DC) compatible with the specified device.
        hBitmapDC = CreateCompatibleDC(hDesktopDC);
        if (hBitmapDC == NULL)
            return FALSE;

        // Used to select an object into the specified device context (DC). The new object replaces the previous object of the same type.
        hOriginal = (HBITMAP)SelectBitmap(hBitmapDC, hBitmap);
        if (hOriginal == NULL)
            return FALSE;

        // Used to perform a bit-block transfer of the color data corresponding
        // to a rectangle of pixels from the hDesktopDC device context into hBitmapDC.
        dwRetVal = BitBlt(hBitmapDC, 0, 0, ShotWidth, ShotHeight, hDesktopDC, xStartPt, yStartPt, SRCCOPY);
        if (dwRetVal == 0)
            return FALSE;

        // Used to retrieve information for the specified graphics object ie hBitmap
        dwRetVal = GetObject(hBitmap, sizeof(BITMAP), &bmpScreen);
        if (dwRetVal == 0)
            return FALSE;

        // Setting the values of the BITMAPINFOHEADER file used to create the bitmap file
        biHeader.biSize = sizeof(BITMAPINFOHEADER);
        biHeader.biWidth = ShotWidth;
        biHeader.biHeight = ShotHeight;
        biHeader.biPlanes = 1;
        biHeader.biBitCount = 32;
        biHeader.biCompression = BI_RGB;
        biHeader.biSizeImage = 0;
        biHeader.biXPelsPerMeter = 0;
        biHeader.biYPelsPerMeter = 0;
        biHeader.biClrUsed = 0;
        biHeader.biClrImportant = 0;

        // Calculating the size of the bitmap file
        DWORD dwBmpSize = ((bmpScreen.bmWidth * biHeader.biBitCount + 31) / 32) * 4 * bmpScreen.bmHeight;

        // Allocating the memory for storing the pixel values
        hDIB = GlobalAlloc(GHND, dwBmpSize);
        if (hDIB == NULL)
            return FALSE;

        // Obtaining the lock for the previously allocated memory
        char *lpBitmap = (char *)GlobalLock(hDIB);
        if (lpBitmap == NULL)
            return FALSE;

        // This function retrieves the bits of the specified compatible bitmap(hBitmap) 
        // and copies them into a buffer(biHeader) as a DIB using the specified format(DIB_RGB_COLORS).
        dwRetVal = GetDIBits(hBitmapDC, hBitmap, 0, (UINT)bmpScreen.bmHeight, lpBitmap, (BITMAPINFO *)&biHeader, DIB_RGB_COLORS);
        if (dwRetVal == 0)
            return FALSE;

        hFile = CreateFile(wszFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == NULL) {
            dwRetVal = GetLastError();
            return FALSE;
        }

        // Calculating the size of the info part of the bitmap
        DWORD dwSizeofDIB = dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

        bmfHeader.bfOffBits = (DWORD)sizeof(BITMAPFILEHEADER) + (DWORD)sizeof(BITMAPINFOHEADER);
        bmfHeader.bfSize = dwSizeofDIB;
        bmfHeader.bfType = 0x4D42;
        DWORD dwBytesWritten = 0;

        // Writing the bitmap file header to bmp file
        dwRetVal = WriteFile(hFile, (LPSTR)&bmfHeader, sizeof(BITMAPFILEHEADER), &dwBytesWritten, NULL);
        if (dwRetVal == 0)
            return FALSE;

        // Writing the bitmap info header to bmp file
        dwRetVal = WriteFile(hFile, (LPSTR)&biHeader, sizeof(BITMAPINFOHEADER), &dwBytesWritten, NULL);
        if (dwRetVal == 0)
            return FALSE;

        // Writing actual pixels data to the bmp file 
        dwRetVal = WriteFile(hFile, (LPSTR)lpBitmap, dwBmpSize, &dwBytesWritten, NULL);
        if (dwRetVal == 0)
            return FALSE;

        return TRUE;

    }
    __finally {

        if (hDIB != NULL) {
            GlobalUnlock(hDIB);
            GlobalFree(hDIB);
        }

        if (hFile != NULL)
            dwRetVal = CloseHandle(hFile);

        if (hBitmapDC != NULL)
            DeleteDC(hBitmapDC);

        if (hDesktopDC != NULL)
            ReleaseDC(HWND_DESKTOP, hDesktopDC);

    }
}
