
// * ServerRM Project *
// WndProc.cpp: Window procedure function to handle messages sent to the server main window.

#include "ServerInclude.h"

extern HWND hMsgOnlyWnd;

// Global variables
HMENU hMainMenu;
HWND hListBox;
HWND hStatusBar;
HWND hClientConfListBox;

PLUGINDB g_PluginDB;
PMONITORS g_PMonitors;
RMSERVERCONF g_RMServerConf;

WCHAR g_wszCWD[MAX_PATH + 1];

// Private Function Prototypes
static DWORD WINAPI		InitThread(LPVOID lpParam);
static BOOL				Cleanup();

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {

    extern HINSTANCE hMainInstance;
    HANDLE hInitThread = NULL;

    RECT rClientArea;
    int aPartsWidth[SB_NPARTS];
    int nWidth;

    int iRetVal;

    switch (message) {
        case WM_CREATE:
        {
            FILE *fp = NULL;
            WCHAR wszDate[64];
            SYSTEMTIME stToday;

            // check whether log file exists
            if ((iRetVal = _wfopen_s(&fp, FP_SERVER_LOGFILE, L"r")) != 0) {
                // create a new file
                if ((iRetVal = _wfopen_s(&fp, FP_SERVER_LOGFILE, L"w, ccs=UTF-8")) != 0) {
                    // set flag in ServerRM status structure (code yet to be written)
                } else {
                    fclose(fp);
                    fp = NULL;
                    WriteToLogFile(FP_SERVER_LOGFILE, NULL, NULL, NULL, 0); // write headers
                }
            } else {
                fclose(fp);
                fp = NULL;
            }
            GetLocalTime(&stToday);
            swprintf_s(wszDate, 64, L"%d/%d/%d ServerRM started", stToday.wDay, stToday.wMonth, stToday.wYear);
            WriteToLogFile(FP_SERVER_LOGFILE, L"Main", L"WndProc", wszDate, 0);

            hMainMenu = GetMenu(hWnd);

            // Create StatusBar
            hStatusBar = CreateWindowEx(
                0,
                STATUSCLASSNAME,
                NULL,
                WS_CHILD | SBARS_SIZEGRIP | WS_VISIBLE,
                0, 0, 0, 0,
                hWnd,
                NULL,
                hMainInstance,
                NULL);

            if (!hStatusBar) {
                WriteToLogFile(FP_SERVER_LOGFILE, L"Main", L"WndProc", L"Failed to create Status Bar", GetLastError());
                MessageBox(NULL, L"Status Bar not created!", NULL, MB_OK);
                break;
            }

            // divide the status bar into 3 parts
            iRetVal = GetClientRect(hWnd, (LPRECT)&rClientArea);
            nWidth = rClientArea.right / SB_NPARTS;
            for (int i = 0; i < SB_NPARTS; ++i) {
                aPartsWidth[i] = nWidth;
                nWidth += nWidth;
            }

            // Tell the status bar to create the window parts.
            SendMessage(hStatusBar, SB_SETPARTS, (WPARAM)SB_NPARTS, (LPARAM)aPartsWidth);

            // Create Status Window, which is a ListBox
            hListBox = CreateWindowEx(
                0,
                L"listbox",
                L"ServerRM_StatusWnd",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOSEL,
                10,							// x pos
                rClientArea.bottom - 225,	// y pos
                rClientArea.right - 25,		// x size
                200,						// y size
                hWnd,
                NULL,
                hMainInstance,
                NULL);
            if (!hListBox) {
                WriteToLogFile(FP_SERVER_LOGFILE, L"Main", L"WndProc", L"Failed to create Status Window", GetLastError());
                MessageBox(NULL, L"ListBox not created", 0, 0);
                break;
            }

            // create the client configuration display listbox
            hClientConfListBox = CreateWindowEx(
                0,
                L"listbox",
                L"ServerRM_ClientConfWnd",
                WS_CHILD | WS_BORDER | WS_VSCROLL | LBS_NOSEL,
                10,							// x pos
                10,							// y pos
                rClientArea.right - 25,		// x size
                200,						// y size
                hWnd,
                NULL,
                hMainInstance,
                NULL);
            if (!hClientConfListBox) {
                WriteToLogFile(FP_SERVER_LOGFILE, L"Main", L"WndProc", L"Failed to create ClientConf Window", GetLastError());
                MessageBox(NULL, L"ClientConf ListBox not created", 0, 0);
                break;
            }

            // add line to status window
            iRetVal = PrintToStatusWindow(L"ServerRM UI started");

            // add line to status bar
            PrintToStatusBar(L"Disconnected", 0);

            // init thread
            // after initialization of the UI, call InitThread() to perform other initialization operations.
            hInitThread = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)InitThread, hWnd, 0, NULL);
            if (hInitThread == NULL)
                MessageBox(hWnd, L"Thread error", 0, 0);

            return 0;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                case IDM_CLIENTDB_ADD:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_ADD_CLIENT),
                        hWnd, AddCliDP);
                    return 0;
                }


                case IDM_CLIENTDB_REMOVE:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_REM_CLIENT),
                        hWnd, RemCliDP);
                    return 0;
                }


                case IDM_CLIENTDB_VIEWCLIENTS:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_VIEW_CLIENTS),
                        hWnd, ViewCliDP);
                    return 0;
                }


                case IDM_PLUGINDB_ADD:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_ADD_PLUGIN),
                        hWnd, AddPluginDP);
                    return 0;
                }


                case IDM_PLUGINDB_REMOVE:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_REM_PLUGIN),
                        hWnd, RemPluginDP);
                    return 0;
                }

                case IDM_PLUGINDB_VIEWPLUGINS:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_VIEW_PLUGINS),
                        hWnd, ViewPluginDP);
                    return 0;
                }


                case IDM_SERVERCONFIG_EXIT:
                {
                    //disconnect from client, if connected
                    ServerPluginManager(SPM_STOPSESSION, NULL, NULL);
                    SendMessage(hWnd, WM_CLOSE, 0, 0);
                    return 0;
                }


                case IDM_MONITOR_SCAN_IP:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_SCAN_IP_ADDR),
                        hWnd, ScanIPAddrDP);
                    if (iRetVal == IDB_SCAN_IP_ADDR_SCAN) {
                        // disable the Connect and Scan IP Addresses menu items once user starts the Scan IP Addresses dialog box
                        EnableMenuItem(hMainMenu, IDM_MONITOR_CONNECT, MF_GRAYED);
                        EnableMenuItem(hMainMenu, IDM_MONITOR_SCAN_IP, MF_GRAYED);
                    }
                    return 0;
                }

                case IDM_MONITOR_CONNECT:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_CONNECT_TO),
                        hWnd, ConnectToDP);
                    return 0;
                }


                case IDM_MONITOR_INSTALLPLUGIN:
                {

                    return 0;
                }


                case IDM_MONITOR_UNINSTALLPLUGIN:
                {

                    return 0;
                }


                case IDM_MONITOR_DISCONNECT:
                {
                    ServerPluginManager(SPM_STOPSESSION, NULL, NULL);
                    return 0;
                }


                case IDM_LAUNCHPLUGIN_KLM:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_KLM),
                        hWnd, KeylogMonDP);
                    return 0;
                }


                case IDM_LAUNCHPLUGIN_SGM:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_SGM),
                        hWnd, SGMonDP);
                    return 0;
                }

                case IDM_LAUNCHPLUGIN_FBM:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_FBM),
                        hWnd, FBMonDP);
                    return 0;
                }

                case IDM_HELP_CONTENTS:
                {

                    return 0;
                }

                case IDM_HELP_ABOUT:
                {
                    iRetVal = DialogBox(hMainInstance, MAKEINTRESOURCE(IDD_ABOUT), hWnd, AboutBoxDP);
                    return 0;
                }

            }// switch(LOWORD(wParam))
            break;
        }

        case WM_START_SCAN_RESULTS:
        {
            // lParam contains the address of the ScanResults structure which holds information about
            // the active clients found during the scan of IPAddresses.
            iRetVal = DialogBoxParam(hMainInstance, MAKEINTRESOURCE(IDD_SCAN_IP_RESULT), hWnd, ScanResultsDP,
                lParam);

            // enable the Connect and Scan IP Addresses menu items once the user closes the Scan Results dialog box
            EnableMenuItem(hMainMenu, IDM_MONITOR_CONNECT, MF_ENABLED);
            EnableMenuItem(hMainMenu, IDM_MONITOR_SCAN_IP, MF_ENABLED);
            return 0;
        }

        case WN_INIT_DONE:
        {
            if ((BOOL)lParam == TRUE) {
                PrintToStatusWindow(L"Initialization Error. Please restart.");
            } else
                PrintToStatusWindow(L"Initialization Complete");

            return 0;
        }

        case WM_CLOSE:
        {
            WriteToLogFile(FP_SERVER_LOGFILE, L"Main", L"WndProc", L"ServerRM exit\n", 0);
            break;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }

    }// switch(message)

    return DefWindowProc(hWnd, message, wParam, lParam);

}// WndProc()

