
// * ClientRM Project *
// CPM.cpp : Entry point for the ClientPluginManager

#include "ClientInclude.h"

typedef struct tagCPM_ThreadArgs {
    RM_COMMAND	cmd;
    CPMARGS		CPMArgs;
    SPMDATA		SPMDataIn;
}CPM_THREADARGS;


extern DLLINFO g_DLLInfo[];



// CPM helper functions
DWORD WINAPI	CPMCmdHandler(LPVOID args);	// thread
BOOL			SPMDataHandler(SPMDATA *SPMDataIn);
BOOL			SendCPMData(CPMDATA CPMDataOut);
BOOL			RetrieveLogFiles(const WCHAR *wszDirPath, const SYSTEMTIME *stFromDate, const SYSTEMTIME *stToDate,
    FILE_TRANSFER_DATA *pLogFilesList);

//ClientPluginManager()
// --
BOOL ClientPluginManager(RM_COMMAND cmd, const CPMARGS *pCPMArgs, const SPMDATA *pSPMDataIn) {

    CPM_THREADARGS	*pThreadArgs = NULL;
    HANDLE			hCPMCmdHandlerThread;

    int iRetVal;

    if ((pThreadArgs = (CPM_THREADARGS*)malloc(sizeof(CPM_THREADARGS))) == NULL) {
        PrintToStatusWindow(L"CPM: malloc error:CPMThreadArgs");
        return FALSE;
    }
    memset(pThreadArgs, 0, sizeof(CPM_THREADARGS));

    if (pCPMArgs == NULL) {
        // if CCMArgs is NULL, only 'cmd' or 'cmd with SCMDataIn' is received

        // copy the command into CCM_THREADARGS structure
        pThreadArgs->cmd = cmd;

        // check if SCMDataIn is NULL or not
        if (pSPMDataIn != NULL) {
            // if it is not NULL, copy the SCMDataIn into CCM_THREADARGS structure
            memcpy(&pThreadArgs->SPMDataIn, pSPMDataIn, sizeof(SPMDATA));
        }
    } else {
        // pCCMArgs is not NULL. Hence, copy the arguments received into CCM_THREADARGS structure.
        pThreadArgs->cmd = pCPMArgs->cmd;
        memcpy(&pThreadArgs->CPMArgs, pCPMArgs, sizeof(CPMARGS));
    }

    // after copying the arguments, invoke a new thread to 
    // handle the command and immediately return to the calling function.
    hCPMCmdHandlerThread = CreateThread(NULL,									// security attributes
        0,										// stack size
        (LPTHREAD_START_ROUTINE)CPMCmdHandler,	// start address of ThreadProc
        (LPVOID)pThreadArgs,					// pointer to variable to be passed to the thread
        0,										// default creation flags
        NULL);									// threadID variable

    if (hCPMCmdHandlerThread == NULL) {
        iRetVal = GetLastError();
        PrintToStatusWindow(L"CPM: Thread creation error");
        return FALSE;
    }

    return TRUE;

}// ClientPluginManager()


// CPMCmdHandler()
// --
DWORD WINAPI CPMCmdHandler(LPVOID lpArgs) {
    extern CLIENTCONF g_ClientConf;

    CPM_THREADARGS	*pMyArgs = NULL;
    CCMARGS			*pCCMArgs = NULL;

    // first, retrieve the arguments
    if ((pMyArgs = (CPM_THREADARGS*)lpArgs) == NULL) {
        PrintToStatusWindow(L"CPMCmdHandler: NULL args");
        return FALSE;
    };


    __try {
        if ((pCCMArgs = (CCMARGS*)malloc(sizeof(CCMARGS))) == NULL)
            return FALSE;

        switch (pMyArgs->cmd) {
            // ** cases for handling commands **

            case CPM_SENDDATA:
            {
                // Fill the CCMArgs structure so that CPMData can be 
                // forwarded to CCM and then to the server.
                pCCMArgs->cmd = CCM_SENDDATA;

                // insert command for the SPM
                pCCMArgs->CCMDataType.CPMDataOut.cmd = SPM_FWD_DATA;

                // copy the plug-in to CPMData
                memcpy(&pCCMArgs->CCMDataType.CPMDataOut.PData, &pMyArgs->CPMArgs.PData, sizeof(PDATA));

                ClientComModule(0, pCCMArgs, NULL);
                return 0;
            }


            // ** cases for handling notifications **

            case CPMN_DATA_IN:
            {
                // check the command from SPM
                switch (pMyArgs->SPMDataIn.cmd) {
                    case CPM_HELLO:
                    {
                        // send the the ClientConf structure to server
                        memset(pCCMArgs, 0, sizeof(CCMARGS));
                        pCCMArgs->cmd = CCM_SENDDATA;
                        pCCMArgs->CCMDataType.CPMDataOut.cmd = SPM_HELLO;
                        memcpy(&pCCMArgs->CCMDataType.CPMDataOut.PData.PDataType.ClientConf,
                            &g_ClientConf, sizeof(CLIENTCONF));

                        ClientComModule(0, pCCMArgs, NULL);
                        return 0;
                    }
                    case CPM_QUERY:
                    {
                        // send the the ClientConf structure to server
                        memset(pCCMArgs, 0, sizeof(CCMARGS));
                        pCCMArgs->cmd = CCM_SENDDATA;
                        pCCMArgs->CCMDataType.CPMDataOut.cmd = SPM_CLIENT_CONF;
                        memcpy(&pCCMArgs->CCMDataType.CPMDataOut.PData.PDataType.ClientConf,
                            &g_ClientConf, sizeof(CLIENTCONF));

                        ClientComModule(0, pCCMArgs, NULL);

                        return 0;
                    }

                    case CPM_FWD_DATA:
                    {
                        int nDLL;
                        // Iterate over the DLLInfo structure and 
                        // call ProcessCmd() of all the plugin interfaces.
                        for (nDLL = 0; nDLL < g_ClientConf.nPlugins; ++nDLL) {
                            if (g_DLLInfo[nDLL].hProcessCmd != NULL)
                                g_DLLInfo[nDLL].hProcessCmd(IF_CMD_PLUGIN, &pMyArgs->SPMDataIn.MNTRData, NULL);
                        }
                        return 0;
                    }
                }
                return 0;
            }

            // ** cases for handling error codes **



        }//switch(MyArgs->cmd)
        return 0;
    }
    __finally {
        // cleanup
        if (pCCMArgs)
            free(pCCMArgs);
        if (pMyArgs)
            free(pMyArgs);
    }

}//CPMCmdHandler()


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