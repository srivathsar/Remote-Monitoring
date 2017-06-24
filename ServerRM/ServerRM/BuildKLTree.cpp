
// ** ServerRM project **
// BuildKLTree.cpp :

#include "ServerInclude.h"

HWND hTree;

// Private helper functions
HTREEITEM AddNewProcessNode(const WCHAR *pwszProcessName);
HTREEITEM AddNewTimeNode(HTREEITEM hParentProcessNode, const SYSTEMTIME *pstProcessCreationTime);
HTREEITEM AddNewHandleNode(HTREEITEM hParent, KLWNDDATA *pKLWndData,
    const WCHAR *pwszKeysPressed, const int iKeyCount, const int iNodeIndex);

BOOL BuildKLTree(const WCHAR *pwszFilePath, HWND hDlg) {
    // File management
    FILE *fpLogFile = NULL;

    // LogFile Data
    int nRecords;
    LOGFILEDATA LogFileData;
    LOGFILEDATA **pLogFileData = NULL;

    // Tree management
    HTREEITEM		hRoot = NULL;
    HTREEITEM		hProcessNode = NULL;
    HTREEITEM		hTimeNode = NULL;
    HTREEITEM		hParentNode = NULL;
    HTREEITEM		hHandleNode = NULL;

    HTREEITEM		hMatchedProcessNode = NULL;
    HTREEITEM		hMatchedTimeNode = NULL;
    HTREEITEM		hMatchedHandleNode = NULL;

    HTREEITEM		hFirstChild = NULL;

    if (hFirstChild != NULL) {

    }
    OutputDebugString(L"hFirstChild != NULL");
    TVINSERTSTRUCT	tvisToInsert = { 0 };
    TVITEM			tviCurNode;

    TVITEM tviAppendKeys = { 0 };

    KLHANDLENODE	*pKLHandleNode = NULL;
    KLTIMENODE		*pKLTimeNode = NULL;

    BOOL fProcessMatch = FALSE;
    BOOL fTimeMatch = FALSE;
    BOOL fHandleMatch = FALSE;
    BOOL fAddComplete = FALSE;

    int iRecordIndex, iHandleIndex, k;
    int iRetVal;

    hTree = GetDlgItem(hDlg, IDC_KLM_OFF_TREE);

    // open the log file
    iRetVal = _wfopen_s(&fpLogFile, pwszFilePath, L"rb");
    if (fpLogFile == NULL)
        return FALSE;


    // find out the number of records in the file
    nRecords = 0;
    while (fread(&LogFileData, sizeof(LOGFILEDATA), 1, fpLogFile)) {
        if (feof(fpLogFile))
            break;

        ++nRecords;
    }

    // close the file
    fclose(fpLogFile);

    // allocate memory to store the read records

    __try {
        // memory for the table of pointers
        if ((pLogFileData = (LOGFILEDATA**)malloc(sizeof(LOGFILEDATA*) * nRecords)) == NULL)
            return FALSE;


        // memory for individual records
        for (iRecordIndex = 0; iRecordIndex < nRecords; ++iRecordIndex) {
            pLogFileData[iRecordIndex] = NULL;
            if ((pLogFileData[iRecordIndex] = (LOGFILEDATA*)malloc(sizeof(LOGFILEDATA))) == NULL)
                return FALSE;
        }

        // now, open the file and store the records in the allocated memory
        // open the log file
        iRetVal = _wfopen_s(&fpLogFile, pwszFilePath, L"rb");
        if (fpLogFile == NULL)
            return FALSE;

        nRecords = 0;

        while (fread(&LogFileData, sizeof(LOGFILEDATA), 1, fpLogFile)) {
            if (feof(fpLogFile))
                break;

            // copy to temp structure
            memcpy(pLogFileData[nRecords], &LogFileData, sizeof(LOGFILEDATA));

            nRecords++;
        }

        // close the file
        fclose(fpLogFile);

        // now, build the tree with the LogFileData copied
        for (iRecordIndex = 0; iRecordIndex < nRecords; ++iRecordIndex) {
            //reset flags
            fProcessMatch = fTimeMatch = fHandleMatch = FALSE;

            if (TreeView_GetCount(hTree) == 0)	// no nodes, empty tree
            {
                // create a new process node at the root level
                if ((hProcessNode = AddNewProcessNode(pLogFileData[iRecordIndex]->ProcessData.wszEXEName)) == NULL) {
                    // error
                    continue;	// goto next record
                }
                hParentNode = hProcessNode;

                // create a new time node under this process node
                if ((hTimeNode = AddNewTimeNode(hProcessNode,
                    &pLogFileData[iRecordIndex]->ProcessData.stStartTime)) == NULL) {
                    // error
                    continue;	// goto next record
                }
                hParentNode = hTimeNode;

                // create new handle nodes
                for (iHandleIndex = pLogFileData[iRecordIndex]->WndCount - 1; iHandleIndex >= 0; --iHandleIndex) {
                    if ((hHandleNode = AddNewHandleNode(hParentNode, &pLogFileData[iRecordIndex]->KLWndData[iHandleIndex],
                        pLogFileData[iRecordIndex]->Keys, pLogFileData[iRecordIndex]->KeyCount, iHandleIndex)) == NULL) {
                        ;// error
                    }
                    hParentNode = hHandleNode;
                }

            }// if(TreeView_GetCount())

            else {
                HTREEITEM hCurNode = NULL;
                HTREEITEM hMatchedProcessNode = NULL;
                HTREEITEM hMatchedTimeNode = NULL;
                WCHAR wszCurNodeText[MAX_FILE_NAME + 1] = L"";

                hCurNode = TreeView_GetRoot(hTree);

                // ** match the process nodes first **
                do {
                    // get the process node's display text
                    memset(&tviCurNode, 0, sizeof(TVITEM));
                    tviCurNode.mask = TVIF_TEXT;
                    tviCurNode.pszText = wszCurNodeText;
                    tviCurNode.cchTextMax = MAX_FILE_NAME + 1;
                    tviCurNode.hItem = hCurNode;

                    if (SendMessage(hTree, TVM_GETITEM, 0, (LPARAM)&tviCurNode) == FALSE) {
                        iRetVal = GetLastError();
                        // handle error : set flag, continue to next record
                    }

                    if (wcscmp(pLogFileData[iRecordIndex]->ProcessData.wszEXEName, tviCurNode.pszText) != 0) {
                        continue;	// goes to next process node
                    } else {
                        fProcessMatch = TRUE;
                        hMatchedProcessNode = hCurNode;
                        break;	// jump out of the process-node do-while loop
                    }
                } while ((hCurNode = TreeView_GetNextSibling(hTree, hCurNode)) != NULL);

                if (fProcessMatch == FALSE) {
                    // add a new process node
                    if ((hProcessNode = AddNewProcessNode(pLogFileData[iRecordIndex]->ProcessData.wszEXEName)) == NULL) {
                        // display error
                        continue;
                    }
                    hParentNode = hProcessNode;

                    // add a new time node
                    if ((hTimeNode = AddNewTimeNode(hParentNode,
                        &pLogFileData[iRecordIndex]->ProcessData.stStartTime)) == NULL) {
                        // display error
                        continue;
                    }
                    hParentNode = hTimeNode;

                    // add new handle nodes
                    for (iHandleIndex = pLogFileData[iRecordIndex]->WndCount - 1; iHandleIndex >= 0; --iHandleIndex) {
                        if ((hHandleNode = AddNewHandleNode(hParentNode, &pLogFileData[iRecordIndex]->KLWndData[iHandleIndex],
                            pLogFileData[iRecordIndex]->Keys, pLogFileData[iRecordIndex]->KeyCount, iHandleIndex)) == NULL) {
                            ;// error
                        }
                        hParentNode = hHandleNode;
                    }

                    // goto next record
                    fAddComplete = TRUE;
                    continue;

                }// if( fProcessMatch == FALSE )

                // ** if process node was matched, match time nodes **

                // first, goto the first child of the matched process node
                if ((hCurNode = TreeView_GetChild(hTree, hMatchedProcessNode)) == NULL) {
                    // add new time node
                    if ((hTimeNode = AddNewTimeNode(hMatchedProcessNode,
                        &pLogFileData[iRecordIndex]->ProcessData.stStartTime)) == NULL) {
                        // display error
                        continue;
                    }

                    // add new handle nodes
                    hParentNode = hTimeNode;
                    for (iHandleIndex = pLogFileData[iRecordIndex]->WndCount - 1; iHandleIndex >= 0; --iHandleIndex) {
                        if ((hHandleNode = AddNewHandleNode(hParentNode, &pLogFileData[iRecordIndex]->KLWndData[iHandleIndex],
                            pLogFileData[iRecordIndex]->Keys, pLogFileData[iRecordIndex]->KeyCount, iHandleIndex)) == NULL) {
                            ;// error
                        }
                        hParentNode = hHandleNode;
                    }

                    // goto next record
                    fAddComplete = TRUE;
                    continue;
                }

                // loop for scanning the time nodes
                do {
                    // get pointer to KLTIMENODE structure of the current time node
                    memset(&tviCurNode, 0, sizeof(TVITEM));
                    tviCurNode.mask = TVIF_PARAM;
                    tviCurNode.hItem = hCurNode;

                    if ((iRetVal = TreeView_GetItem(hTree, &tviCurNode)) == FALSE) {
                        iRetVal = GetLastError();
                        // handle error : set flag, continue to next record
                    }

                    pKLTimeNode = (KLTIMENODE*)tviCurNode.lParam;

                    // match the time values
                    if (pLogFileData[iRecordIndex]->ProcessData.stStartTime.wHour == pKLTimeNode->wHour
                        && pLogFileData[iRecordIndex]->ProcessData.stStartTime.wMinute == pKLTimeNode->wMinute
                        && pLogFileData[iRecordIndex]->ProcessData.stStartTime.wSecond == pKLTimeNode->wSecond
                        && pLogFileData[iRecordIndex]->ProcessData.stStartTime.wMilliseconds == pKLTimeNode->wMilliseconds) {
                        fTimeMatch = TRUE;
                        hMatchedTimeNode = hCurNode;
                        break;		// jump out of the time-node do-while loop
                    }

                } while ((hCurNode = TreeView_GetNextSibling(hTree, hCurNode)) != NULL);

                if (fTimeMatch == FALSE) {
                    // add new time node
                    if ((hTimeNode = AddNewTimeNode(hMatchedProcessNode,
                        &pLogFileData[iRecordIndex]->ProcessData.stStartTime)) == NULL) {
                        // display error
                        continue;
                    }

                    // add new handle nodes
                    hParentNode = hTimeNode;
                    for (iHandleIndex = pLogFileData[iRecordIndex]->WndCount - 1; iHandleIndex >= 0; --iHandleIndex) {
                        if ((hHandleNode = AddNewHandleNode(hParentNode, &pLogFileData[iRecordIndex]->KLWndData[iHandleIndex],
                            pLogFileData[iRecordIndex]->Keys, pLogFileData[iRecordIndex]->KeyCount, iHandleIndex)) == NULL) {
                            ;// error
                        }
                        hParentNode = hHandleNode;
                    }

                    // goto next record
                    fAddComplete = TRUE;
                    continue;
                }

                // ** if time node was matched, match handle nodes **

                // first, goto child of the matched time node, which is a handle node
                if ((hFirstChild = TreeView_GetChild(hTree, hCurNode)) == NULL) {
                    // add new handle nodes
                    hParentNode = hMatchedTimeNode;
                    for (iHandleIndex = pLogFileData[iRecordIndex]->WndCount - 1; iHandleIndex >= 0; --iHandleIndex) {
                        if ((hHandleNode = AddNewHandleNode(hParentNode, &pLogFileData[iRecordIndex]->KLWndData[iHandleIndex],
                            pLogFileData[iRecordIndex]->Keys, pLogFileData[iRecordIndex]->KeyCount, iHandleIndex)) == NULL) {
                            ;// error
                        }
                        hParentNode = hHandleNode;
                    }

                    // goto next record
                    fAddComplete = TRUE;
                    continue;
                }

                // loop for scanning handle nodes

                // loop for scanning KLWNDDATA entries
                hCurNode = hFirstChild;
                for (iHandleIndex = pLogFileData[iRecordIndex]->WndCount - 1;
                    iHandleIndex >= 0 && hCurNode != NULL;
                    --iHandleIndex, hCurNode = TreeView_GetChild(hTree, hCurNode)) {
                    fHandleMatch = FALSE;

                    // get the lParam value, iRecordIndex.e., the pointer to KLHANDLENODE structure
                    memset(&tviCurNode, 0, sizeof(TVITEM));
                    tviCurNode.mask = TVIF_PARAM | TVIF_HANDLE;
                    tviCurNode.hItem = hCurNode;

                    if (TreeView_GetItem(hTree, &tviCurNode) == FALSE) {
                        iRetVal = GetLastError();
                        MessageBox(hDlg, L"Error occured during tree build!", L"Build Tree", MB_ICONEXCLAMATION);
                        continue;
                    }

                    pKLHandleNode = (KLHANDLENODE*)tviCurNode.lParam;

                    // match the handle
                    if (pLogFileData[iRecordIndex]->KLWndData[iHandleIndex].hWnd == pKLHandleNode->hWnd) {
                        hMatchedHandleNode = hCurNode;
                        fHandleMatch = TRUE;
                    } else {
                        // add new handle nodes
                        hParentNode = hCurNode;
                        fHandleMatch = FALSE;
                        break;
                    }

                }// for iHandleIndex

                if (fHandleMatch == TRUE && iHandleIndex == -1) {
                    KLHANDLENODE *pKLHandleNode = NULL;

                    // get the KLHANDLENODE structure
                    tviAppendKeys.mask = TVIF_PARAM;
                    tviAppendKeys.hItem = hMatchedHandleNode;

                    if (TreeView_GetItem(hTree, &tviAppendKeys) == FALSE) {
                        ; // error

                    }

                    pKLHandleNode = (KLHANDLENODE*)tviAppendKeys.lParam;

                    wcscat_s(pKLHandleNode->wszKeys, MAX_KLIF_KEYS + 1,
                        pLogFileData[iRecordIndex]->Keys);

                    continue; // append keys
                }

                // handle was not matched, add new nodes to the hParentNode
                for (k = iHandleIndex; k >= 0; --k) {
                    if ((hHandleNode = AddNewHandleNode(hParentNode, &pLogFileData[iRecordIndex]->KLWndData[k],
                        pLogFileData[iRecordIndex]->Keys, pLogFileData[iRecordIndex]->KeyCount, k)) == NULL) {
                        ;// error
                    }
                    hParentNode = hHandleNode;
                }

            }// end of if(TreeView_GetCount())'s else

        }// for iRecordIndex

        return TRUE;
    }
    __finally {
        for (iRecordIndex = 0; iRecordIndex < nRecords; ++iRecordIndex) {
            if (pLogFileData[iRecordIndex])
                free(pLogFileData[iRecordIndex]);
        }

        if (pLogFileData)
            free(pLogFileData);
    }

}// BuildKLTree()

