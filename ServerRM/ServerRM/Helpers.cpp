

// * ServerRM Project *
// Library.cpp: Commonly used user-defined functions

#include "ServerInclude.h"

extern WCHAR g_FTwszSourcePath[];
extern WCHAR g_FTwszDestPath[];

SOCKET FTPSocket = INVALID_SOCKET;

// Private helper functions
DWORD WINAPI GetFileThread(LPVOID lpArgs);

// DWORD GetFile(WCHAR *SourcePath, WCHAR *DestPath)
// Description: Transfers a file from SourcePath, which is on a remote client machine
// to DestPath, which is on the machine in which GetFile() runs.
BOOL GetFile(const WCHAR *pwszCliSrcPath, const WCHAR *pwszLocalDestPath) {
    HANDLE  hGetFileThread = NULL;

    // copy the paths to the global variables
    wcscpy_s(g_FTwszSourcePath, MAX_PATH + 1, pwszCliSrcPath);
    wcscpy_s(g_FTwszDestPath, MAX_PATH + 1, pwszLocalDestPath);

    hGetFileThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GetFileThread,
        NULL, NULL, NULL);
    if (hGetFileThread == NULL)
        return FALSE;

    return TRUE;

}// GetFile()


DWORD WINAPI GetFileThread(LPVOID lpArgs) {
    extern HANDLE hFTCompleteEvent;
    extern HWND hMsgOnlyWnd;
    extern WCHAR g_wszCurCliIP[];

    WSADATA wsaData;
    char	szCliIP[IPADDR_LEN + 1];
    struct	sockaddr_in ClientAddr;

    SCMARGS *pSCMArgs = NULL;

    int		iRetVal;
    DWORD	dwEventRetVal;
    BOOL	fError = FALSE;

    __try {
        // initialize WSA
        if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
            iRetVal = WSAGetLastError();
            PrintToStatusWindow(L"GetFile(): WSAStartup() error");
            return FALSE;
        }
        PrintToStatusWindow(L"WSAStartup() called");

        // create socket
        if ((FTPSocket = socket(AF_INET, SOCK_STREAM,
            IPPROTO_TCP)) == INVALID_SOCKET) {
            fError = TRUE;
            iRetVal = WSAGetLastError();
            return FALSE;
        }

        // call WSAAsyncSelect() to force functions to be non-blocking
        if (WSAAsyncSelect(FTPSocket, hMsgOnlyWnd,
            WM_FT_SOCKET_NOTIFY, FD_CONNECT |		// notification of completed connection
            FD_READ |			// notification of readiness for reading
            FD_WRITE |		// notification of readiness for writing
            FD_CLOSE)		// notification of socket closure
            == SOCKET_ERROR) {
            iRetVal = WSAGetLastError();
            PrintToStatusWindow(L"GetFile(): WSAAsyncSelect() error");
            fError = TRUE;
            return FALSE;
        }

        // convert wszCurCliIP to szCliIP
        if ((iRetVal = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, g_wszCurCliIP, -1,
            szCliIP, IPADDR_LEN + 1, "?", NULL)) == 0) {
            fError = TRUE;
            PrintToStatusWindow(L"GetFile(): Error obtaining ClientIP");
            return FALSE;
        }

        // fill ClientAddr structure
        ClientAddr.sin_family = AF_INET;
        ClientAddr.sin_port = htons(FTP_PORT);
        ClientAddr.sin_addr.S_un.S_addr = inet_addr(szCliIP);

        // create an event to receive notification of FTReady
        HANDLE hFTReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_FT_READY);
        if (hFTReadyEvent == NULL) {
            fError = TRUE;
            PrintToStatusWindow(L"GetFile(): FTReadyEvent error");
            return FALSE;
        }

        if ((pSCMArgs = (SCMARGS*)malloc(sizeof(SCMARGS))) == NULL)
            return FALSE;

        // fill the SCMArgs structure with FILE_TRANSFER_DATA and send it to SCM
        pSCMArgs->cmd = SCM_INIT_FT;
        pSCMArgs->FileTransferData.nFiles = 1;
        wsprintf(pSCMArgs->FileTransferData.ListOfFiles[0].wszFileName, L"%s", g_FTwszSourcePath);

        // Ask SCM to initiate file transfer
        if ((iRetVal = ServerComModule(SCM_INIT_FT, pSCMArgs, NULL)) == FALSE) {
            fError = TRUE;
            return FALSE;
        }

        PrintToStatusWindow(L"GetFile(): Waiting for ClientFT to start up");

        // wait for the FT ready event
        dwEventRetVal = WaitForSingleObject(hFTReadyEvent, INFINITE);
        if (dwEventRetVal = WAIT_OBJECT_0)
            ResetEvent(hFTReadyEvent);
        CloseHandle(hFTReadyEvent);

        PrintToStatusWindow(L"GetFile(): ClientFT running. Attempting to connect...");

        // attempt to connect to socket
        iRetVal = connect(FTPSocket, (sockaddr*)&ClientAddr, sizeof(ClientAddr));
        if ((iRetVal = GetLastError()) != WSAEWOULDBLOCK) {
            iRetVal = WSAGetLastError();
            fError = TRUE;
            PrintToStatusWindow(L"GetFile(): connect error");
            return 0;
        }

        Sleep(1);

        return 0;
    }
    __finally {
        if (pSCMArgs)
            free(pSCMArgs);

        if (fError == TRUE) {
            WSACleanup();
            PrintToStatusWindow(L"GetFile() __finally WSACleanup() called");
        }
    }

}// GetFileThread()

