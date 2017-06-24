
#include <shlwapi.h>
#include <stdio.h>
#include <windows.h>
#include <WindowsX.h>
#include <wchar.h>
#include <io.h>
#include <wchar.h>
#include <share.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <errno.h>
#include "..\..\..\Include\RMDefs.h"
#include "..\..\..\Include\DataStructures.h"

#define MOUSE_SHOT_AREA		25
#define MOUSE_SHOT_ADJUST	12
#define IDT_OFFLINE_TIMER	50000
#define IDT_REALTIME_TIMER	50001
#define SGCONFIGUPDATE		L"SGConfigUpdate"

extern "C" __declspec(dllexport) BOOL InstallSGHook(WCHAR *pszSGDir);
extern "C" __declspec(dllexport) BOOL RemoveSGHook();
LRESULT CALLBACK SGGetMsgProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SGCallWndProc(int nCode, WPARAM wParam, LPARAM lParam);

// This function process a click based on the shot and sets all parameters need for taking the snap
BOOL ProcessClick(int xPos, int yPos, HWND hwnd);

// This fucntion checks whether this application needs to be monitored or not
BOOL ProcessThisApp();

// This function takes the actual snap and stores it in a bmp file
BOOL TakeSnap(HWND hWnd, long ScreenWidth, long ScreenHeight, long ShotWidth, long ShotHeight, long xStartPt, long yStartPt, WCHAR szFileName[MAX_PATH + 1]);

// This thread is called only once and used to read the configuration file to set the inital parameters for the application
DWORD WINAPI InitHookProc(LPVOID lpParam);

DWORD WINAPI UpdateConfigProc(LPVOID lpParam);

BOOL InitHook();

// This function is used to create the InitHookProc and ConfigUpdateProc threads
BOOL InitProcess();

// This function is used to change the set parameters based on the configuration file update
BOOL UpdateConfig();

// This funtion is used to set both real time and offline timer
BOOL SetTimerFunc();

// This function is used to kill both real time and offline timers
BOOL KillTimerFunc();

// This function is used to check whether applications messages has to ignored or not
BOOL NeverHookProcessList();