HTREEITEM AddNewProcessNode(const WCHAR *pwszProcessName) {
    TVINSERTSTRUCT	tvisToInsert = { 0 };
    HTREEITEM		hNewNode;
    WCHAR			wszNodeText[MAX_FILE_NAME + 1];

    // copy the process name into local variable
    wcscpy_s(wszNodeText, MAX_FILE_NAME,
        pwszProcessName);

    //set up TVINSERTSTRUCT structure
    tvisToInsert.hParent = NULL;
    tvisToInsert.hInsertAfter = TVI_ROOT;
    tvisToInsert.item.mask = TVIF_TEXT | TVIF_CHILDREN;
    tvisToInsert.item.cChildren = 1;
    tvisToInsert.item.pszText = wszNodeText;
    tvisToInsert.item.cchTextMax = lstrlen(wszNodeText) + 1;

    //send message TVM_INSERTITEM
    if ((hNewNode = (HTREEITEM)SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tvisToInsert)) == NULL)
        return NULL;

    return hNewNode;
}

HTREEITEM AddNewTimeNode(HTREEITEM hParentProcessNode, const SYSTEMTIME *pstProcessCreationTime) {
    KLTIMENODE *pKLTimeNode = NULL;
    HTREEITEM hNewNode = NULL;
    TVINSERTSTRUCT	tvisToInsert = { 0 };
    WCHAR wszNodeText[MAX_TIMENODE_NAME + 1];
    BOOL fError = FALSE;

    __try {
        // allocate memory for the KLTIMENODE structure
        if ((pKLTimeNode = (KLTIMENODE*)malloc(sizeof(KLTIMENODE))) == NULL) {
            fError = TRUE;
            return NULL;
        }

        memset(pKLTimeNode, 0, sizeof(KLTIMENODE));

        // fill the structure
        pKLTimeNode->wHour = pstProcessCreationTime->wHour;
        pKLTimeNode->wMinute = pstProcessCreationTime->wMinute;
        pKLTimeNode->wSecond = pstProcessCreationTime->wSecond;
        pKLTimeNode->wMilliseconds = pstProcessCreationTime->wMilliseconds;

        // construct the node text to be displayed
        wsprintf(wszNodeText, L"%02u:%02u:%02u.%03u",
            pKLTimeNode->wHour, pKLTimeNode->wMinute,
            pKLTimeNode->wSecond, pKLTimeNode->wMilliseconds);

        //set up TVINSERTSTRUCT structure
        memset(&tvisToInsert, 0, sizeof(TVINSERTSTRUCT));
        tvisToInsert.hParent = hParentProcessNode;
        tvisToInsert.hInsertAfter = TVI_LAST;
        tvisToInsert.item.mask = TVIF_TEXT | TVIF_CHILDREN | TVIF_PARAM;
        tvisToInsert.item.cChildren = 1;
        tvisToInsert.item.pszText = wszNodeText;
        tvisToInsert.item.cchTextMax = lstrlen(wszNodeText) + 1;
        tvisToInsert.item.lParam = (LPARAM)pKLTimeNode;
        //		WriteToAddressFile(pKLTimeNode, 0);

                //send message TVM_INSERTITEM
        if ((hNewNode = (HTREEITEM)SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tvisToInsert)) == NULL) {
            fError = TRUE;
            return NULL;
        }
        return hNewNode;
    }
    __finally {
        if (fError == TRUE)
            if (pKLTimeNode)
                free(pKLTimeNode);
    }
}