void WINAPI OnSelChanged(HWND hParentDlg) {
    DLGHDR *pDlgHdr = (DLGHDR*)GetWindowLong(hParentDlg, GWL_USERDATA);
    int iRetVal;

    if (pDlgHdr == NULL) {
        iRetVal = GetLastError();
        return;
    }

    int iSel = TabCtrl_GetCurSel(pDlgHdr->hTab);

    // Hide the current child dialog box, if any
    if (pDlgHdr->hCurDlg != NULL)
        iRetVal = ShowWindow(pDlgHdr->hCurDlg, SW_HIDE);

    // tell the un-selected dialog box that it is being deactivated
    SendMessage(pDlgHdr->hCurDlg, WM_CHILD_DLG_DEACTIVATE, 0, 0);

    // tell the selected child dialog that it is being activated
    SendMessage(pDlgHdr->hAllDlgs[iSel], WM_CHILD_DLG_ACTIVATE, 0, 0);

    // Make the selected dialog box visible
    iRetVal = ShowWindow(pDlgHdr->hAllDlgs[iSel], SW_SHOW);

    // set the handle to the currently visible dialog box
    pDlgHdr->hCurDlg = pDlgHdr->hAllDlgs[iSel];

    return;

}

// BOOL GetClientIP(WCHAR *pwszCliIP)
// Function to retrieve the IP address of the client to which
// the server is currently connected.
BOOL GetClientIP(WCHAR *wszCliIP) {
    extern WCHAR g_wszCurCliIP[IPADDR_LEN + 1];

    if (wcscmp(g_wszCurCliIP, L"") == 0)
        return FALSE;

    wcscpy_s(wszCliIP, IPADDR_LEN + 1, g_wszCurCliIP);

    return TRUE;
}

BOOL GetClientDir(WCHAR *pwszCliDir) {
    extern WCHAR g_wszCurCliDIR[];

    if (wcscmp(g_wszCurCliDIR, L"") == 0)
        return FALSE;

    wcscpy_s(pwszCliDir, MAX_PATH + 1, g_wszCurCliDIR);

    return TRUE;
}

BOOL GetCurrentWorkDir(WCHAR *pwszCWD) {
    if ((pwszCWD = _wgetcwd(pwszCWD, MAX_PATH + 1)) == NULL)
        return FALSE;

    return TRUE;
}

BOOL ShowRightClickMenu(HWND hOwnerWnd) {
    HMENU hRightClickMenu = NULL;

    BOOL fFirstTime = TRUE;

    POINT MousePos;

    WCHAR wszMenuText[32] = L"Remove";

    if (fFirstTime) {
        hRightClickMenu = CreatePopupMenu();
        fFirstTime = FALSE;
    }

    AppendMenu(hRightClickMenu, MF_STRING | MF_ENABLED, (UINT)10000, wszMenuText);

    GetCursorPos(&MousePos);

    TrackPopupMenu(hRightClickMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON,
        MousePos.x,
        MousePos.y,
        0, hOwnerWnd, NULL);

    return TRUE;
}

