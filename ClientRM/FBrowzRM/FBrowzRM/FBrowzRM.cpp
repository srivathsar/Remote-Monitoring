
// ** FBrowzRM project **
// FBrowzRM.cpp :

#include "FBIncludes.h"


// Global variables
WCHAR		g_wszCWD[MAX_PATH + 1];

// Helper Function Prototypes
int FBListDir(const WCHAR *pwszDirPath, const WCHAR *pwszExt, FBFILELIST fbFileList[], int nMaxFiles);
int FBListFiles(const WCHAR *pwszDirPath, const WCHAR *pwszExt, FBFILELIST fbFileList[], int nMaxFiles);


BOOL ProcessCmd(RM_COMMAND cmd, MNTRDATA *pMNTRData, PDATA __out **pPDataToCPM) {
    int iRetVal;

    // Pointer to the PDATA structure
    // which holds data to be sent to CPM.
    static PDATA *pMyPData = NULL;

    // FILELIST variable to hold absolute paths
    FBDATA *pFBData = NULL;

    // Handle to event which notifies CPM 
    // that data is ready to be sent.
    HANDLE hIFDataReadyEvent = NULL;

    switch (cmd) {
        case IF_INIT_PLUGIN:
        {
            // create FB directory
            if ((iRetVal = CreateDirectory(FP_FB_DIR, NULL)) == ERROR_PATH_NOT_FOUND) {
                return FALSE;
            }
            return TRUE;
        }


        case IF_TERM_PLUGIN:
        {

            return TRUE;
        }

        case IF_CMD_PLUGIN:
        {
            switch (pMNTRData->FBMData.cmd) {
                case FBIF_GETDRIVES:
                {
                    int nDrives = 0;
                    unsigned long uDriveMask;
                    WCHAR wszDrives[] = L"A:";

                    if ((uDriveMask = _getdrives()) == 0) {
                        return FALSE;
                    }

                    __try {
                        if ((pFBData = (FBDATA*)malloc(sizeof(FBDATA))) == NULL)
                            return FALSE;
                        memset(pFBData, 0, sizeof(FBDATA));

                        while (uDriveMask) {
                            //if available, insert into file list
                            if (uDriveMask & 1) {
                                pFBData->nPaths++;
                                wcscpy_s(pFBData->FBFileList[nDrives].wszFileName, MAX_PATH + 1,
                                    wszDrives);
                                nDrives++;
                            }

                            ++wszDrives[0];
                            uDriveMask >>= 1;
                        }

                        // copy the FBDATA into PDATA
                        pMyPData = NULL;
                        if ((pMyPData = (PDATA*)malloc(sizeof(PDATA))) == NULL)
                            return FALSE;
                        memset(pMyPData, 0, sizeof(PDATA));

                        pMyPData->iMessage = FBM_DRIVES;
                        pMyPData->PluginID = FILEBROWSER;
                        memcpy(&pMyPData->PDataType.FBData, pFBData, sizeof(FBDATA));

                        // set event so that CPM can call ProcessCmd() and take the PDATA

                        // first, obtain handle to the event
                        hIFDataReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_IF_DATA_READY);
                        if (hIFDataReadyEvent == NULL) {
                            iRetVal = GetLastError();
                            return FALSE;
                        }

                        SetEvent(hIFDataReadyEvent);
                        CloseHandle(hIFDataReadyEvent);
                        return TRUE;
                    }
                    __finally {
                        if (pFBData)
                            free(pFBData);
                    }
                }

                case FBIF_EXPAND:
                {
                    // call ListDir() to retrieve the list of sub-directories
                    // in the absolute path mentioned in the pMNTRData.

                    // trivial parameter validation
                    if (wcscmp(pMNTRData->FBMData.wszPath, L"") == 0) {
                        return TRUE;
                    }

                    __try {
                        if ((pFBData = (FBDATA*)malloc(sizeof(FBDATA))) == NULL)
                            return FALSE;
                        memset(pFBData, 0, sizeof(FBDATA));

                        // call ListDir()
                        if ((iRetVal = FBListDir(pMNTRData->FBMData.wszPath, L"*.*",
                            pFBData->FBFileList, MAX_FILES)) <= 0) {
                            return FALSE;
                        }
                        pFBData->nPaths = iRetVal;

                        // store the file paths in PDATA
                        pMyPData = NULL;
                        if ((pMyPData = (PDATA*)malloc(sizeof(PDATA))) == NULL)
                            return FALSE;
                        memset(pMyPData, 0, sizeof(PDATA));

                        pMyPData->iMessage = FBM_DIRS;
                        pMyPData->PluginID = FILEBROWSER;
                        memcpy(&pMyPData->PDataType.FBData, pFBData, sizeof(FBDATA));

                        // set event so that CPM can call ProcessCmd() and take the PDATA

                        // first, obtain handle to the event
                        hIFDataReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_IF_DATA_READY);
                        if (hIFDataReadyEvent == NULL) {
                            iRetVal = GetLastError();
                            return FALSE;
                        }

                        SetEvent(hIFDataReadyEvent);
                        CloseHandle(hIFDataReadyEvent);
                        return TRUE;
                    }
                    __finally {
                        if (pFBData)
                            free(pFBData);
                    }
                }

                case FBIF_LIST_FILES:
                {
                    // call ListDir() to retrieve the list of sub-directories
                    // in the absolute path mentioned in the pMNTRData.

                    // trivial parameter validation
                    if (wcscmp(pMNTRData->FBMData.wszPath, L"") == 0) {
                        return TRUE;
                    }

                    __try {
                        if ((pFBData = (FBDATA*)malloc(sizeof(FBDATA))) == NULL)
                            return FALSE;
                        memset(pFBData, 0, sizeof(FBDATA));

                        // call ListDir()
                        if ((iRetVal = FBListFiles(pMNTRData->FBMData.wszPath, L"*.*",
                            pFBData->FBFileList, MAX_FILES)) <= 0) {
                            return FALSE;
                        }

                        pFBData->nPaths = iRetVal;

                        // store the file paths in PDATA
                        pMyPData = NULL;
                        if ((pMyPData = (PDATA*)malloc(sizeof(PDATA))) == NULL)
                            return FALSE;
                        memset(pMyPData, 0, sizeof(PDATA));

                        pMyPData->iMessage = FBM_FILES;
                        pMyPData->PluginID = FILEBROWSER;
                        memcpy(&pMyPData->PDataType.FBData, pFBData, sizeof(FBDATA));

                        // set event so that CPM can call ProcessCmd() and take the PDATA

                        // first, obtain handle to the event
                        hIFDataReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_IF_DATA_READY);
                        if (hIFDataReadyEvent == NULL) {
                            iRetVal = GetLastError();
                            return FALSE;
                        }

                        SetEvent(hIFDataReadyEvent);
                        CloseHandle(hIFDataReadyEvent);
                        return TRUE;
                    }
                    __finally {
                        if (pFBData)
                            free(pFBData);
                    }
                }

            }
            return FALSE;
        }

        case IF_GET_PDATA:
        {

            // copy the previously generated PDATA to the __out parameter pPData
            if (pMyPData) {
                *pPDataToCPM = pMyPData;
                pMyPData = NULL;
                return TRUE;
            } else {
                *pPDataToCPM = NULL;
                return FALSE;
            }
        }

    }// switch(cmd)
    return FALSE;

}//ProcessCmd()


