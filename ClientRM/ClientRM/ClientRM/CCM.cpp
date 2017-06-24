
// ** ClientRM project **
// CCM.cpp : Entry point for the ClientComModule.

#include "ClientInclude.h"

typedef struct tagCCM_ThreadArgs {
    RM_COMMAND	cmd;
    CCMARGS		CCMArgs;
    SCMDATA		SCMDataIn;

}CCM_THREADARGS;

// Global variables
SOCKET ListenSocket = INVALID_SOCKET;
WCHAR g_wszSrcFilePath[MAX_PATH + 1];

// Private Functions
DWORD WINAPI CCMCmdHandler(LPVOID lpArgs);


BOOL ClientComModule(RM_COMMAND cmd, const CCMARGS *pCCMArgs, const SCMDATA *pSCMDataIn) {
    CCM_THREADARGS	*pThreadArgs = NULL;
    HANDLE			hCCMCmdHandlerThread = NULL;


    if ((pThreadArgs = (CCM_THREADARGS*)malloc(sizeof(CCM_THREADARGS))) == NULL) {
        PrintToStatusWindow(L"CCM: malloc error:CCMThreadArgs");
        return FALSE;
    }

    if (pCCMArgs == NULL) {
        // if CCMArgs is NULL, only 'cmd' or 'cmd with SCMDataIn' is received

        // copy the command into CCM_THREADARGS structure
        pThreadArgs->cmd = cmd;

        // check if SCMDataIn is NULL or not
        if (pSCMDataIn != NULL) {
            // if it is not NULL, copy the SCMDataIn into CCM_THREADARGS structure
            memcpy(&pThreadArgs->SCMDataIn, pSCMDataIn, sizeof(SCMDATA));
        }
    } else {
        // pCCMArgs is not NULL. Hence, copy the arguments received into CCM_THREADARGS structure.
        pThreadArgs->cmd = pCCMArgs->cmd;
        memcpy(&pThreadArgs->CCMArgs, pCCMArgs, sizeof(CCMARGS));
    }


    // after copying the arguments, invoke a new thread to 
    // handle the command and immediately return to the calling function.
    hCCMCmdHandlerThread = CreateThread(NULL,
        CUSTOM_STACK_SIZE,
        (LPTHREAD_START_ROUTINE)CCMCmdHandler,
        pThreadArgs,
        0,
        NULL);
    if (hCCMCmdHandlerThread == NULL)
        return FALSE;

    return TRUE;

}// ClientComModule()