BOOL AddClientToDBFile(const CLIENTINFO *pClientInfo) {
    int i, iRetVal, nClients;
    CLIENTDB *pClientDB = NULL;
    WCHAR wszCliDir[MAX_PATH + 1];

    __try {
        if ((pClientDB = (CLIENTDB*)malloc(sizeof(CLIENTDB))) == NULL) {
            WriteToLogFile(FP_SERVER_LOGFILE, L"Helpers", L"AddClientToDBFile", L"malloc() error: pClientDB",
                GetLastError());
            return FALSE;
        }
        memset(pClientDB, 0, sizeof(CLIENTDB));

        if ((iRetVal = ReadClientsDBFile(pClientDB)) == FALSE)
            return FALSE;

        // check for duplicate entry
        for (i = 0; i < pClientDB->nClients; ++i) {
            if (wcscmp(pClientDB->ClientInfo[i].wszIP, pClientInfo->wszIP) == 0) {
                MessageBox(0, L"Duplicate entry.", L"Add Client", 0);
                return FALSE;
            }
        }

        // check if MAX_CLIENTS limit has been reached
        if (pClientDB->nClients == MAX_CLIENTS) {
            MessageBox(0, L"MAX_CLIENTS limit reached", L"Add Client", MB_ICONINFORMATION);
            return FALSE;
        }

        // increment number of clients
        ++pClientDB->nClients;
        nClients = pClientDB->nClients;

        // copy the client conf structure to the clientDB structure
        if (pClientInfo)
            memcpy(&(pClientDB->ClientInfo[nClients - 1]), pClientInfo, sizeof(CLIENTINFO));

        // write the updated structure to file
        if ((iRetVal = WriteClientsDBFile(pClientDB)) == FALSE)
            return FALSE;

        // create a directory for the new client
        // construct the directory path
        swprintf_s(wszCliDir, MAX_PATH + 1, L"%s\\%s", FP_CLIENTS_DIR, pClientInfo->wszIP);
        iRetVal = CreateDirectory(wszCliDir, NULL);

        // plugins directory
        swprintf_s(wszCliDir, MAX_PATH + 1, L"%s\\%s\\%s", FP_CLIENTS_DIR, pClientInfo->wszIP, FP_PLUGINS_DIR);
        iRetVal = CreateDirectory(wszCliDir, NULL);

        return TRUE;
    }
    __finally {
        if (pClientDB)
            free(pClientDB);
    }

    return TRUE;
}
BOOL ReadClientsDBFile(CLIENTDB *pClientsDB) {
    int iCliDBFile = 0;
    int iRetVal;

    if (pClientsDB == NULL)
        return FALSE;

    memset(pClientsDB, 0, sizeof(CLIENTDB));

    // open file in readonly mode
    if ((iRetVal = _wsopen_s(&iCliDBFile, FP_CLIENTS_DB, _O_RDONLY | _O_BINARY,
        _SH_DENYWR, _S_IWRITE | _S_IREAD)) != 0) {
        if ((iRetVal = GetLastError()) == ERROR_FILE_NOT_FOUND) {
            if ((iRetVal = _wsopen_s(&iCliDBFile, FP_CLIENTS_DB, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
                _SH_DENYRW, _S_IWRITE | _S_IREAD)) != 0) {
                // show error msg
                return FALSE;
            }
            if (iCliDBFile) {
                _close(iCliDBFile);
                iCliDBFile = 0;
            }
            WriteToLogFile(FP_SERVER_LOGFILE, L"Helpers", L"ReadClientsDBFile", L"New clients database file created", 0);
            return TRUE;
        } else
            return FALSE;
    }

    // read the file
    iRetVal = _read(iCliDBFile, pClientsDB,
        sizeof(CLIENTDB));
    if (iRetVal == 0) {
        if (iCliDBFile) {
            _close(iCliDBFile);
            iCliDBFile = 0;
        }
        return TRUE;
    } else if (iRetVal == -1) {
        if (iCliDBFile) {
            _close(iCliDBFile);
            iCliDBFile = 0;
        }
        return FALSE;
    } else {
        if (iCliDBFile) {
            _close(iCliDBFile);
            iCliDBFile = 0;
        }
        return TRUE;
    }
}

