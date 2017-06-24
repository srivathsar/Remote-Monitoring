
// * ClientRM Project *
// WndProc.cpp: Window procedure function to handle messages sent 
// to the hidden client main window.

#include "ClientInclude.h"

// Global variables
HWND hListBox, hStatusBar;
CLIENTCONF g_ClientConf;
HANDLE hKLConfigEvent = NULL;
WCHAR g_wszCWD[MAX_PATH + 1];
DLLINFO g_DLLInfo[MAX_PLUGINS];

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    extern HINSTANCE hMainInstance;

    int i, iRetVal;
    RECT rClientArea;
    HANDLE hInitThread = NULL;
    static HANDLE hDataReadyEventThread = NULL;

    WCHAR wszSWMsg[MAX_SW_MSG + 1];

    switch (message) {

        case WM_CREATE:
        {
            FILE *fp = NULL;
            WCHAR wszDate[64];
            SYSTEMTIME stToday;

            // check whether log file exists
            if ((iRetVal = _wfopen_s(&fp, FP_CLIENT_LOGFILE, L"r")) != 0) {
                // create a new file
                if ((iRetVal = _wfopen_s(&fp, FP_CLIENT_LOGFILE, L"w, ccs=UTF-8")) != 0) {
                    // set flag in ServerRM status structure (code yet to be written)
                } else {
                    fclose(fp);
                    fp = NULL;
                    WriteToLogFile(FP_CLIENT_LOGFILE, NULL, NULL, NULL, 0); // write headers
                }
            } else {
                fclose(fp);
                fp = NULL;
            }
            GetLocalTime(&stToday);
            swprintf_s(wszDate, 64, L"%d/%d/%d ClientRM started", stToday.wDay, stToday.wMonth, stToday.wYear);
            WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"WndProc", wszDate, 0);

            iRetVal = GetClientRect(hWnd, (LPRECT)&rClientArea);

            // Create Status Window, which is a ListBox
            hListBox = CreateWindowEx(
                0,
                L"listbox",
                L"ClientRM_StatusWnd",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | WS_HSCROLL | LBS_NOSEL | LBS_NOINTEGRALHEIGHT,
                6,							// x pos
                rClientArea.bottom - 125,	// y pos
                rClientArea.right - 12,		// x size
                100,						// y size
                hWnd,
                NULL,
                hMainInstance,
                NULL);
            if (!hListBox) {
                WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"WndProc", L"Failed to create Status Window", GetLastError());
                MessageBox(NULL, L"ListBox not created", 0, 0);
            }

            // get current working directory and store in global variable
            WCHAR *pwszCWD = NULL;
            if ((pwszCWD = _wgetcwd(NULL, 0)) == NULL) {
                break;
            }

            // copy the path returned to global variable
            wcscpy_s(g_wszCWD, MAX_PATH, pwszCWD);

            // free the memory
            if (pwszCWD)
                free(pwszCWD);

            // start the ClientComModule(CCM)
            if ((iRetVal = ClientComModule(CCM_INIT_MODULE, NULL, NULL)) != TRUE) {
                /*ErrorFlag = TRUE;
                ErrorCode = E_CCM;
                SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);*/
                return FALSE;
            }

            // After initialization of the UI, call InitThread() 
            // to perform other initialization operations.
            hInitThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)InitThread, hWnd, 0, NULL);
            if (hInitThread == NULL) {
                WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"WndProc", L"Failed to create InitThread", GetLastError());
                MessageBox(hWnd, L"Init Thread error", 0, 0);
            }

            // Create a thread to wait for IFDataReadyEvent from the plug-in interface DLLs.
            hDataReadyEventThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)DataReadyEventThread,
                NULL, 0, NULL);
            if (hDataReadyEventThread == NULL) {
                WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"WndProc", L"Failed to create DataReadyEventThread", GetLastError());
                MessageBox(hWnd, L"DataReadyEvent Thread error", 0, 0);
            }

            return 0;
        }

        case WM_CLOSE:
        {
            // terminate all plugin interfaces
            for (i = 0; i < g_ClientConf.nPlugins; ++i) {
                // read the DLL interface name and call ProcessCmd()
                if (g_DLLInfo[i].hProcessCmd != NULL &&
                    (g_DLLInfo[i].hProcessCmd(IF_TERM_PLUGIN, NULL, NULL) == FALSE)) {
                    swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"%s failed to terminate", g_ClientConf.PluginInfo[i].wszName);
                    WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"WndProc", wszSWMsg, GetLastError());
                    PrintToStatusWindow(wszSWMsg);
                }
                if (g_DLLInfo[i].hPluginDLL != NULL && FreeLibrary(g_DLLInfo[i].hPluginDLL) == FALSE) {
                    swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"FreeLibrary() failed to free %s", g_ClientConf.PluginInfo[i].wszName);
                    WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"WndProc", wszSWMsg, GetLastError());
                    PrintToStatusWindow(wszSWMsg);
                }
            }

            WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"WndProc", L"ClientRM exit\n", 0);
            break;
        }

        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }

        case WN_INIT_DONE:
        {
            // initialize all plugin interfaces
            for (i = 0; i < g_ClientConf.nPlugins; ++i) {
                // read the DLL interface name and call LoadLibrary()
                if ((g_DLLInfo[i].hPluginDLL = LoadLibrary(g_ClientConf.PluginInfo[i].wszDLLInterfaceName)) == NULL) {
                    swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"Error initializing %s", g_ClientConf.PluginInfo[i].wszName);
                    PrintToStatusWindow(wszSWMsg);
                    WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"WndProc", wszSWMsg, GetLastError());
                }
                if ((g_DLLInfo[i].hProcessCmd = (PROCESSCMD)GetProcAddress(g_DLLInfo[i].hPluginDLL, DLL_IF_PROCESS)) == NULL) {
                    wsprintf(wszSWMsg, L"Error calling interface of %s", g_ClientConf.PluginInfo[i].wszName);
                    PrintToStatusWindow(wszSWMsg);
                    WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"WndProc", wszSWMsg, GetLastError());
                }
                if (g_DLLInfo[i].hProcessCmd != NULL &&
                    (g_DLLInfo[i].hProcessCmd(IF_INIT_PLUGIN, NULL, NULL) == FALSE)) {
                    wsprintf(wszSWMsg, L"%s failed to initialize", g_ClientConf.PluginInfo[i].wszName);
                    PrintToStatusWindow(wszSWMsg);
                    WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"WndProc", wszSWMsg, GetLastError());
                }
            }

            // display notification to the user
            if ((BOOL)lParam == TRUE)
                PrintToStatusWindow(L"Error while initializing");
            else
                PrintToStatusWindow(L"Initializing done");

            return 0;
        }

    }// switch(message)

    return DefWindowProc(hWnd, message, wParam, lParam);

}