// InitThread()
// Function to carry out initialization operations.
static DWORD WINAPI InitThread(LPVOID lpParam) {
    HWND hOwnerWnd = (HWND)lpParam;
    BOOL ErrorFlag = FALSE;
    int iRetVal, ErrorCode = 0;

    // get current working directory and store in global variable
    WCHAR *pwszCWD = NULL;
    pwszCWD = _wgetcwd(pwszCWD, MAX_PATH + 1);
    // handle error

    // copy the path returned to global variable
    wsprintf(g_wszCWD, L"%s", pwszCWD);

    // create the default directories
    // first, the Clients dir
    if ((iRetVal = CreateDirectory(FP_CLIENTS_DIR, NULL)) == ERROR_PATH_NOT_FOUND) {
        ErrorFlag = TRUE;
        ErrorCode = E_CREATE_DIR;
        WriteToLogFile(FP_SERVER_LOGFILE, L"Main", L"InitThread", L"Failed to create Clients Directory", GetLastError());
        SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);
        return FALSE;
    }

    // Plugin DLL directory
    if ((iRetVal = CreateDirectory(FP_SERVER_PLUGIN_DLL_DIR, NULL)) == ERROR_PATH_NOT_FOUND) {
        ErrorFlag = TRUE;
        ErrorCode = E_CREATE_DIR;
        WriteToLogFile(FP_SERVER_LOGFILE, L"Main", L"InitThread", L"Failed to create PluginDLLs Directory", GetLastError());
        SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);
        return FALSE;
    }

    // Temp dir
    if ((iRetVal = CreateDirectory(FP_TEMP_DIR, NULL)) == ERROR_PATH_NOT_FOUND) {
        ErrorFlag = TRUE;
        ErrorCode = E_CREATE_DIR;
        WriteToLogFile(FP_SERVER_LOGFILE, L"Main", L"InitThread", L"Failed to create Temp Directory", GetLastError());
        SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);
        return FALSE;
    }

    // check whether config file exists
    //if( (iRetVal = _wfopen_s(&fpConfigFile, FP_SERVER_CONF, L"r")) != 0 )
    //{
    //	if(iRetVal == ERROR_FILE_NOT_FOUND)
    //	{
    //		// create the file
    //		if( (iRetVal = _wfopen_s(&fpConfigFile, FP_SERVER_CONF, L"w")) != 0 )
    //		{
    //			// cannot create the file
    //			ErrorFlag = TRUE;
    //			ErrorCode = E_FILE_CREATION;
    //			SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);
    //			return FALSE;
    //		}

    //		memset(&RMServerConf, 0, sizeof(RMSERVERCONF));

    //		// fill the server config structure
    //		RMServerConf.FirstRun = TRUE;
    //		RMServerConf.iComPort = 80;

    //		GetLocalTime(&stCurrentTime);
    //		memcpy(&(RMServerConf.stPrevRunTime), &stCurrentTime, sizeof(SYSTEMTIME));

    //		RMServerConf.nPlugins = 2;

    //		// copy pre-defined file paths to structure
    //		lstrcpy(RMServerConf.LocalFilePaths[0].wszName, L"RMServerConf");
    //		lstrcpy(RMServerConf.LocalFilePaths[0].wszPath, wszRMServerConf);
    //		lstrcpy(RMServerConf.LocalFilePaths[1].wszName, L"PluginsDir");
    //		lstrcpy(RMServerConf.LocalFilePaths[1].wszPath, wszPluginsDir);
    //		lstrcpy(RMServerConf.LocalFilePaths[2].wszName, L"KLDir");
    //		lstrcpy(RMServerConf.LocalFilePaths[2].wszPath, wszKLDir);
    //		lstrcpy(RMServerConf.LocalFilePaths[3].wszName, L"SGDir");
    //		lstrcpy(RMServerConf.LocalFilePaths[3].wszPath, wszSGDir);
    //		lstrcpy(RMServerConf.LocalFilePaths[4].wszName, L"KLDll");
    //		lstrcpy(RMServerConf.LocalFilePaths[4].wszPath, wszKLDll);
    //		lstrcpy(RMServerConf.LocalFilePaths[5].wszName, L"SGDll");
    //		lstrcpy(RMServerConf.LocalFilePaths[5].wszPath, wszSGDll);
    //		lstrcpy(RMServerConf.LocalFilePaths[6].wszName, L"KLLogs");
    //		lstrcpy(RMServerConf.LocalFilePaths[6].wszPath, wszKLLogs);
    //		lstrcpy(RMServerConf.LocalFilePaths[7].wszName, L"SGShots");
    //		lstrcpy(RMServerConf.LocalFilePaths[7].wszPath, wszSGShots);
    //		lstrcpy(RMServerConf.LocalFilePaths[8].wszName, L"TempDir");
    //		lstrcpy(RMServerConf.LocalFilePaths[8].wszPath, wszTempDir);

    //		// write the structure to config file
    //		if( fwrite(&RMServerConf, sizeof(RMServerConf), 1, fpConfigFile) <= 0 )
    //		{
    //			fclose(fpConfigFile);
    //			ErrorFlag = TRUE;
    //			ErrorCode = E_FILE_CREATION;
    //			SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);
    //			return FALSE;
    //		}

    //		// copy the configuration to the global variable
    //		memcpy(&g_RMServerConf, &RMServerConf, sizeof(RMSERVERCONF));
    //	}
    //	else
    //	{
    //		// unknown error
    //	}
    //}// _wfopen_s()

    //// file found, read the contents into global variable g_RMServerConf
    //if( (iRetVal = fread(&g_RMServerConf, sizeof(RMServerConf), 1, fpConfigFile)) != 1)
    //{
    //	fclose(fpConfigFile);
    //	ErrorFlag = TRUE;
    //	ErrorCode = E_FILE_READ;
    //	SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);
    //	return FALSE;
    //}

    //// update the previous run time
    //g_RMServerConf.FirstRun = FALSE;

    // Menu items
    EnableMenuItem(hMainMenu, IDM_MONITOR_INSTALLPLUGIN, MF_GRAYED);
    EnableMenuItem(hMainMenu, IDM_MONITOR_UNINSTALLPLUGIN, MF_GRAYED);
    EnableMenuItem(hMainMenu, IDM_MONITOR_DISCONNECT, MF_GRAYED);
    EnableMenuItem(hMainMenu, IDM_LAUNCHPLUGIN_KLM, MF_GRAYED);
    EnableMenuItem(hMainMenu, IDM_LAUNCHPLUGIN_SGM, MF_GRAYED);
    EnableMenuItem(hMainMenu, IDM_LAUNCHPLUGIN_FBM, MF_GRAYED);
    EnableMenuItem(hMainMenu, IDM_MONITOR_CONNECT, MF_ENABLED);

    SendMessage(hOwnerWnd, WN_INIT_DONE, 0, 0);

    return TRUE;

}