BOOL WriteClientsDBFile(const CLIENTDB *pClientsDB) {
    int iCliDBFile = 0;
    int iRetVal;

    if (pClientsDB == NULL)
        return FALSE;

    // open file in write mode
    if ((iRetVal = _wsopen_s(&iCliDBFile, FP_CLIENTS_DB, _O_WRONLY | _O_BINARY,
        _SH_DENYRW, _S_IWRITE | _S_IREAD)) != 0) {
        if ((iRetVal = GetLastError()) == ERROR_FILE_NOT_FOUND) {
            if ((iRetVal = _wsopen_s(&iCliDBFile, FP_CLIENTS_DB, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
                _SH_DENYRW, _S_IWRITE | _S_IREAD)) != 0) {
                // show error msg
                return FALSE;
            }

            if ((iRetVal = _write(iCliDBFile, pClientsDB,
                sizeof(CLIENTDB))) == -1) {
                if (iCliDBFile) {
                    _close(iCliDBFile);
                    iCliDBFile = 0;
                }
                return FALSE;
            }
            if (iCliDBFile) {
                _close(iCliDBFile);
                iCliDBFile = 0;
            }

            WriteToLogFile(FP_SERVER_LOGFILE, L"Helpers", L"WriteClientsDBFile", L"New clients database file created", 0);
            return TRUE;
        } else
            return FALSE;
    }
    // once file is opened successfully, write the struct
    if ((iRetVal = _write(iCliDBFile, pClientsDB,
        sizeof(CLIENTDB))) == -1) {
        if (iCliDBFile) {
            _close(iCliDBFile);
            iCliDBFile = 0;
        }
        return FALSE;
    }
    if (iCliDBFile) {
        _close(iCliDBFile);
        iCliDBFile = 0;
    }
    return TRUE;
}

BOOL AddPluginToDBFile(const PLUGININFO *pNewPluginInfo) {
    int i;
    PLUGINDB *pPluginDB = NULL;

    __try {
        if ((pPluginDB = (PLUGINDB*)malloc(sizeof(PLUGINDB))) == NULL) {
            // WriteToLog()
            return FALSE;
        }
        memset(pPluginDB, 0, sizeof(PLUGINDB));

        // read existing plugin database
        if (ReadPluginDBFile(pPluginDB) == FALSE)
            return FALSE;

        // check for duplicate entry (PluginID is primary key)
        for (i = 0; i < pPluginDB->nPlugins; ++i) {
            if (pNewPluginInfo->iPluginID == pPluginDB->PluginInfo[i].iPluginID) {
                MessageBox(0, L"Duplicate entry", L"Add Plugin", MB_ICONEXCLAMATION);
                return FALSE;
            }
        }

        // check if MAX_PLUGINS limit has been reached
        if (pPluginDB->nPlugins == MAX_PLUGINS) {
            MessageBox(0, L"MAX_PLUGINS limit reached", L"Add Plugin", MB_ICONINFORMATION);
            return FALSE;
        }

        // copy pNewPluginInfo to pPluginDB
        memcpy(&pPluginDB->PluginInfo[pPluginDB->nPlugins], pNewPluginInfo, sizeof(PLUGININFO));
        ++(pPluginDB->nPlugins);

        // write updated config to file
        if (WritePluginDBFile(pPluginDB) == FALSE)
            return FALSE;

        return TRUE;

    }
    __finally {
        if (pPluginDB) {
            free(pPluginDB);
            pPluginDB = NULL;
        }
    }
}

// ReadPluginDBFile()
// --
BOOL ReadPluginDBFile(PLUGINDB *pPluginDB) {
    int iPluginDBFile = 0;
    int iRetVal;

    if (pPluginDB == NULL)
        return FALSE;

    memset(pPluginDB, 0, sizeof(PLUGINDB));

    // open file in readonly mode
    if ((iRetVal = _wsopen_s(&iPluginDBFile, FP_PLUGINS_DB, _O_RDONLY | _O_BINARY,
        _SH_DENYWR, _S_IWRITE | _S_IREAD)) != 0) {
        if ((iRetVal = GetLastError()) == ERROR_FILE_NOT_FOUND) {
            iRetVal = MessageBox(0, L"Plugins database file not found. Do you want to create a new one?",
                L"Write to Clients DB File", MB_YESNO | MB_ICONQUESTION);
            if (iRetVal == IDYES) {
                if ((iRetVal = _wsopen_s(&iPluginDBFile, FP_PLUGINS_DB, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
                    _SH_DENYRW, _S_IWRITE | _S_IREAD)) != 0) {
                    // show error msg
                    return FALSE;
                }
                if (iPluginDBFile) {
                    _close(iPluginDBFile);
                    iPluginDBFile = 0;
                }

                WriteToLogFile(FP_SERVER_LOGFILE, L"Helpers", L"ReadPluginDBFile", L"New plugins database file created", 0);
                return TRUE;
            }
        } else
            return FALSE;
    }

    // read the file
    iRetVal = _read(iPluginDBFile, pPluginDB,
        sizeof(PLUGINDB));
    if (iRetVal == 0) {
        if (iPluginDBFile) {
            _close(iPluginDBFile);
            iPluginDBFile = 0;
        }
        return TRUE;
    } else if (iRetVal == -1) {
        if (iPluginDBFile) {
            _close(iPluginDBFile);
            iPluginDBFile = 0;
        }
        return FALSE;
    } else {
        if (iPluginDBFile) {
            _close(iPluginDBFile);
            iPluginDBFile = 0;
        }
        return TRUE;
    }
}

