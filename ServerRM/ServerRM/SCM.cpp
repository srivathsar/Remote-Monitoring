
// * ServerRM project *
// SCM.cpp : Contains the main entry point of ServerComModule, ServerComModule() function.

#include "ServerInclude.h"

extern HWND hMsgOnlyWnd;

typedef struct tagSCM_ThreadArgs {

    RM_COMMAND	cmd;
    SCMARGS		SCMArgs;
    CCMDATA		CCMDataIn;

}SCM_THREADARGS;

DWORD WINAPI SCMCmdHandler(LPVOID args);

BOOL ServerComModule(RM_COMMAND cmd, const SCMARGS *pSCMArgs, const CCMDATA *pCCMDataIn) {
    SCM_THREADARGS	*pThreadArgs = NULL;
    HANDLE			hSCMCmdHandlerThread;


    if ((pThreadArgs = (SCM_THREADARGS*)malloc(sizeof(SCM_THREADARGS))) == NULL) {
        PrintToStatusWindow(L"CCM: malloc error:CCMThreadArgs");
        return FALSE;
    }

    if (pSCMArgs == NULL) {
        // if CCMArgs is NULL, only 'cmd' or 'cmd with SCMDataIn' is received

        // copy the command into CCM_THREADARGS structure
        pThreadArgs->cmd = cmd;

        // check if SCMDataIn is NULL or not
        if (pCCMDataIn != NULL) {
            // if it is not NULL, copy the SCMDataIn into CCM_THREADARGS structure
            memcpy(&pThreadArgs->CCMDataIn, pCCMDataIn, sizeof(CCMDATA));
        }
    } else {
        // pCCMArgs is not NULL. Hence, copy the arguments received into CCM_THREADARGS structure.
        pThreadArgs->cmd = pSCMArgs->cmd;
        memcpy(&pThreadArgs->SCMArgs, pSCMArgs, sizeof(SCMARGS));
    }

    // after copying the arguments, invoke a new thread to 
    // handle the command and immediately return to the calling function.
    hSCMCmdHandlerThread = CreateThread(NULL,
        0,
        (LPTHREAD_START_ROUTINE)SCMCmdHandler,
        pThreadArgs,
        0,
        NULL);
    if (hSCMCmdHandlerThread == NULL)
        return FALSE;

    return TRUE;

}// ServerComModule()