static BOOL Cleanup() {
    ServerPluginManager(SPM_STOPSESSION, NULL, NULL);

    return TRUE;
}

BOOL CenterWindow(HWND hWnd) {
    int x, y;
    int iWidth, iHeight;
    RECT rect, rectP;
    HWND hwndParent = NULL;
    int iScreenWidth, iScreenHeight;

    //make the window relative to its parent
    if ((hwndParent = GetParent(hWnd)) == NULL)
        return FALSE;

    GetWindowRect(hWnd, &rect);
    GetWindowRect(hwndParent, &rectP);

    iWidth = rect.right - rect.left;
    iHeight = rect.bottom - rect.top;

    x = ((rectP.right - rectP.left) - iWidth) / 2 + rectP.left;
    y = ((rectP.bottom - rectP.top) - iHeight) / 2 + rectP.top;

    iScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    iScreenHeight = GetSystemMetrics(SM_CYSCREEN);

    //make sure that the dialog box never moves outside of
    //the screen
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + iWidth > iScreenWidth)  x = iScreenWidth - iWidth;
    if (y + iHeight > iScreenHeight) y = iScreenHeight - iHeight;

    MoveWindow(hWnd, x, y, iWidth, iHeight, FALSE);

    return TRUE;
}

BOOL UpdateClientConfUI(const CLIENTCONF *pClientConf) {
    extern WCHAR g_wszCurCliIP[], g_wszCurCliNick[], g_wszCurCliDIR[];

    int i, iRetVal, nItems;
    WCHAR wszClientConfMsg[MAX_SW_MSG + 1] = L"";

    if (!hClientConfListBox)
        return FALSE;

    if (!pClientConf) {
        ShowWindow(hClientConfListBox, SW_HIDE);
        return TRUE;
    }

    nItems = SendMessage(hClientConfListBox, LB_GETCOUNT, 0, 0);

    // delete existing strings
    for (i = 0; i < nItems; ++i) {
        iRetVal = SendMessage(hClientConfListBox, LB_DELETESTRING, 0, 0);
        if (iRetVal == LB_ERR)
            iRetVal = GetLastError();
    }

    wsprintf(wszClientConfMsg, L"** Details of Client Machine Currently Connected To **");
    iRetVal = SendMessage(hClientConfListBox, LB_ADDSTRING, 0, (LPARAM)wszClientConfMsg);

    // blank line
    iRetVal = SendMessage(hClientConfListBox, LB_ADDSTRING, 0, (LPARAM)L"");

    // first display the clientIP and nickname
    wsprintf(wszClientConfMsg, L"Client IP: %s", pClientConf->wszIP);
    iRetVal = SendMessage(hClientConfListBox, LB_ADDSTRING, 0, (LPARAM)wszClientConfMsg);

    // nick name
    wsprintf(wszClientConfMsg, L"Client Nickname: %s", g_wszCurCliNick);
    iRetVal = SendMessage(hClientConfListBox, LB_ADDSTRING, 0, (LPARAM)wszClientConfMsg);

    // client working directory
    wsprintf(wszClientConfMsg, L"Client Directory: %s", g_wszCurCliDIR);
    iRetVal = SendMessage(hClientConfListBox, LB_ADDSTRING, 0, (LPARAM)wszClientConfMsg);

    // blank line
    iRetVal = SendMessage(hClientConfListBox, LB_ADDSTRING, 0, (LPARAM)L"");

    // plugins
    wsprintf(wszClientConfMsg, L"* Plugins Running *");
    iRetVal = SendMessage(hClientConfListBox, LB_ADDSTRING, 0, (LPARAM)wszClientConfMsg);

    for (i = 0; i < pClientConf->nPlugins; ++i) {
        wsprintf(wszClientConfMsg, L"%s | %s", pClientConf->PluginInfo[i].wszName,
            pClientConf->PluginInfo[i].wszDLLInterfaceName);
        iRetVal = SendMessage(hClientConfListBox, LB_ADDSTRING, 0, (LPARAM)wszClientConfMsg);
    }

    // make sure the added string is visible
    iRetVal = SendMessage(hClientConfListBox, LB_SETTOPINDEX, iRetVal, 0);

    // show window
    ShowWindow(hClientConfListBox, SW_SHOW);

    return TRUE;
}