DWORD WINAPI DataReadyEventThread(LPVOID lpArgs) {
    int i;

    static PDATA *pPData = NULL;

    CPMARGS *pCPMArgs = NULL;
    HANDLE hDataReadyEvent = NULL;
    DWORD dwDataReadyEventSignal;

    // Create an event to receive notification from the interface DLLs
    // when there is data to be sent from the interface DLL to the server.
    hDataReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_IF_DATA_READY);
    if (hDataReadyEvent == NULL) {
        // indicate to CPM
        PrintToStatusWindow(L"DataReadyEventThread: Unable to create DataReadyEvent handle");
        WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"DataReadyEventThread", L"Unable to create DataReadyEvent handle",
            GetLastError());
        return FALSE;
    }

    while (TRUE) {
        dwDataReadyEventSignal = WaitForSingleObject(hDataReadyEvent, INFINITE);
        if (dwDataReadyEventSignal == WAIT_OBJECT_0) {
            // allocate memory for CPMArgs
            if ((pCPMArgs = (CPMARGS*)malloc(sizeof(CPMARGS))) == NULL) {
                // indicate to CPM
                PrintToStatusWindow(L"DataReadyEventThread: malloc error: pCPMArgs");
                WriteToLogFile(FP_CLIENT_LOGFILE, L"Main", L"DataReadyEventThread", L"malloc() error: pCPMArgs",
                    GetLastError());
                return FALSE;
            }

            // zero memory
            memset(pCPMArgs, 0, sizeof(CPMARGS));

            // call process command of all the interface DLLs
            for (i = 0; i < g_ClientConf.nPlugins; ++i) {
                if (g_DLLInfo[i].hProcessCmd != NULL &&
                    (g_DLLInfo[i].hProcessCmd(IF_GET_PDATA, NULL, &pPData) == FALSE)) {
                    continue;
                }

                if (pPData != NULL) {
                    // copy PDATA received from the plugin interface to CPMARGS structure
                    memcpy(&pCPMArgs->PData, pPData, sizeof(PDATA));
                    pCPMArgs->cmd = CPM_SENDDATA;

                    // call CPM and send the PDATA structure
                    ClientPluginManager(0, pCPMArgs, NULL);

                    if (pPData)
                        free(pPData);
                }

            }// for i
        }

        // after sending the data, free the allocated memory
        if (pCPMArgs)
            free(pCPMArgs);

        // reset the event
        ResetEvent(hDataReadyEvent);

    }//while(TRUE)
}