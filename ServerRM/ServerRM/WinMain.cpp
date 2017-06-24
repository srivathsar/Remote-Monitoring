
// * ServerRM Project *
// WinMain.cpp: Entry point for the application.

#include "ServerInclude.h"

HWND hMainWnd;
HWND hMsgOnlyWnd;
HCURSOR hCursorWait;
HINSTANCE hMainInstance;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow) {
    MSG MainWndMsg;
    WNDCLASS MainWndClass;
    WNDCLASS MsgOnlyWndClass;

    WCHAR szAppName[] = L"ServerRM";
    WCHAR szMsgOnlyWndClassName[] = L"ServerRMMOWndClass";

    hMainInstance = hInstance;

    int iScreenX, iScreenY, iWndX, iWndY, iWidth, iHeight;
    RECT rcMainWnd;

    // RMServer UI Window Class
    MainWndClass.style = CS_HREDRAW | CS_VREDRAW;
    MainWndClass.lpfnWndProc = WndProc;
    MainWndClass.cbClsExtra = 0;
    MainWndClass.cbWndExtra = 0;
    MainWndClass.hInstance = hMainInstance;
    MainWndClass.hIcon = LoadIcon(hMainInstance, MAKEINTRESOURCE(IDC_MAIN_WINDOW));
    MainWndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    MainWndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    MainWndClass.lpszMenuName = MAKEINTRESOURCE(IDM_MAIN_WINDOW);
    MainWndClass.lpszClassName = szAppName;

    if (!RegisterClass(&MainWndClass)) {
        MessageBox(NULL, TEXT("This program requires Windows NT!"), szAppName, MB_ICONERROR);
        return 0;
    }

    // Message-only window class
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

    // Create the main window
    hMainWnd = CreateWindow(szAppName,			// class name
        szAppName,				// caption
        WS_CAPTION |
        WS_MINIMIZEBOX |
        WS_SYSMENU,				// window style
        CW_USEDEFAULT,			// initial X position
        CW_USEDEFAULT,			// initial Y position
        800,					// initial X size
        600,					// initial Y size
        NULL,					// parent window handle
        NULL,					// window menu handle
        hMainInstance,				// program instance handle
        NULL);

    // exit if window was not created
    if (!hMainWnd) {
        MessageBox(0, L"Window creation error. Cannot continue.", 0, 0);
        return 0;
    }

    // centre the main window in the screen

    // get the screen co-ordinates
    iScreenX = GetSystemMetrics(SM_CXSCREEN);
    iScreenY = GetSystemMetrics(SM_CYSCREEN);

    // get window rect and calculate the main window dimensions
    GetWindowRect(hMainWnd, &rcMainWnd);
    iWidth = rcMainWnd.right - rcMainWnd.left;
    iHeight = rcMainWnd.bottom - rcMainWnd.top;

    // calculate the new co-ordinates for the main window
    iWndX = iScreenX / 2 - iWidth / 2;
    iWndY = iScreenY / 2 - iHeight / 2;

    MoveWindow(hMainWnd, iWndX, iWndY, iWidth, iHeight, FALSE);

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
        MessageBox(0, L"Message-only window creation error. Cannot continue.", 0, 0);
        return 0;
    }

    // load the wait cursor (hour-glass)
    hCursorWait = LoadCursor(NULL, IDC_WAIT);

    ShowWindow(hMainWnd, iCmdShow);
    UpdateWindow(hMainWnd);

    while (GetMessage(&MainWndMsg, NULL, 0, 0)) {
        TranslateMessage(&MainWndMsg);
        DispatchMessage(&MainWndMsg);
    }

    return MainWndMsg.wParam;

}//WinMain()