HTREEITEM AddNewHandleNode(HTREEITEM hParent, KLWNDDATA *pKLWndData,
    const WCHAR *pwszKeysPressed, const int iKeyCount, const int iNodeIndex) {
    KLHANDLENODE *pKLHandleNode = NULL;
    HTREEITEM hNewNode = NULL;
    TVINSERTSTRUCT	tvisToInsert = { 0 };
    WCHAR wszNodeText[MAX_CLASS_NAME + 1];
    BOOL fError = FALSE;

    // construct the node text to be displayed
    wcscpy_s(wszNodeText, MAX_FILE_NAME + 1, pKLWndData->wszClassName);

    __try {
        // malloc the KLHANDLENODE structure to hold node information
        if ((pKLHandleNode = (KLHANDLENODE*)malloc(sizeof(KLHANDLENODE))) == NULL) {
            fError = TRUE;
            return NULL;
        }

        memset(pKLHandleNode, 0, sizeof(KLHANDLENODE));

        // fill the structure
        pKLHandleNode->hWnd = pKLWndData->hWnd;
        wcscpy_s(pKLHandleNode->wszWndTitle, MAX_WND_TITLE + 1, pKLWndData->wszWndTitle);
        //if( iKeyCount != 0 && iNodeIndex == 0 )
        if (iNodeIndex == 0)
            wcscpy_s(pKLHandleNode->wszKeys, MAX_KLIF_KEYS + 1, pwszKeysPressed);

        //set up TVINSERTSTRUCT structure
        memset(&tvisToInsert, 0, sizeof(TVINSERTSTRUCT));
        tvisToInsert.hParent = hParent;
        tvisToInsert.hInsertAfter = TVI_LAST;
        tvisToInsert.item.mask = TVIF_TEXT | TVIF_CHILDREN | TVIF_PARAM;
        tvisToInsert.item.cChildren = ((iNodeIndex == 0) ? 0 : 1);			// nNodeIndex == 0 implies that this handle node
        tvisToInsert.item.pszText = wszNodeText;							// will be a leaf node.
        tvisToInsert.item.cchTextMax = lstrlen(wszNodeText) + 1;
        tvisToInsert.item.lParam = (LPARAM)pKLHandleNode;
        //WriteToAddressFile(pKLHandleNode, 0);

        //send message TVM_INSERTITEM
        if ((hNewNode = (HTREEITEM)SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tvisToInsert)) == NULL) {
            fError = TRUE;
            return NULL;
        }

        return hNewNode;
    }
    __finally {
        if (fError == TRUE)
            if (pKLHandleNode)
                free(pKLHandleNode);
    }
}