DWORD WINAPI SCMCmdHandler(LPVOID lpArgs) {

    int iRetVal;

    SCM_THREADARGS *pMyArgs = NULL;
    SCMDATA *pSCMDataOut = NULL;	// to encapsulate SPM data to forward to client
    SPMARGS *pSPMArgs = NULL;		// to pass incoming data from CCM to SPM

    if ((pMyArgs = (SCM_THREADARGS*)lpArgs) == NULL) {
        PrintToStatusWindow(L"SCMCmdHandler: NULL args");
        return FALSE;
    }

    __try {
        switch (pMyArgs->cmd) {

            // ** cases for handling commands **

            case SCM_STARTSESSION:
            {
                iRetVal = SendMessage(hMsgOnlyWnd, WM_SESSION_START, 0, (LPARAM)&pMyArgs->SCMArgs.wszCliIP);
                return TRUE;
            }


            case SCM_STOPSESSION:
            {
                iRetVal = SendMessage(hMsgOnlyWnd, WM_SESSION_STOP, 0, 0);
                return TRUE;
            }


            case SCM_SENDDATA:
            {
                // malloc pSCMDataOut
                if ((pSCMDataOut = (SCMDATA*)malloc(sizeof(SCMDATA))) == NULL) {
                    PrintToStatusWindow(L"SCM: malloc error: pSCMDataOut");
                    return FALSE;
                }
                // fill the SCMData structure with command and SPMData
                memset(pSCMDataOut, 0, sizeof(SCMDATA));
                pSCMDataOut->iPacketType = PM_DATA;
                pSCMDataOut->SCMDataType.cmd = CCM_FWDDATA_TO_CPM;
                memcpy(&(pSCMDataOut->SCMDataType.SPMDataOut), &(pMyArgs->SCMArgs.SPMDataOut),
                    sizeof(SPMDATA));

                iRetVal = SendMessage(hMsgOnlyWnd, WM_SEND_DATA, 0, (LPARAM)pSCMDataOut);
                if (iRetVal == FALSE) {
                    PrintToStatusWindow(L"SCM: SPM data not sent");
                    return FALSE;
                }
                PrintToStatusWindow(L"SCM: SPM data sent");
                return TRUE;
            }


            case SCM_INIT_FT:
            {
                // malloc pSCMDataOut
                if ((pSCMDataOut = (SCMDATA*)malloc(sizeof(SCMDATA))) == NULL) {
                    PrintToStatusWindow(L"SCM: malloc error: pSCMDataOut");
                    return FALSE;
                }
                // fill the SCMData structure with command and file transfer data
                pSCMDataOut->iPacketType = COMMAND;
                pSCMDataOut->SCMDataType.cmd = CCM_INIT_FT;
                memcpy(&pSCMDataOut->SCMDataType.FileTransferData, &pMyArgs->SCMArgs.FileTransferData,
                    sizeof(FILE_TRANSFER_DATA));

                // send the SCMData structure to client
                iRetVal = SendMessage(hMsgOnlyWnd, WM_SEND_DATA, 0, (LPARAM)pSCMDataOut);
                if (iRetVal == FALSE) {
                    PrintToStatusWindow(L"SCM: FT data not sent");
                    return FALSE;
                }
                PrintToStatusWindow(L"SCM: CCM_INIT_FT command sent");
                return TRUE;
            }


            // ** cases for handling notifications **

            case SCMN_CONNECTED:
            {
                ServerPluginManager(SPMN_CONNECTED, NULL, NULL);
                return TRUE;
            }


            case SCMN_DISCONNECTED:
            {
                ServerPluginManager(SPMN_DISCONNECTED, NULL, NULL);
                return TRUE;
            }


            case SCMN_DATA_IN:
            {
                // check packet type
                switch (pMyArgs->CCMDataIn.iPacketType) {
                    case COMMAND:
                    {
                        return TRUE;
                    }


                    case NOTIFICATION:
                    {
                        switch (pMyArgs->CCMDataIn.CCMDataType.cmd) {
                            case SCMN_FT_READY:
                            {
                                HANDLE hFTReadyEvent = NULL;

                                PrintToStatusWindow(L"SCM: FT Ready notification received");

                                // get handle to FT ready event
                                hFTReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_FT_READY);
                                if (hFTReadyEvent == NULL) {
                                    PrintToStatusWindow(L"SCM: FTReadyEvent error");
                                    return FALSE;
                                }

                                // set the event
                                SetEvent(hFTReadyEvent);
                                CloseHandle(hFTReadyEvent);

                                return TRUE;
                            }

                        }// switch(MyArgs->CCMDataIn.CCMDataType.cmd)
                        return FALSE;

                    }// case NOTIFICATION


                    case PM_DATA:
                    {
                        // forward CPMDATA to SPM
                        PrintToStatusWindow(L"CPM Data received");
                        ServerPluginManager(SPMN_DATA_IN, NULL, &pMyArgs->CCMDataIn.CCMDataType.CPMDataOut);
                        return TRUE;
                    }
                }// switch(MyArgs->CCMDataIn.iPacketType)
                return FALSE;
            }


            // ** cases for handling error codes **
            case E_CONNFAIL:
            {
                ServerPluginManager(E_CONNFAIL, NULL, NULL);
                return TRUE;
            }

        }// switch(MyArgs->cmd)
        return FALSE;
    }
    __finally {
        if (pMyArgs)
            free(pMyArgs);

        if (pSCMDataOut)
            free(pSCMDataOut);
    }

}
