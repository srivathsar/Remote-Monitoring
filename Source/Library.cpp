
// ** Remote Monitoring **

// Library.cpp : Common functions used by both ServerRM and ClientRM

#include <windows.h>	
#include <commctrl.h>	// InitCommControls()
#include <stdio.h>		// _wfopen_s(), fclose(), fprintf(), fscanf()
#include <stdlib.h>		// errno
#include <ERRNO.H>		
#include <tchar.h> 
#include <wchar.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>			// _write
#include <share.h>

#include "..\Include\RMDefs.h"
#include "..\Include\DataStructures.h"


// PrintToStatusWindow()
// --
BOOL PrintToStatusWindow(const WCHAR *wszMsg) {
    extern HWND hListBox;

    int iRetVal;
    SYSTEMTIME stCurrentTime;
    WCHAR szStatusMsg[MAX_SW_MSG + 1];

    // get time
    GetLocalTime(&stCurrentTime);

    //construct the string to be displayed
    swprintf_s(szStatusMsg, MAX_SW_MSG + 1, L"[%02d:%02d:%02d.%03d] %s", stCurrentTime.wHour,
        stCurrentTime.wMinute, stCurrentTime.wSecond, stCurrentTime.wMilliseconds, wszMsg);

    // add the string
    iRetVal = SendMessage(hListBox, LB_ADDSTRING, 0, (LPARAM)szStatusMsg);
    if (iRetVal == LB_ERR || iRetVal == LB_ERRSPACE)
        return FALSE;

    // make sure the added string is visible
    iRetVal = SendMessage(hListBox, LB_SETTOPINDEX, iRetVal, 0);
    if (iRetVal == LB_ERR)
        return FALSE;

    return TRUE;

}

// PrintToStatusBar()
// --

BOOL PrintToStatusBar(const WCHAR *wszMsg, int iPart) {
    extern HWND hStatusBar;

    int iRetVal;

    // returns TRUE if successful, FALSE otherwise
    iRetVal = SendMessage(hStatusBar, SB_SETTEXT, iPart, (LPARAM)(LPSTR)wszMsg);

    return iRetVal;
}

// WriteToLogFile()
// --
int WriteToLogFile(const WCHAR *pwszFilePath, const WCHAR *pwszFromModule, const WCHAR *pwszFunction,
    const WCHAR *pwszData, int iError) {
    FILE *fp = NULL;
    int iRetVal, iDataLen;

    SYSTEMTIME stCurrentTime;

    WCHAR wszLogFileLine[MAX_LOGFILE_LINE + 1];

    // open file in append mode
    if ((iRetVal = _wfopen_s(&fp, pwszFilePath, L"a")) != 0) {
        return iRetVal;
    }

    iDataLen = wcsnlen_s(pwszData, MAX_LOGFILE_LINE + 1);
    if (iDataLen == MAX_LOGFILE_LINE + 1)
        return 1;

    // get time
    GetLocalTime(&stCurrentTime);

    if (pwszData == NULL) {
        // write header to newly created file
        swprintf_s(wszLogFileLine, MAX_LOGFILE_LINE + 1,
            L"**LogFile Format**\n[Timestamp][ModuleName][FunctionName][Optional ErrorCode]Message\n\n");
    } else {
        if (iError > 0) {
            //construct the string to be written
            swprintf_s(wszLogFileLine, MAX_LOGFILE_LINE + 1, L"[%02d:%02d:%02d.%03d][%s][%s][errno %d]%s\n", stCurrentTime.wHour,
                stCurrentTime.wMinute, stCurrentTime.wSecond, stCurrentTime.wMilliseconds, pwszFromModule,
                pwszFunction, iError, pwszData);
        } else {
            //construct the string to be written
            swprintf_s(wszLogFileLine, MAX_LOGFILE_LINE + 1, L"[%02d:%02d:%02d.%03d][%s][%s]%s\n",
                stCurrentTime.wHour, stCurrentTime.wMinute, stCurrentTime.wSecond, stCurrentTime.wMilliseconds,
                pwszFromModule, pwszFunction, pwszData);
        }
    }

    // write the data
    iDataLen = wcsnlen_s(wszLogFileLine, MAX_LOGFILE_LINE);
    if ((iRetVal = fwprintf_s(fp, L"%s", wszLogFileLine)) < 0)
        return 2;

    fclose(fp);

    return ERROR_SUCCESS;
}

// ListDir()
// Function to list the files under the specified directory.
int ListDir(const WCHAR *pwszDirPath, const WCHAR *pwszExt, FILELIST flFileList[], int nMaxFiles) {
    HANDLE h;
    int nFiles;
    WIN32_FIND_DATA FileData;
    BOOL fNextFile;
    WCHAR wszSearchPath[MAX_PATH + 1];

    // validate the nMaxFiles argument
    if (nMaxFiles <= 0) {
        return 0;
    }

    // initialize the 'number of files' variable
    nFiles = -1;

    // concatenate the extension to the file path
    swprintf_s(wszSearchPath, MAX_PATH + 1, L"%s\\%s", pwszDirPath, pwszExt);


    /* check whether path exists or not */
    h = (HANDLE)FindFirstFile(wszSearchPath, &FileData);
    if (h == INVALID_HANDLE_VALUE) {
        int iRetVal = GetLastError();
        return -1;
    }

    // copy the first path to ppFileList
    ++nFiles;
    swprintf_s(flFileList[nFiles].wszFileName, L"%s", FileData.cFileName);

    while ((fNextFile = FindNextFile(h, &FileData))) {
        if (nFiles < nMaxFiles) {
            ++nFiles;

            // copy it to ppFileList
            swprintf_s(flFileList[nFiles].wszFileName, MAX_PATH + 1, L"%s", FileData.cFileName);
        } else
            break;

    }

    // close the handle
    FindClose(h);

    return nFiles + 1;

}// ListDir()