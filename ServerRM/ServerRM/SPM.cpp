
// * ServerRM project *
// SPM.cpp : Contains the main entry point of ServerPluginManager, the ServerPluginManager() function.

#include "ServerInclude.h"

// Data structure to store arguments to the SPMCmdHandlerThread() function
typedef struct tagSPM_ThreadArgs {

    RM_COMMAND	cmd;
    SPMARGS		SPMArgs;
    CPMDATA		CPMDataIn;

}SPM_THREADARGS;

extern PMONITORS g_PMonitors;

// Global variables
WCHAR g_wszCurCliIP[IPADDR_LEN + 1];
WCHAR g_wszCurCliNick[MAX_NICK_NAME + 1];
WCHAR g_wszCurCliDIR[MAX_PATH + 1];

// Ping Client
CLIENTCONF ccPingReply = { 0 };
BOOL fClientActive = FALSE;
BOOL fPingInProgress = FALSE;


// Private helper functions
static DWORD WINAPI	SPMCmdHandlerThread(LPVOID args);


// ServerPluginManager(RM_COMMAND cmd, const SPMARGS *pSPMArgs, const CPMDATA *pCPMDataIn)
// --

BOOL ServerPluginManager(RM_COMMAND cmd, const SPMARGS *pSPMArgs, const CPMDATA *pCPMDataIn) {

    SPM_THREADARGS	*pThreadArgs = NULL;
    HANDLE			hSPMCmdHandlerThread;

    int iRetVal;

    if ((pThreadArgs = (SPM_THREADARGS*)malloc(sizeof(SPM_THREADARGS))) == NULL) {
        PrintToStatusWindow(L"SPM: malloc error:SPMThreadArgs");
        return FALSE;
    }
    memset(pThreadArgs, 0, sizeof(SPM_THREADARGS));

    if (pSPMArgs == NULL) {
        // if CCMArgs is NULL, only 'cmd' or 'cmd with SCMDataIn' is received

        // copy the command into CCM_THREADARGS structure
        pThreadArgs->cmd = cmd;

        // check if SCMDataIn is NULL or not
        if (pCPMDataIn != NULL) {
            // if it is not NULL, copy the SCMDataIn into CCM_THREADARGS structure
            memcpy(&pThreadArgs->CPMDataIn, pCPMDataIn, sizeof(CPMDATA));
        }
    } else {
        // pCCMArgs is not NULL. Hence, copy the arguments received into CCM_THREADARGS structure.
        pThreadArgs->cmd = pSPMArgs->cmd;
        memcpy(&pThreadArgs->SPMArgs, pSPMArgs, sizeof(SPMARGS));
    }

    // after copying the arguments, invoke a new thread to 
    // handle the command and immediately return to the calling function.
    hSPMCmdHandlerThread = CreateThread(NULL,											// security attributes
        0,												// stack size
        (LPTHREAD_START_ROUTINE)SPMCmdHandlerThread,	// start address of ThreadProc
        (LPVOID)pThreadArgs,							// pointer to variable to be passed to the thread
        0,												// default creation flags
        NULL);											// threadID variable

    if (hSPMCmdHandlerThread == NULL) {
        iRetVal = GetLastError();
        PrintToStatusWindow(L"SPM: Thread creation error");
        return FALSE;
    }

    return TRUE;

}// ServerPluginManager()

