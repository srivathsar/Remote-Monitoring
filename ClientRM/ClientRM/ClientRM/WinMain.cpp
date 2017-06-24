
// * ClientRM project *
// WinMain.cpp: Main header file for the ClientRM exe

#include "ClientInclude.h"

HINSTANCE hMainInstance;
HWND hMainWnd;
HWND hMsgOnlyWnd;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow) {
    MSG MainWndMsg;
    WNDCLASS MainWndClass;
    WNDCLASS MsgOnlyWndClass;

    WCHAR szAppName[] = L"ClientRM";
    WCHAR szMsgOnlyWndClassName[] = L"ClientRM_MOWndClass";

    hMainInstance = hInstance;

    int iRetVal;

    // ClientRM UI Window Class
    MainWndClass.style = CS_HREDRAW | CS_VREDRAW;
    MainWndClass.lpfnWndProc = WndProc;
    MainWndClass.cbClsExtra = 0;
    MainWndClass.cbWndExtra = 0;
    MainWndClass.hInstance = hMainInstance;
    MainWndClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    MainWndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    MainWndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    MainWndClass.lpszMenuName = NULL;
    MainWndClass.lpszClassName = szAppName;

    if (!RegisterClass(&MainWndClass)) {
        iRetVal = GetLastError();
        MessageBox(NULL, TEXT("This program requires Windows NT!"), szAppName, MB_ICONERROR);
        return 0;
    }

    // Create the main window
    hMainWnd = CreateWindow(szAppName,			// class name
        szAppName,				// caption
        WS_CAPTION |
        WS_MINIMIZEBOX |
        WS_SYSMENU,				// window style
        CW_USEDEFAULT,			// initial X position
        CW_USEDEFAULT,			// initial Y position
        640,					// initial X size
        480,					// initial Y size
        NULL,					// parent window handle
        NULL,					// window menu handle
        hMainInstance,			// program instance handle
        NULL);

    // exit if window was not created
    if (!hMainWnd) {
        MessageBox(0, L"Window creation error. Cannot continue.", 0, 0);
        return 0;
    }


    // ClientRM message-only window class
    MsgOnlyWndClass.style = CS_HREDRAW | CS_VREDRAW;
    MsgOnlyWndClass.lpfnWndProc = MsgOnlyWndProc;
    MsgOnlyWndClass.cbClsExtra = 0;
    MsgOnlyWndClass.cbWndExtra = 0;
    MsgOnlyWndClass.hInstance = hMainInstance;
    MsgOnlyWndClass.hIcon = NULL;
    MsgOnlyWndClass.hCursor = NULL;
    MsgOnlyWndClass.hbrBackground = NULL;
    MsgOnlyWndClass.lpszMenuName = NULL;
    MsgOnlyWndClass.lpszClassName = szMsgOnlyWndClassName;

    if (!RegisterClass(&MsgOnlyWndClass)) {
        MessageBox(NULL, TEXT("This program requires Windows NT!"), szAppName, MB_ICONERROR);
        return 0;
    }

    // Initialize common controls
    InitCommonControls();

    // create the message-only window
    hMsgOnlyWnd = CreateWindow(szMsgOnlyWndClassName,
        NULL,
        WS_CHILD,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        HWND_MESSAGE,
        NULL,
        hMainInstance,
        NULL);
    if (!hMsgOnlyWnd) {
        iRetVal = GetLastError();
        MessageBox(0, L"Message-only window creation error. Cannot continue.", 0, 0);
        return 0;
    }

    ShowWindow(hMainWnd, iCmdShow);
    UpdateWindow(hMainWnd);

    while (GetMessage(&MainWndMsg, NULL, 0, 0)) {
        TranslateMessage(&MainWndMsg);
        DispatchMessage(&MainWndMsg);
    }

    return MainWndMsg.wParam;

}//WinMain()