// BOOL WritePluginDBFile(const PLUGINDB *pPluginDB)
BOOL WritePluginDBFile(const PLUGINDB *pPluginDB) {
    int iPluginDBFile = 0;
    int iRetVal;

    if (pPluginDB == NULL)
        return FALSE;

    // open file in write mode
    if ((iRetVal = _wsopen_s(&iPluginDBFile, FP_PLUGINS_DB, _O_WRONLY | _O_BINARY,
        _SH_DENYRW, _S_IWRITE | _S_IREAD)) != 0) {
        if ((iRetVal = GetLastError()) == ERROR_FILE_NOT_FOUND) {
            iRetVal = MessageBox(0, L"Clients database file not found. Do you want to create a new one?",
                L"Write to Clients DB File", MB_YESNO | MB_ICONQUESTION);
            if (iRetVal == IDYES) {
                if ((iRetVal = _wsopen_s(&iPluginDBFile, FP_PLUGINS_DB, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
                    _SH_DENYRW, _S_IWRITE | _S_IREAD)) != 0) {
                    // show error msg
                    return FALSE;
                }

                if ((iRetVal = _write(iPluginDBFile, pPluginDB,
                    sizeof(PLUGINDB))) == -1) {
                    if (iPluginDBFile) {
                        _close(iPluginDBFile);
                        iPluginDBFile = 0;
                    }
                    return FALSE;
                }
                if (iPluginDBFile) {
                    _close(iPluginDBFile);
                    iPluginDBFile = 0;
                }

                WriteToLogFile(FP_SERVER_LOGFILE, L"Helpers", L"WritePluginDBFile", L"New plugins database file created", 0);
                return TRUE;
            } else {
                return FALSE;
            }
        } else
            return FALSE;
    }
    // once file is opened successfully, write the struct
    if ((iRetVal = _write(iPluginDBFile, pPluginDB,
        sizeof(PLUGINDB))) == -1) {
        if (iPluginDBFile) {
            _close(iPluginDBFile);
            iPluginDBFile = 0;
        }
        return FALSE;
    }
    if (iPluginDBFile) {
        _close(iPluginDBFile);
        iPluginDBFile = 0;
    }
    return TRUE;
}

// FreeTreeViewMemory()
// Free the memory whose address is stored in the lParam field of the tree node
// LogFile display, FBM and SGM offline image list trees
void FreeTreeViewMemory(HWND hTree) {
    HTREEITEM hRoot;

    if (hTree == NULL)
        return;

    if ((hRoot = TreeView_GetRoot(hTree)) == NULL)
        return;

    TreeDFS(hTree, hRoot);

    return;
}

// TreeDFS()
// Helper for FreeTreeViewMemory() function which traverses the 
// tree view in a depth-first manner and frees memory.
void TreeDFS(HWND hTree, HTREEITEM hCurNode) {
    HTREEITEM hNext = NULL;

    // first, check for children
    if ((hNext = TreeView_GetChild(hTree, hCurNode)) != NULL)
        TreeDFS(hTree, hNext);

    // next, sibling
    if ((hNext = TreeView_GetNextSibling(hTree, hCurNode)) != NULL)
        TreeDFS(hTree, hNext);

    // if both child and sibling are absent, then this is the leaf node
    {
        // get the tree node information and free memory if address is present in lParam
        TVITEM tvItem = { 0 };
        tvItem.mask = TVIF_PARAM | TVIF_HANDLE;
        tvItem.hItem = hCurNode;

        if (SendMessage(hTree, TVM_GETITEM, 0, (LPARAM)&tvItem) == FALSE)
            return;

        if (tvItem.lParam) {
            //WriteToAddressFile((void*)tvItem.lParam, 1);
            free((void*)tvItem.lParam);
        }
    }

    return;
}

// type: 0 indicates allocated address, 1 indicates deallocated address
BOOL WriteToAddressFile(void *pAddress, int type) {
    FILE *fp = NULL;

    int iRetVal;

    if (type == 0) {
        if ((iRetVal = _wfopen_s(&fp, L"AllocAddresses.txt", L"a")) != 0) {
            return FALSE;
        }
    } else if (type == 1) {
        if ((iRetVal = _wfopen_s(&fp, L"DeallocAddresses.txt", L"a")) != 0) {
            return FALSE;
        }
    } else
        return FALSE;

    // write the address to file
    fprintf_s(fp, "%p\n", pAddress);

    fclose(fp);

    return TRUE;
}