BOOL UpdateRTTree(HWND hDlg, const PDATA *pPData) {
    // Tree management
    HTREEITEM		hRoot = NULL;
    HTREEITEM		hProcessNode = NULL;
    HTREEITEM		hTimeNode = NULL;
    HTREEITEM		hParentNode = NULL;
    HTREEITEM		hHandleNode = NULL;

    HTREEITEM		hMatchedProcessNode = NULL;
    HTREEITEM		hMatchedTimeNode = NULL;
    HTREEITEM		hMatchedHandleNode = NULL;

    HTREEITEM		hFirstChild = NULL;

    TVINSERTSTRUCT	tvisToInsert = { 0 };
    TVITEM			tviCurNode;

    TVITEM tviAppendKeys = { 0 };

    KLHANDLENODE	*pKLHandleNode = NULL;
    KLTIMENODE		*pKLTimeNode = NULL;

    BOOL fProcessMatch = FALSE;
    BOOL fTimeMatch = FALSE;
    BOOL fHandleMatch = FALSE;
    BOOL fAddComplete = FALSE;

    LOGFILEDATA *pKLRealTimeData = NULL;

    int iHandleIndex, iRetVal, k;

    if ((pKLRealTimeData = (LOGFILEDATA*)&pPData->PDataType.KLRTData) == NULL) {

        return FALSE;
    }

    hTree = GetDlgItem(hDlg, IDC_KLM_RT_TREE);


    if (TreeView_GetCount(hTree) == 0)	// no nodes, empty tree
    {
        // create a new process node at the root level
        if ((hProcessNode = AddNewProcessNode(pKLRealTimeData->ProcessData.wszEXEName)) == NULL) {
            // error
            return NULL;
        }
        hParentNode = hProcessNode;

        // create a new time node under this process node
        if ((hTimeNode = AddNewTimeNode(hProcessNode,
            &pKLRealTimeData->ProcessData.stStartTime)) == NULL) {
            // error
            return NULL;	// goto next record
        }
        hParentNode = hTimeNode;

        // create new handle nodes
        for (iHandleIndex = pKLRealTimeData->WndCount - 1; iHandleIndex >= 0; --iHandleIndex) {
            if ((hHandleNode = AddNewHandleNode(hParentNode, &pKLRealTimeData->KLWndData[iHandleIndex],
                pKLRealTimeData->Keys, pKLRealTimeData->KeyCount, iHandleIndex)) == NULL) {
                ;// error
            }
            hParentNode = hHandleNode;
        }

    }// if(TreeView_GetCount())

    else {
        HTREEITEM hCurNode = NULL;
        HTREEITEM hMatchedProcessNode = NULL;
        HTREEITEM hMatchedTimeNode = NULL;
        WCHAR wszCurNodeText[MAX_FILE_NAME + 1] = L"";

        hCurNode = TreeView_GetRoot(hTree);

        // ** match the process nodes first **
        do {
            // get the process node's display text
            memset(&tviCurNode, 0, sizeof(TVITEM));
            tviCurNode.mask = TVIF_TEXT;
            tviCurNode.pszText = wszCurNodeText;
            tviCurNode.cchTextMax = MAX_FILE_NAME + 1;
            tviCurNode.hItem = hCurNode;

            if (SendMessage(hTree, TVM_GETITEM, 0, (LPARAM)&tviCurNode) == FALSE) {
                iRetVal = GetLastError();
                // handle error : set flag, continue to next record
            }

            if (wcscmp(pKLRealTimeData->ProcessData.wszEXEName, tviCurNode.pszText) != 0) {
                continue;	// goes to next process node
            } else {
                fProcessMatch = TRUE;
                hMatchedProcessNode = hCurNode;
                break;	// jump out of the process-node do-while loop
            }
        } while ((hCurNode = TreeView_GetNextSibling(hTree, hCurNode)) != NULL);

        if (fProcessMatch == FALSE) {
            // add a new process node
            if ((hProcessNode = AddNewProcessNode(pKLRealTimeData->ProcessData.wszEXEName)) == NULL) {
                // display error
                return NULL;
            }
            hParentNode = hProcessNode;

            // add a new time node
            if ((hTimeNode = AddNewTimeNode(hParentNode,
                &pKLRealTimeData->ProcessData.stStartTime)) == NULL) {
                // display error
                return NULL;
            }
            hParentNode = hTimeNode;

            // add new handle nodes
            for (iHandleIndex = pKLRealTimeData->WndCount - 1; iHandleIndex >= 0; --iHandleIndex) {
                if ((hHandleNode = AddNewHandleNode(hParentNode, &pKLRealTimeData->KLWndData[iHandleIndex],
                    pKLRealTimeData->Keys, pKLRealTimeData->KeyCount, iHandleIndex)) == NULL) {
                    ;// error
                }
                hParentNode = hHandleNode;
            }

            // goto next record
            fAddComplete = TRUE;
            return NULL;

        }// if( fProcessMatch == FALSE )

        // ** if process node was matched, match time nodes **

        // first, goto the first child of the matched process node
        if ((hCurNode = TreeView_GetChild(hTree, hMatchedProcessNode)) == NULL) {
            // add new time node
            if ((hTimeNode = AddNewTimeNode(hMatchedProcessNode,
                &pKLRealTimeData->ProcessData.stStartTime)) == NULL) {
                // display error
                return NULL;
            }

            // add new handle nodes
            hParentNode = hTimeNode;
            for (iHandleIndex = pKLRealTimeData->WndCount - 1; iHandleIndex >= 0; --iHandleIndex) {
                if ((hHandleNode = AddNewHandleNode(hParentNode, &pKLRealTimeData->KLWndData[iHandleIndex],
                    pKLRealTimeData->Keys, pKLRealTimeData->KeyCount, iHandleIndex)) == NULL) {
                    ;// error
                }
                hParentNode = hHandleNode;
            }

            // goto next record
            fAddComplete = TRUE;
            return NULL;
        }

        // loop for scanning the time nodes
        do {
            // get pointer to KLTIMENODE structure of the current time node
            memset(&tviCurNode, 0, sizeof(TVITEM));
            tviCurNode.mask = TVIF_PARAM;
            tviCurNode.hItem = hCurNode;

            if ((iRetVal = TreeView_GetItem(hTree, &tviCurNode)) == FALSE) {
                iRetVal = GetLastError();
                // handle error : set flag, continue to next record
            }

            pKLTimeNode = (KLTIMENODE*)tviCurNode.lParam;

            // match the time values
            if (pKLRealTimeData->ProcessData.stStartTime.wHour == pKLTimeNode->wHour
                && pKLRealTimeData->ProcessData.stStartTime.wMinute == pKLTimeNode->wMinute
                && pKLRealTimeData->ProcessData.stStartTime.wSecond == pKLTimeNode->wSecond
                && pKLRealTimeData->ProcessData.stStartTime.wMilliseconds == pKLTimeNode->wMilliseconds) {
                fTimeMatch = TRUE;
                hMatchedTimeNode = hCurNode;
                break;		// jump out of the time-node do-while loop
            }

        } while ((hCurNode = TreeView_GetNextSibling(hTree, hCurNode)) != NULL);

        if (fTimeMatch == FALSE) {
            // add new time node
            if ((hTimeNode = AddNewTimeNode(hMatchedProcessNode,
                &pKLRealTimeData->ProcessData.stStartTime)) == NULL) {
                // display error
                return NULL;
            }

            // add new handle nodes
            hParentNode = hTimeNode;
            for (iHandleIndex = pKLRealTimeData->WndCount - 1; iHandleIndex >= 0; --iHandleIndex) {
                if ((hHandleNode = AddNewHandleNode(hParentNode, &pKLRealTimeData->KLWndData[iHandleIndex],
                    pKLRealTimeData->Keys, pKLRealTimeData->KeyCount, iHandleIndex)) == NULL) {
                    ;// error
                }
                hParentNode = hHandleNode;
            }

            // goto next record
            fAddComplete = TRUE;
            return NULL;
        }

        // ** if time node was matched, match handle nodes **

        // first, goto child of the matched time node, which is a handle node
        if ((hFirstChild = TreeView_GetChild(hTree, hCurNode)) == NULL) {
            // add new handle nodes
            hParentNode = hMatchedTimeNode;
            for (iHandleIndex = pKLRealTimeData->WndCount - 1; iHandleIndex >= 0; --iHandleIndex) {
                if ((hHandleNode = AddNewHandleNode(hParentNode, &pKLRealTimeData->KLWndData[iHandleIndex],
                    pKLRealTimeData->Keys, pKLRealTimeData->KeyCount, iHandleIndex)) == NULL) {
                    ;// error
                }
                hParentNode = hHandleNode;
            }

            // goto next record
            fAddComplete = TRUE;
            return NULL;
        }

        // loop for scanning handle nodes

        // loop for scanning KLWNDDATA entries
        hCurNode = hFirstChild;
        for (iHandleIndex = pKLRealTimeData->WndCount - 1;
            iHandleIndex >= 0;
            --iHandleIndex, hCurNode = TreeView_GetChild(hTree, hCurNode)) {
            fHandleMatch = FALSE;

            // get the lParam value, iRecordIndex.e., the pointer to KLHANDLENODE structure
            memset(&tviCurNode, 0, sizeof(TVITEM));
            tviCurNode.mask = TVIF_PARAM;
            tviCurNode.hItem = hCurNode;

            if (TreeView_GetItem(hTree, &tviCurNode) == FALSE) {
                iRetVal = GetLastError();
                // handle error : set flag, continue to next record
            }

            pKLHandleNode = (KLHANDLENODE*)tviCurNode.lParam;

            // match the handle
            if (pKLRealTimeData->KLWndData[iHandleIndex].hWnd == pKLHandleNode->hWnd) {
                hMatchedHandleNode = hCurNode;
                fHandleMatch = TRUE;
            } else {
                // add new handle nodes
                hParentNode = hCurNode;
                fHandleMatch = FALSE;
                break;
            }

        }// for iHandleIndex

        if (fHandleMatch == TRUE && iHandleIndex == -1) {
            KLHANDLENODE *pKLHandleNode = NULL;

            // get the KLHANDLENODE structure
            tviAppendKeys.mask = TVIF_PARAM;
            tviAppendKeys.hItem = hMatchedHandleNode;

            if (TreeView_GetItem(hTree, &tviAppendKeys) == FALSE) {
                ; // error

            }

            pKLHandleNode = (KLHANDLENODE*)tviAppendKeys.lParam;

            wcscat_s(pKLHandleNode->wszKeys, MAX_KLIF_KEYS + 1,
                pKLRealTimeData->Keys);

            return NULL;
        }

        // handle was not matched, add new nodes to the hParentNode
        for (k = iHandleIndex; k >= 0; --k) {
            if ((hHandleNode = AddNewHandleNode(hParentNode, &pKLRealTimeData->KLWndData[k],
                pKLRealTimeData->Keys, pKLRealTimeData->KeyCount, k)) == NULL) {
                ;// error
            }
            hParentNode = hHandleNode;
        }

    }// end of if(TreeView_GetCount())'s else

    return TRUE;
}