DWORD WINAPI SPMCmdHandlerThread(LPVOID lpArgs) {
    extern HMENU hMainMenu;
    extern HWND hMainWnd, hMsgOnlyWnd;

    int i, iRetVal;

    SCMARGS *pSCMArgs = NULL;
    SPM_THREADARGS *pMyArgs = NULL;

    WCHAR wszSBMsg[MAX_SB_MSG + 1];		// buffer to store string to be displayed in the status bar
    WCHAR wszSWMsg[MAX_SW_MSG + 1];		// buffer to store string to be displayed in the status window
    WCHAR wszDirPath[MAX_PATH + 1];

    // first, retrieve the arguments
    if ((pMyArgs = (SPM_THREADARGS*)lpArgs) == NULL) {
        PrintToStatusWindow(L"SCMCmdHandler: NULL args");
        return 0;
    };

    __try {
        switch (pMyArgs->cmd) {
            // ** cases for handling commands **

            case SPM_STARTSESSION:
            {
                if (fPingInProgress == TRUE)
                    fClientActive = FALSE;

                if ((pSCMArgs = (SCMARGS*)malloc(sizeof(SCMARGS))) == NULL) {
                    PrintToStatusWindow(L"SPM: malloc error:SCMArgs");
                    return 0;
                }
                memset(pSCMArgs, 0, sizeof(SCMARGS));

                // initialize the global variable which holds the currently
                // connected client machine's IPaddress and nick name.
                lstrcpy(g_wszCurCliIP, pMyArgs->SPMArgs.wszCliIP);
                lstrcpy(g_wszCurCliNick, pMyArgs->SPMArgs.wszCliNick);

                // Call ServerComModule()
                pSCMArgs->cmd = SCM_STARTSESSION;
                lstrcpy(pSCMArgs->wszCliIP, pMyArgs->SPMArgs.wszCliIP);

                swprintf_s(wszSBMsg, MAX_SB_MSG + 1, L"SPM: Connecting to %s...", pMyArgs->SPMArgs.wszCliIP);
                PrintToStatusBar(wszSBMsg, 0);

                if ((iRetVal = ServerComModule(NULL, pSCMArgs, NULL)) == FALSE) {
                    iRetVal = GetLastError();
                    PrintToStatusBar(L"SPM: SCMThread creation error", 0);
                }
                return 0;
            }

            case SPM_STOPSESSION:
            {
                ServerComModule(SCM_STOPSESSION, NULL, NULL);
                return 0;
            }

            case SPM_SENDDATA:
            {
                __try {
                    if ((pSCMArgs = (SCMARGS*)malloc(sizeof(SCMARGS))) == NULL) {
                        WriteToLogFile(FP_SERVER_LOGFILE, L"SPM", L"SPMCmdHandler", L"malloc() error: pSCMArgs",
                            GetLastError());
                        return 0;
                    }

                    pSCMArgs->SPMDataOut.cmd = CPM_FWD_DATA;
                    pSCMArgs->SPMDataOut.PluginID = pMyArgs->SPMArgs.iFromPlugin;
                    memcpy(&pSCMArgs->SPMDataOut.MNTRData, &pMyArgs->SPMArgs.MNTRData, sizeof(MNTRDATA));

                    pSCMArgs->cmd = SCM_SENDDATA;

                    ServerComModule(NULL, pSCMArgs, NULL);
                    return 0;
                }
                __finally {
                    if (pSCMArgs) {
                        free(pSCMArgs);
                        pSCMArgs = NULL;
                    }
                }
            }

            case SPM_FT_TERM:
            {
                SendMessage(hMsgOnlyWnd, WM_FT_TERM, 0, 0);
                return 0;
            }


            // ** cases for handling notifications **

            case SPMN_CONNECTED:
            {
                if ((pSCMArgs = (SCMARGS*)malloc(sizeof(SCMARGS))) == NULL) {
                    PrintToStatusWindow(L"SPM: malloc error:SCMArgs");
                    return 0;
                }
                memset(pSCMArgs, 0, sizeof(SCMARGS));

                if (fPingInProgress) {
                    // send CPM_PING_CLIENT command to client
                    pSCMArgs->cmd = SCM_SENDDATA;
                    pSCMArgs->SPMDataOut.cmd = CPM_HELLO;

                    ServerComModule(0, pSCMArgs, NULL);
                    return 0;
                }

                // once connected, query the CPM for client configuration
                pSCMArgs->cmd = SCM_SENDDATA;
                pSCMArgs->SPMDataOut.cmd = CPM_QUERY;

                ServerComModule(0, pSCMArgs, NULL);

                // enable the grayed menu items and disable the Connect item
                EnableMenuItem(hMainMenu, IDM_MONITOR_DISCONNECT, MF_ENABLED);
                EnableMenuItem(hMainMenu, IDM_MONITOR_CONNECT, MF_GRAYED);
                EnableMenuItem(hMainMenu, IDM_MONITOR_SCAN_IP, MF_GRAYED);

                swprintf_s(wszSBMsg, MAX_SB_MSG + 1, L"Connected to %s", g_wszCurCliIP);
                PrintToStatusBar(wszSBMsg, 0);

                // save the current client's directory in the global variable
                swprintf_s(g_wszCurCliDIR, MAX_PATH + 1, L"%s\\%s", FP_CLIENTS_DIR, g_wszCurCliIP);

                // create the Client directory if it does not exist
                iRetVal = CreateDirectory(g_wszCurCliDIR, NULL);

                // create the Plugins directory if it does not exist
                swprintf_s(wszDirPath, MAX_PATH + 1, L"%s\\%s", g_wszCurCliDIR, FP_CLIENT_PLUGINS_DIR);
                iRetVal = CreateDirectory(wszDirPath, NULL);

                WriteToLogFile(FP_SERVER_LOGFILE, L"SPM", L"SPMCmdHandlerThread", wszSBMsg, 0);
                return 0;
            }

            case SPMN_DISCONNECTED:
            {
                PrintToStatusBar(L"Disconnected", 0);

                if (fPingInProgress) {
                    // obtain handle to PingReplyEvent
                    HANDLE hPingReplyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_PING_INFO_READY);
                    if (hPingReplyEvent == NULL) {
                        // fatal error
                        //WriteToLogFile();
                        return 0;
                    }

                    // set event
                    SetEvent(hPingReplyEvent);
                    return 0;
                }

                if (UpdateClientConfUI(NULL) == FALSE)
                    PrintToStatusWindow(L"Error updating client information window");

                // display message
                MessageBox(hMainWnd, L"Disconnected from client machine.", L"Disconnected", MB_ICONINFORMATION);
                WriteToLogFile(FP_SERVER_LOGFILE, L"SPM", L"SPMCmdHandlerThread", L"Disconnected", 0);

                // clear the global IPaddress and nickname variables and client DIR
                lstrcpy(g_wszCurCliIP, L"");
                lstrcpy(g_wszCurCliNick, L"");
                lstrcpy(g_wszCurCliDIR, L"");

                // disable all Monitor menu sub-items except Connect
                EnableMenuItem(hMainMenu, IDM_MONITOR_DISCONNECT, MF_GRAYED);
                EnableMenuItem(hMainMenu, IDM_LAUNCHPLUGIN_KLM, MF_GRAYED);
                EnableMenuItem(hMainMenu, IDM_LAUNCHPLUGIN_SGM, MF_GRAYED);
                EnableMenuItem(hMainMenu, IDM_LAUNCHPLUGIN_FBM, MF_GRAYED);
                EnableMenuItem(hMainMenu, IDM_MONITOR_CONNECT, MF_ENABLED);
                EnableMenuItem(hMainMenu, IDM_MONITOR_SCAN_IP, MF_ENABLED);

                for (i = 0; i < g_PMonitors.nPlugins; ++i) {
                    SendMessage(g_PMonitors.PluginMonitor[i].hDlgBox,
                        WN_DLG_NOTIFICATION, WN_DLG_DISCONNECTED, NULL);
                }

                WriteToLogFile(FP_SERVER_LOGFILE, L"SPM", L"SPMCmdHandlerThread", L"Disconnected", 0);
                return 0;
            }


            case SPMN_DATA_IN:
            {
                PDATA *pPDataIn = NULL;

                // call CPMDataHandler()
                switch (pMyArgs->CPMDataIn.cmd) {
                    case SPM_HELLO:
                    {
                        // set flag
                        fClientActive = TRUE;

                        // copy ClientConf to global variable and update the fClientActive flag
                        memset(&ccPingReply, 0, sizeof(CLIENTCONF));
                        memcpy(&ccPingReply, &pMyArgs->CPMDataIn.PData.PDataType.ClientConf, sizeof(CLIENTCONF));

                        // close connection
                        ServerPluginManager(SPM_STOPSESSION, NULL, NULL);
                        return 0;
                    }

                    case SPM_CLIENT_CONF:
                    {
                        // update the client info in the main window
                        if (UpdateClientConfUI(&pMyArgs->CPMDataIn.PData.PDataType.ClientConf) == FALSE)
                            PrintToStatusWindow(L"Error updating client information window");

                        // Update the Main Menu
                        for (i = 0; i < pMyArgs->CPMDataIn.PData.PDataType.ClientConf.nPlugins; ++i) {
                            switch (pMyArgs->CPMDataIn.PData.PDataType.ClientConf.PluginInfo[i].iPluginID) {
                                case KEYLOGGER:
                                {
                                    EnableMenuItem(hMainMenu, IDM_LAUNCHPLUGIN_KLM, MF_ENABLED);
                                    break;
                                }

                                case SCREENGRABBER:
                                {
                                    EnableMenuItem(hMainMenu, IDM_LAUNCHPLUGIN_SGM, MF_ENABLED);
                                    break;
                                }

                                case FILEBROWSER:
                                {
                                    EnableMenuItem(hMainMenu, IDM_LAUNCHPLUGIN_FBM, MF_ENABLED);
                                    break;
                                }
                            }
                        }
                        return 0;
                    }

                    case SPM_FWD_DATA:
                    {
                        // SPM must now send the CPMData.PData to all the plugin monitors
                        for (i = 0; i < g_PMonitors.nPlugins; ++i) {
                            // allocate memory
                            if ((pPDataIn = (PDATA*)malloc(sizeof(PDATA))) == NULL) {
                                WriteToLogFile(FP_SERVER_LOGFILE, L"SPM", L"SPMCmdHandler", L"malloc() error: pPDataIn",
                                    GetLastError());
                                continue;
                            }

                            // copy the data to allocated memory
                            memcpy(pPDataIn, &pMyArgs->CPMDataIn.PData, sizeof(PDATA));

                            // Freeing the allocated memory is the 
                            // responsibility of the individual monitors.

                            if (g_PMonitors.PluginMonitor[i].hDlgBox != NULL) {
                                // Send message to the monitor dialog box with 
                                // pointer to PDATA in lParam.
                                SendMessage(g_PMonitors.PluginMonitor[i].hDlgBox,
                                    WM_CLIENT_RESPONSE, 0, (LPARAM)pPDataIn);
                            }
                        }// for i
                    }

                }// if(pMyArgs->CPMDataIn.cmd == SPM_FWD_DATA)
                return TRUE;
            }


            // ** cases for handling error codes **

            case E_CONNFAIL:
            {
                PrintToStatusBar(L"Disconnected", 0);

                if (fPingInProgress == TRUE) {
                    // obtain handle to PingReplyEvent
                    HANDLE hPingReplyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_PING_INFO_READY);
                    if (hPingReplyEvent == NULL) {
                        // fatal error
                        //WriteToLogFile();
                        return 0;
                    }

                    // set event
                    SetEvent(hPingReplyEvent);
                    return 0;
                }
                wsprintf(wszSWMsg, L"Failed to connect to client machine %s / %s",
                    g_wszCurCliIP, g_wszCurCliNick);
                PrintToStatusWindow(wszSWMsg);
                return TRUE;
            }

        }// switch(cmd)
        return FALSE;

    }// __try

    __finally {
        // cleanup
        if (pSCMArgs)
            free(pSCMArgs);
        if (pMyArgs)
            free(pMyArgs);
    }

}// SPMCmdHandler()