//ListDir()
// Function to list the sub-directories under the specified directory.
int FBListDir(const WCHAR *pwszDirPath, const WCHAR *pwszExt, FBFILELIST fbFileList[], int nMaxFiles) {
    HANDLE h;
    int nFiles;
    WIN32_FIND_DATA FileData;
    BOOL fNextFile;
    WCHAR wszSearchPath[MAX_PATH + 1];

    // validate the nMaxFiles argument
    if (nMaxFiles <= 0) {
        return 0;
    }

    // initialize the number of files variable
    nFiles = -1;

    // concatenate the extension to the file path
    wsprintf(wszSearchPath, L"%s\\%s", pwszDirPath, pwszExt);


    /* check whether path exists or not */
    h = (HANDLE)FindFirstFile(wszSearchPath, &FileData);
    if (h == INVALID_HANDLE_VALUE) {
        int iRetVal = GetLastError();
        return -1;
    }

    // copy the first sub-directory's details to fbFileList
    if (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        ++nFiles;
        fbFileList[nFiles].dwFileSize = (FileData.nFileSizeHigh * (MAXDWORD + 1)) + FileData.nFileSizeLow;
        fbFileList[nFiles].dwFileAttributes = FileData.dwFileAttributes;
        if (FileTimeToSystemTime(&FileData.ftLastAccessTime, &fbFileList[nFiles].stLastAccess) == 0) {
            // function failed, store 0 in stLastAccess
            memset(&fbFileList[nFiles].stLastAccess, 0, sizeof(SYSTEMTIME));
        }
        swprintf_s(fbFileList[nFiles].wszFileName, MAX_PATH + 1, L"%s", FileData.cFileName);
    }

    while ((fNextFile = FindNextFile(h, &FileData))) {
        // skip over files, because we need to list only sub-directories
        if (!FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        if (nFiles < nMaxFiles - 1) {
            ++nFiles;

            // copy the file details to the list
            fbFileList[nFiles].dwFileSize = (FileData.nFileSizeHigh * (MAXDWORD + 1)) + FileData.nFileSizeLow;
            fbFileList[nFiles].dwFileAttributes = FileData.dwFileAttributes;
            if (FileTimeToSystemTime(&FileData.ftLastAccessTime, &fbFileList[nFiles].stLastAccess) == 0) {
                // function failed, store 0 in stLastAccess
                memset(&fbFileList[nFiles].stLastAccess, 0, sizeof(SYSTEMTIME));
            }
            swprintf_s(fbFileList[nFiles].wszFileName, MAX_PATH + 1, L"%s", FileData.cFileName);
        } else
            break;
    }

    // close the handle
    FindClose(h);

    return nFiles + 1;

}// ListDir()

int FBListFiles(const WCHAR *pwszDirPath, const WCHAR *pwszExt, FBFILELIST fbFileList[], int nMaxFiles) {
    HANDLE h;
    int nFiles;
    WIN32_FIND_DATA FileData;
    BOOL fNextFile;
    WCHAR wszSearchPath[MAX_PATH + 1];

    // validate the nMaxFiles argument
    if (nMaxFiles <= 0) {
        return 0;
    }

    // initialize the number of files variable
    nFiles = -1;

    // concatenate the extension to the file path
    wsprintf(wszSearchPath, L"%s\\%s", pwszDirPath, pwszExt);


    /* check whether path exists or not */
    h = (HANDLE)FindFirstFile(wszSearchPath, &FileData);
    if (h == INVALID_HANDLE_VALUE) {
        int iRetVal = GetLastError();
        return -1;
    }

    // copy the first file's details to fbFileList
    if (!FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        ++nFiles;
        fbFileList[nFiles].dwFileSize = (FileData.nFileSizeHigh * (MAXDWORD + 1)) + FileData.nFileSizeLow;
        fbFileList[nFiles].dwFileAttributes = FileData.dwFileAttributes;
        if (FileTimeToSystemTime(&FileData.ftLastAccessTime, &fbFileList[nFiles].stLastAccess) == 0) {
            // function failed, store 0 in stLastAccess
            memset(&fbFileList[nFiles].stLastAccess, 0, sizeof(SYSTEMTIME));
        }
        swprintf_s(fbFileList[nFiles].wszFileName, MAX_PATH + 1, L"%s", FileData.cFileName);
    }

    while ((fNextFile = FindNextFile(h, &FileData))) {
        // skip directories
        if (FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;

        if (nFiles < nMaxFiles - 1) {
            ++nFiles;

            // copy the file details to the list
            fbFileList[nFiles].dwFileSize = (FileData.nFileSizeHigh * (MAXDWORD + 1)) + FileData.nFileSizeLow;
            fbFileList[nFiles].dwFileAttributes = FileData.dwFileAttributes;
            if (FileTimeToSystemTime(&FileData.ftLastAccessTime, &fbFileList[nFiles].stLastAccess) == 0) {
                // function failed, store 0 in stLastAccess
                memset(&fbFileList[nFiles].stLastAccess, 0, sizeof(SYSTEMTIME));
            }
            swprintf_s(fbFileList[nFiles].wszFileName, MAX_PATH + 1, L"%s", FileData.cFileName);
        } else
            break;
    }

    // close the handle
    FindClose(h);


    return nFiles + 1;
}