DWORD WINAPI CCMCmdHandler(LPVOID lpArgs) {
    extern HWND hMsgOnlyWnd;

    int iRetVal;
    CCMDATA *pCCMData = NULL;
    CCM_THREADARGS *pMyArgs = (CCM_THREADARGS*)lpArgs;

    __try {
        switch (pMyArgs->cmd) {

            // ** cases for handling commands **

            case CCM_INIT_MODULE:
            {
                extern HWND hMsgOnlyWnd;
                extern CLIENTCONF g_ClientConf;

                WSADATA wsaData;
                char szHostName[MAX_HOST_NAME + 1];
                char *szHostIP;
                WCHAR wszHostIP[IPADDR_LEN + 1];
                struct hostent *HostDetails;
                struct sockaddr_in MyAddr;

                // set up the server and wait for incoming connection from the server

                // initialize WSA
                if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
                    iRetVal = WSAGetLastError();
                    WriteToLogFile(FP_CLIENT_LOGFILE, L"CCM", L"CCMCmdHandler", L"WSAStartup() error", iRetVal);
                    PrintToStatusWindow(L"WSAStartup() error");
                    return 0;
                }
                PrintToStatusWindow(L"WSAStartup() called");

                // get local hostname and true IPaddress assigned(not 127.0.0.1!! )
                // first, the hostname
                gethostname(szHostName, 255);

                // get host details
                HostDetails = gethostbyname(szHostName);
                szHostIP = inet_ntoa(*(struct in_addr *)*HostDetails->h_addr_list);

                // create socket
                if ((ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
                    iRetVal = WSAGetLastError();
                    WriteToLogFile(FP_CLIENT_LOGFILE, L"CCM", L"CCMCmdHandler", L"socket creation error", iRetVal);
                    PrintToStatusWindow(L"socket() error");
                    WSACleanup();
                    PrintToStatusWindow(L"WSACleanup() called");
                    return iRetVal;
                }

                // call WSAAsyncSelect() to force functions to be non-blocking
                if (WSAAsyncSelect(ListenSocket, hMsgOnlyWnd,
                    WM_SOCKET_NOTIFY, FD_READ |			// notification of readiness for reading
                    FD_WRITE |		// notification of readiness for writing
                    FD_CLOSE |		// notification of socket closure
                    FD_ACCEPT)		// notification of incoming connections
                    == SOCKET_ERROR) {
                    iRetVal = WSAGetLastError();
                    WriteToLogFile(FP_CLIENT_LOGFILE, L"CCM", L"CCMCmdHandler", L"WSAAsyncSelect() error", iRetVal);
                    PrintToStatusWindow(L"WSAAsyncSelect() error");
                    closesocket(ListenSocket);
                    ListenSocket = INVALID_SOCKET;
                    WSACleanup();
                    PrintToStatusWindow(L"WSACleanup() called");
                    return 0;
                }


                // bind socket to port
                MyAddr.sin_family = AF_INET;
                MyAddr.sin_port = htons(COM_PORT);
                MyAddr.sin_addr.S_un.S_addr = inet_addr(szHostIP);

                if (bind(ListenSocket, (SOCKADDR*)&MyAddr, sizeof(MyAddr)) != 0) {
                    iRetVal = WSAGetLastError();
                    WriteToLogFile(FP_CLIENT_LOGFILE, L"CCM", L"CCMCmdHandler", L"bind() error", iRetVal);
                    PrintToStatusWindow(L"bind() error");
                    closesocket(ListenSocket);
                    ListenSocket = INVALID_SOCKET;
                    WSACleanup();
                    PrintToStatusWindow(L"WSACleanup() called");
                    return 0;
                }

                // listen
                if (listen(ListenSocket, 3) != 0) {
                    iRetVal = WSAGetLastError();
                    WriteToLogFile(FP_CLIENT_LOGFILE, L"CCM", L"CCMCmdHandler", L"listen() error", iRetVal);
                    PrintToStatusWindow(L"listen() error");
                    closesocket(ListenSocket);
                    ListenSocket = INVALID_SOCKET;
                    WSACleanup();
                    PrintToStatusWindow(L"WSACleanup() called");
                    return 0;
                }

                iRetVal = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szHostIP, strlen(szHostIP),
                    wszHostIP, IPADDR_LEN + 1);
                wszHostIP[iRetVal] = 0;
                PrintToStatusWindow(wszHostIP);
                wcscpy_s(g_ClientConf.wszIP, IPADDR_LEN + 1, wszHostIP);
                return 0;

            } // CCM_INIT_MODULE


            case CCM_SENDDATA:
            {
                __try {
                    if ((pCCMData = (CCMDATA*)malloc(sizeof(CCMDATA))) == NULL) {
                        // WriteToLog()
                        return 0;
                    }
                    memset(pCCMData, 0, sizeof(CCMDATA));

                    // fill the CCMData structure with command and CPMData
                    pCCMData->iPacketType = PM_DATA;
                    pCCMData->CCMDataType.cmd = SCM_FWDDATA_TO_SPM;
                    memcpy(&(pCCMData->CCMDataType.CPMDataOut), &(pMyArgs->CCMArgs.CCMDataType.CPMDataOut),
                        sizeof(CPMDATA));

                    iRetVal = SendMessage(hMsgOnlyWnd, WM_SEND_DATA, 0, (LPARAM)pCCMData);
                    if (iRetVal == FALSE) {
                        PrintToStatusWindow(L"CCM: CPM data not sent");
                        return FALSE;
                    }
                    PrintToStatusWindow(L"CCM: CPM data sent");
                    return TRUE;
                }
                __finally {
                    if (pCCMData) {
                        free(pCCMData);
                        pCCMData = NULL;
                    }
                }
            }


            // ** cases for handling notifications **

            case CCMN_DATA_IN:
            {
                // check packet type
                switch (pMyArgs->SCMDataIn.iPacketType) {
                    case COMMAND:
                    {
                        switch (pMyArgs->SCMDataIn.SCMDataType.cmd) {
                            case CCM_INIT_FT:
                            {
                                wcscpy_s(g_wszSrcFilePath, MAX_PATH + 1,
                                    pMyArgs->SCMDataIn.SCMDataType.FileTransferData.ListOfFiles[0].wszFileName);

                                // call TransferFile()
                                if ((iRetVal = TransferFile(pMyArgs->SCMDataIn.SCMDataType.
                                    FileTransferData.ListOfFiles[0].wszFileName)) == FALSE) {
                                    // send error msg to CPM
                                    return FALSE;
                                }
                                return TRUE;
                            }
                        }
                    }

                    case NOTIFICATION:
                    {
                        return 0;
                    }

                    case PM_DATA:
                    {
                        // check command from SCM
                        switch (pMyArgs->SCMDataIn.SCMDataType.cmd) {
                            case CCM_FWDDATA_TO_CPM:
                            {
                                PrintToStatusWindow(L"SPM data received");

                                // call ClientPluginManager()
                                ClientPluginManager(CPMN_DATA_IN, NULL, &pMyArgs->SCMDataIn.SCMDataType.SPMDataOut);
                                return TRUE;
                            }

                        }// switch(pMyArgs->SCMDataIn.SCMDataType.cmd)
                        return FALSE;
                    }
                    return TRUE;
                }// switch(pMyArgs->SCMDataIn.iPacketType)
            }

            case CCMN_FT_READY:
            {
                __try {
                    if ((pCCMData = (CCMDATA*)malloc(sizeof(CCMDATA))) == NULL) {
                        // WriteToLog()
                        return 0;
                    }
                    memset(pCCMData, 0, sizeof(CCMDATA));

                    // send notification to SCM that FT is ready to begin
                    pCCMData->iPacketType = NOTIFICATION;
                    pCCMData->CCMDataType.cmd = SCMN_FT_READY;
                    iRetVal = SendMessage(hMsgOnlyWnd, WM_SEND_DATA, 0, (LPARAM)pCCMData);
                    if (iRetVal == FALSE) {
                        PrintToStatusWindow(L"CCM: FT Ready notification not sent");
                        return FALSE;
                    }
                    PrintToStatusWindow(L"CCM: FT Ready notification sent");
                    return 0;
                }
                __finally {
                    if (pCCMData) {
                        free(pCCMData);
                        pCCMData = NULL;
                    }
                }
            }

        }// switch(pMyArgs->cmd)
        return FALSE;

    }
    __finally {
        if (pMyArgs)
            free(pMyArgs);
    }

}// CCMCmdHandler()