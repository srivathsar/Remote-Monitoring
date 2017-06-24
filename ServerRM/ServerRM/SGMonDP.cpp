
// * ServerRM project *
// SGMonDP.cpp: Contains all Dialog procks for SGM dialog box 
// and the four child dialog boxes.

#include "ServerInclude.h"

#define SGM_RT_DLG		0
#define SGM_OFF_DLG		1
#define SGM_RET_DLG		2
#define SGM_CONFIG_DLG	3

extern BOOL fConnected;
extern BOOL fFTSuccess;
extern int iFTStatus;
extern BOOL fFTDeleteFile;
extern PMONITORS g_PMonitors;
extern HINSTANCE hMainInstance;

extern WCHAR g_wszCurCliDIR[];
extern WCHAR g_wszCurCliIP[];
extern WCHAR g_wszCurCliNick[];

// Global variables
static HWND hSGMRetDlg;
static HWND hSGMRTDlg;

// SGM Dialog Box Helper functions
void WINAPI OnSGMDialogInit(HWND hDlg);

// Child dialog box procedures
BOOL CALLBACK SGMonRealtimeDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK SGMonOffDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK SGMonRetDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK SGMonConfigDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// RT helpers
BOOL UpdateRTTree(HWND hDlg, const WCHAR *pwszImageFilePath);
DWORD WINAPI GetRTShots(LPVOID lpArgs);

// Offline display helpers
BOOL SGMonOffDPListImages(HWND hDlg);
BOOL DisplayNodeImage(HWND hTree, HTREEITEM hItem, HWND hPicCtrl, int cx, int cy);
BOOL DisplayImage(HWND hPictureCtrl, WCHAR *wszFilePath, int cxDesired, int cyDesired);

// Retrieve helpers
BOOL SGMonUpdateRet(HWND hDlg);
DWORD WINAPI FTGetSnapShots(LPVOID lpArgs);

// Configuration helpers
BOOL UpdateSGConfigUI(HWND hDlg);
BOOL ReadSGConfigFile(SGCONFIG *pSGConfig);
BOOL WriteSGConfigFile(const SGCONFIG *pSGConfig);
BOOL WriteSGRealTimeConfig(const SGREALAPP *pSGRealApp);


// ** Screen Grabber Monitor Dialog Proc **
BOOL CALLBACK SGMonDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

    switch (message) {
        case WM_INITDIALOG:
        {
            // store this dialog box information in the global PMONITORS structure
            g_PMonitors.PluginMonitor[g_PMonitors.nPlugins].hDlgBox = hDlg;
            g_PMonitors.PluginMonitor[g_PMonitors.nPlugins].iPluginType = SCREENGRABBER;
            g_PMonitors.nPlugins++;

            OnSGMDialogInit(hDlg);
            CenterWindow(hDlg);

            return TRUE;
        }


        case WM_COMMAND:
        {
            // handle commands from child controls
            switch (LOWORD(wParam)) {
                case IDB_SGM_CANCEL:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }

                return FALSE;
            }
            return FALSE;
        }


        case WM_CLIENT_RESPONSE:
        {
            DLGHDR *pDlgHdr = NULL;

            pDlgHdr = (DLGHDR*)GetWindowLong(hDlg, GWL_USERDATA);
            if (pDlgHdr != NULL) {
                switch (((PDATA*)lParam)->iMessage) {
                    case SGM_RT_SHOT:
                    {
                        SendMessage(pDlgHdr->hAllDlgs[SGM_RT_DLG], WM_CLIENT_RESPONSE, 0, lParam);
                        return TRUE;
                    }

                    case SGM_SHOTS:
                    {
                        SendMessage(pDlgHdr->hAllDlgs[SGM_RET_DLG], WM_CLIENT_RESPONSE, 0, lParam);
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

        case WN_DLG_NOTIFICATION:
        {
            if (wParam == WN_DLG_DISCONNECTED) {
                SetWindowText(hDlg, L"Screen Grabber Monitor: Disconnected");
            }

            return TRUE;
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

}// SGMonDP()


void WINAPI OnSGMDialogInit(HWND hDlg) {
    int i, iRetVal;

    TCITEM tie;

    WCHAR wszDirPath[MAX_PATH + 1] = L"";
    WCHAR wszRTShotsDir[MAX_PATH + 1] = L"";
    WCHAR wszWndTitle[MAX_WND_TITLE + 1] = L"";

    DLGHDR *pDlgHdr = (DLGHDR*)LocalAlloc(LPTR, sizeof(DLGHDR));


    // Set window title
    swprintf_s(wszWndTitle, MAX_WND_TITLE + 1, L"ScreenGrabber Monitor: Connected to %s / %s", g_wszCurCliIP, g_wszCurCliNick);
    SetWindowText(hDlg, wszWndTitle);

    // SGM directory
    swprintf_s(wszDirPath, L"%s\\%s", g_wszCurCliDIR, FP_CLIENT_SGM_DIR);
    if ((iRetVal = CreateDirectory(wszDirPath, NULL)) == 0) {
        if ((iRetVal = GetLastError()) == ERROR_PATH_NOT_FOUND) {
            //WriteToLogFile(FP_SERVER_LOGFILE, L"SGMonDP", L"OnSGMDialogInit", L"Error creating SGM directory", GetLastError());
            MessageBox(hDlg, L"Error creating SGM dir", L"OnSGMDialogInit", 0);
        }
    }

    // snap shots directory
    swprintf_s(wszDirPath, L"%s\\%s", g_wszCurCliDIR, FP_CLIENT_SHOTS_DIR);
    if ((iRetVal = CreateDirectory(wszDirPath, NULL)) == 0) {
        if ((iRetVal = GetLastError()) == ERROR_PATH_NOT_FOUND) {
            //WriteToLogFile(FP_SERVER_LOGFILE, L"SGMonDP", L"OnSGMDialogInit", L"Error creating SGM directory", GetLastError());
            MessageBox(hDlg, L"Error creating SGM shots dir", L"OnSGMDialogInit", 0);
        }
    }

    // realtime shots directory
    swprintf_s(wszRTShotsDir, MAX_PATH + 1, L"%s\\%s", g_wszCurCliDIR, FP_CLIENT_RT_SHOTS_DIR);
    if ((iRetVal = CreateDirectory(wszRTShotsDir, NULL)) == 0) {
        if ((iRetVal = GetLastError()) == ERROR_PATH_NOT_FOUND) {
            //WriteToLogFile(FP_SERVER_LOGFILE, L"SGMonDP", L"OnSGMDialogInit", L"Error creating SGM directory", GetLastError());
            MessageBox(hDlg, L"Error creating SGM realtime shots dir", L"OnSGMDialogInit", 0);
        }
    }

    memset(pDlgHdr, 0, sizeof(DLGHDR));

    // Save a pointer to the DLGHDR structure. 
    SetWindowLong(hDlg, GWL_USERDATA, (LONG)pDlgHdr);

    // retrieve and save handle of the tab control
    pDlgHdr->hTab = GetDlgItem(hDlg, IDC_SGM_TAB);
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

    tie.pszText = L"Retrieve Snaps";
    iRetVal = TabCtrl_InsertItem(pDlgHdr->hTab, 2, &tie);

    tie.pszText = L"Configuration";
    iRetVal = TabCtrl_InsertItem(pDlgHdr->hTab, 3, &tie);

    // create the four dialog boxes and save the handle in the DLGHDR structure
    pDlgHdr->hAllDlgs[SGM_RT_DLG] = CreateDialog(hMainInstance, MAKEINTRESOURCE(IDD_SGM_RT), hDlg, SGMonRealtimeDP);
    pDlgHdr->hAllDlgs[SGM_OFF_DLG] = CreateDialog(hMainInstance, MAKEINTRESOURCE(IDD_SGM_OFFLINE), hDlg, SGMonOffDP);
    pDlgHdr->hAllDlgs[SGM_RET_DLG] = CreateDialog(hMainInstance, MAKEINTRESOURCE(IDD_SGM_RETRIEVE), hDlg, SGMonRetDP);
    pDlgHdr->hAllDlgs[SGM_CONFIG_DLG] = CreateDialog(hMainInstance, MAKEINTRESOURCE(IDD_SGM_CONFIG), hDlg, SGMonConfigDP);

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

}// OnSGMDialogInit()


BOOL CALLBACK SGMonRealtimeDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    int i, iRetVal;
    static int iSelected = -1;

    SPMARGS *pSPMArgs = NULL;
    SGCONFIG *pSGConfig = NULL;

    BOOL fRTStarted = FALSE;

    static RECT	rcPicCtrl;
    static HWND hComboAppList;
    static HWND hRTTree;
    static HWND hPicCtrl;

    HTREEITEM h;
    HTREEITEM hPrevNode = NULL;
    static HTREEITEM hCurNode;

    TVITEM tviItem = { 0 };

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

            hSGMRTDlg = hDlg;
            hComboAppList = GetDlgItem(hDlg, IDC_SGM_RT_COMBO);
            hRTTree = GetDlgItem(hDlg, IDC_SGM_RT_TREE);
            hPicCtrl = GetDlgItem(hDlg, IDC_SGM_RT_IMG);

            // store the bounding rectangle of the picture control
            if (GetClientRect(hPicCtrl, &rcPicCtrl) == 0) {
                // error
            }

            __try {
                if ((pSGConfig = (SGCONFIG*)malloc(sizeof(SGCONFIG))) == NULL) {
                    // WriteToLogFile()
                    return TRUE;
                }
                memset(pSGConfig, 0, sizeof(SGCONFIG));

                // read the application list from the configuration file
                if ((iRetVal = ReadSGConfigFile(pSGConfig)) == FALSE) {
                    MessageBox(hDlg, L"Error reading configuration file.", L"RealTime", 0);
                    return TRUE;
                }

                if (pSGConfig->nApps == 0) {
                    SendMessage(hComboAppList, CB_INSERTSTRING, 0, (LPARAM)L"No Applications");
                    return TRUE;
                }

                for (i = 0; i < pSGConfig->nApps; ++i) {
                    SendMessage(hComboAppList, CB_INSERTSTRING, 0, (LPARAM)pSGConfig->SGAppList[i].wszEXEName);
                }

                return TRUE;
            }
            __finally {
                if (pSGConfig) {
                    free(pSGConfig);
                    pSGConfig = NULL;
                }
            }
        }

        case WM_CTLCOLORDLG:	// fall-through

        case WM_CTLCOLORSTATIC:
        {
            HBRUSH hbrBackground;

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

            HANDLE hGetRTShotsThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)GetRTShots, (LPVOID)pPData, 0, NULL);
            if (hGetRTShotsThread == NULL) {
                iRetVal = GetLastError();
                return TRUE;
            }

            return TRUE;
        }


        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                case IDB_SGM_RT_START:
                {
                    int iSelItemIndex;
                    WCHAR wszSelApp[MAX_FILE_NAME + 1];

                    // read application name from combo box
                    //get index
                    iSelItemIndex = (int)SendMessage(hComboAppList, CB_GETCURSEL, 0, 0);
                    if (iSelItemIndex == CB_ERR) {
                        MessageBox(hDlg, L"Please select an application", L"RealTime", MB_OK | MB_ICONINFORMATION);
                        return TRUE;
                    }

                    //get string
                    if ((iRetVal = SendMessage(hComboAppList, CB_GETLBTEXT,
                        iSelItemIndex, (LPARAM)wszSelApp)) == CB_ERR) {
                        MessageBox(hDlg, L"Unable to read the selected application name", L"RealTime",
                            MB_OK | MB_ICONINFORMATION);
                        return TRUE;
                    }

                    __try {
                        // send command to client
                        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                            PrintToStatusWindow(L"SGM: malloc error: SPMArgs");
                            return TRUE;
                        }
                        memset(pSPMArgs, 0, sizeof(SPMARGS));

                        pSPMArgs->cmd = SPM_SENDDATA;
                        pSPMArgs->iFromPlugin = SCREENGRABBER;
                        pSPMArgs->MNTRData.SGMData.cmd = SGIF_RT_START;
                        if (wcscpy_s(pSPMArgs->MNTRData.SGMData.SGMStructs.SGRealApp.wszEXEName, MAX_PATH + 1, wszSelApp) != 0) {
                            MessageBox(hDlg, L"Error saving selected application", L"Realtime", 0);
                            return TRUE;
                        }

                        if (ServerPluginManager(0, pSPMArgs, NULL) == FALSE) {
                            MessageBox(hDlg, L"Error sending data to client", L"RealTime", MB_OK);
                            return TRUE;
                        }

                        fRTStarted = TRUE;

                        // disable Start button
                        EnableWindow(GetDlgItem(hDlg, IDB_SGM_RT_START), FALSE);

                        // enable Stop button
                        EnableWindow(GetDlgItem(hDlg, IDB_SGM_RT_STOP), TRUE);


                        return TRUE;
                    }
                    __finally {
                        if (pSPMArgs)
                            free(pSPMArgs);
                    }
                }

                case IDB_SGM_RT_STOP:
                {
                    __try {
                        // send command to client
                        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                            PrintToStatusWindow(L"SGM: malloc error: SPMArgs");
                            return TRUE;
                        }
                        memset(pSPMArgs, 0, sizeof(SPMARGS));

                        pSPMArgs->cmd = SPM_SENDDATA;
                        pSPMArgs->iFromPlugin = SCREENGRABBER;
                        pSPMArgs->MNTRData.SGMData.cmd = SGIF_RT_STOP;

                        if (ServerPluginManager(0, pSPMArgs, NULL) == FALSE) {
                            MessageBox(hDlg, L"Error sending data to client", L"RealTime", MB_OK);
                            return TRUE;
                        }

                        fRTStarted = FALSE;

                        // enable the Start button
                        EnableWindow(GetDlgItem(hDlg, IDB_SGM_RT_START), TRUE);

                        // disable the Stop button
                        EnableWindow(GetDlgItem(hDlg, IDB_SGM_RT_STOP), FALSE);
                        return TRUE;
                    }
                    __finally {
                        if (pSPMArgs)
                            free(pSPMArgs);
                    }
                    return TRUE;
                }

                case IDB_SGM_RT_FIRST:
                {
                    if ((h = TreeView_GetRoot(hRTTree)) == NULL)
                        return TRUE;

                    iRetVal = TreeView_SelectItem(hRTTree, h);
                    return TRUE;
                }

                case IDB_SGM_RT_PREV:
                {
                    if ((h = TreeView_GetPrevSibling(hRTTree, hCurNode)) == NULL)
                        return TRUE;

                    iRetVal = TreeView_SelectItem(hRTTree, h);
                    return FALSE;
                }


                case IDB_SGM_RT_NEXT:
                {
                    if ((h = TreeView_GetNextSibling(hRTTree, hCurNode)) == NULL)
                        return TRUE;

                    iRetVal = TreeView_SelectItem(hRTTree, h);
                    return TRUE;
                }


                case IDB_SGM_RT_LAST:
                {
                    for (h = hCurNode;
                        h != NULL;
                        h = TreeView_GetNextSibling(hRTTree, h)) {
                        hPrevNode = h;
                    }
                    if (hPrevNode != NULL) {
                        iRetVal = TreeView_SelectItem(hRTTree, hPrevNode);
                    }

                    return TRUE;
                }


                case IDC_SGM_RT_TREE:
                {

                    return FALSE;
                }


            }// switch(LOWORD(wParam))
            return FALSE;
        }

        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
                case TVN_SELCHANGED:
                {
                    HTREEITEM hSelectedItem = NULL;

                    //get handle to selected item
                    if ((hSelectedItem = TreeView_GetSelection(hRTTree)) == NULL)
                        return TRUE;


                    hCurNode = hSelectedItem;
                    TreeView_EnsureVisible(hRTTree, hSelectedItem);
                    DisplayNodeImage(hRTTree, hCurNode, hPicCtrl, rcPicCtrl.right, rcPicCtrl.bottom);
                    return TRUE;

                }// case TVN_SELCHANGED

            }// switch ( ((LPNMHDR)lParam)->code )
            return FALSE;

        }// case WM_NOTIFY

        case WM_CHILD_DLG_ACTIVATE:
        {
            // delete all strings from combo box
            int nItems = SendMessage(hComboAppList, CB_GETCOUNT, 0, 0);
            for (i = 0; i < nItems; ++i)
                if (SendMessage(hComboAppList, CB_DELETESTRING, 0, NULL) == CB_ERR) {
                    break;
                }

            __try {
                if ((pSGConfig = (SGCONFIG*)malloc(sizeof(SGCONFIG))) == NULL) {
                    // WriteToLogFile()
                    return TRUE;
                }
                memset(pSGConfig, 0, sizeof(SGCONFIG));

                // read the application list from the configuration file
                if ((iRetVal = ReadSGConfigFile(pSGConfig)) == FALSE) {
                    MessageBox(hDlg, L"Error reading configuration file.", L"RealTime", 0);
                    return TRUE;
                }

                if (pSGConfig->nApps == 0) {
                    SendMessage(hComboAppList, CB_INSERTSTRING, 0, (LPARAM)L"No Applications");
                    return TRUE;
                }

                for (i = 0; i < pSGConfig->nApps; ++i) {
                    SendMessage(hComboAppList, CB_INSERTSTRING, 0, (LPARAM)pSGConfig->SGAppList[i].wszEXEName);
                }

                // enable Start button
                EnableWindow(GetDlgItem(hDlg, IDB_SGM_RT_START), TRUE);

                // disable Stop button
                EnableWindow(GetDlgItem(hDlg, IDB_SGM_RT_STOP), FALSE);

                return FALSE;
            }
            __finally {
                if (pSGConfig)
                    free(pSGConfig);
            }
        }

        case WM_CHILD_DLG_CLOSE:				// fall-through

        case WM_CHILD_DLG_DEACTIVATE:
        {
            TreeView_DeleteAllItems(hRTTree);

            if (fRTStarted == FALSE)
                return TRUE;

            __try {
                // send command to client
                if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                    PrintToStatusWindow(L"SGM: malloc error: SPMArgs");
                    return TRUE;
                }
                memset(pSPMArgs, 0, sizeof(SPMARGS));

                pSPMArgs->cmd = SPM_SENDDATA;
                pSPMArgs->iFromPlugin = SCREENGRABBER;
                pSPMArgs->MNTRData.SGMData.cmd = SGIF_RT_STOP;

                if (ServerPluginManager(0, pSPMArgs, NULL) == FALSE) {
                    MessageBox(hDlg, L"Error sending data to client", L"RealTime", MB_OK);
                    return TRUE;
                }

                fRTStarted = FALSE;

                // enable the Start button
                EnableWindow(GetDlgItem(hDlg, IDB_SGM_RT_START), TRUE);

                // disable the Stop button
                EnableWindow(GetDlgItem(hDlg, IDB_SGM_RT_STOP), FALSE);

                return TRUE;
            }
            __finally {
                if (pSPMArgs)
                    free(pSPMArgs);
            }
        }

        case WM_CLOSE:
        {
            EndDialog(hDlg, 0);
            return TRUE;
        }

    }// switch(message)
    return FALSE;


}


BOOL CALLBACK SGMonOffDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    int		iRetVal;
    HBRUSH	hbrBackground;

    static RECT	rcPicCtrl;
    static HWND	hTree = NULL;
    static HWND	hPicCtrl = NULL;
    static WCHAR wszSnapShotsDir[MAX_PATH + 1];

    HTREEITEM h = NULL;
    HTREEITEM hPrevNode = NULL;
    static HTREEITEM hCurNode;

    TVITEM tviItem = { 0 };

    switch (message) {
        case WM_INITDIALOG:
        {
            RECT rcTab = { 0,0,0,0 };

            // get the pointer to the DLGHDR structure from the parent window
            HWND hParentDlg = GetParent(hDlg);
            DLGHDR *pDlgHdr = (DLGHDR*)GetWindowLong(hParentDlg, GWL_USERDATA);

            // get the bounding rectangle of the client area of the tab control
            if (GetClientRect(pDlgHdr->hTab, &rcTab) == 0) {
                // error
            }

            // set the dialog box position to fit inside the tab control
            if ((iRetVal = SetWindowPos(hDlg, NULL, rcTab.left + 15, rcTab.top + 35,
                0, 0, SWP_NOZORDER | SWP_NOSIZE)) == 0) {
                iRetVal = GetLastError();
                PrintToStatusWindow(L"Unable to set dialog box position");
                return FALSE;
            }

            // store handle to tree view and picture control
            hPicCtrl = GetDlgItem(hDlg, IDC_SGM_OFF_STATIC);
            hTree = GetDlgItem(hDlg, IDC_SGM_OFF_TREE);

            // store the bounding rectangle of the picture control
            if (GetClientRect(hPicCtrl, &rcPicCtrl) == 0) {
                // error
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

        case WM_CHILD_DLG_ACTIVATE:
        {
            // This child dialog is now active. Perform any updates required.

            // update the images list
            SGMonOffDPListImages(hDlg);

            return TRUE;
        }

        case WM_CHILD_DLG_DEACTIVATE:
        {
            FreeTreeViewMemory(hTree);
            TreeView_DeleteAllItems(hTree);
            //SendMessage(hPicCtrl, STN_DISABLE, 
            return TRUE;
        }

        case WM_CHILD_DLG_CLOSE:
        {
            FreeTreeViewMemory(hTree);
            TreeView_DeleteAllItems(hTree);
            return TRUE;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                /*case IDB_SGM_OFF_BUILD:
                    {
                        PrintToStatusBar(L"Off build", 2);
                        return TRUE;
                    }*/


                    /*case IDC_SGM_OFF_TREE:
                        {
                            PrintToStatusBar(L"Off tree", 2);
                            return TRUE;
                        }*/


                case IDC_SGM_OFF_FIRST:
                {
                    if ((h = TreeView_GetParent(hTree, hCurNode)) == NULL)
                        return TRUE;

                    if ((h = TreeView_GetChild(hTree, h)) == NULL)
                        return TRUE;

                    iRetVal = TreeView_SelectItem(hTree, h);
                    return FALSE;
                }


                case IDC_SGM_OFF_PREV:
                {
                    if ((h = TreeView_GetPrevSibling(hTree, hCurNode)) == NULL)
                        return TRUE;

                    iRetVal = TreeView_SelectItem(hTree, h);
                    //iRetVal = TreeView_SelectDropTarget(hTree, h);
                    return FALSE;
                }


                case IDC_SGM_OFF_NEXT:
                {
                    if ((h = TreeView_GetNextSibling(hTree, hCurNode)) == NULL)
                        return TRUE;

                    iRetVal = TreeView_SelectItem(hTree, h);
                    //iRetVal = TreeView_SelectDropTarget(hTree, h);
                    return FALSE;
                }


                case IDC_SGM_OFF_LAST:
                {
                    for (h = hCurNode;
                        h != NULL;
                        h = TreeView_GetNextSibling(hTree, h)) {
                        hPrevNode = h;
                    }
                    if (hPrevNode != NULL) {
                        iRetVal = TreeView_SelectItem(hTree, hPrevNode);
                        //iRetVal = TreeView_SelectDropTarget(hTree, hPrevNode);
                    }

                    return FALSE;
                }

            }// switch(LOWORD(wParam))
            return FALSE;
        }


        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
                case TVN_SELCHANGED:
                {
                    HTREEITEM hSelectedItem = NULL;

                    //get handle to selected item
                    if ((hSelectedItem = TreeView_GetSelection(hTree)) == NULL)
                        return TRUE;

                    tviItem.hItem = hSelectedItem;
                    tviItem.mask = TVIF_HANDLE | TVIF_STATE;
                    tviItem.state = TVIS_SELECTED;
                    TreeView_SetItem(hTree, &tviItem);

                    hCurNode = hSelectedItem;
                    TreeView_EnsureVisible(hTree, hSelectedItem);
                    DisplayNodeImage(hTree, hCurNode, hPicCtrl, rcPicCtrl.right, rcPicCtrl.bottom);
                    return TRUE;

                }// case TVN_SELCHANGED

            }// switch ( ((LPNMHDR)lParam)->code )
            return FALSE;

        }// case WM_NOTIFY


        case WM_CLOSE:
        {
            EndDialog(hDlg, 0);
            return TRUE;
        }

    }// switch(message)
    return FALSE;


}


BOOL CALLBACK SGMonRetDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    extern HWND hMsgOnlyWnd;

    int iRetVal;

    HBRUSH hbrBackground;

    static int	iSelected = -1;
    static HWND	hList = NULL;
    LVCOLUMN	lvColumn = { 0 };

    // Retrieve snap shots
    SYSTEMTIME stFromDate;
    SYSTEMTIME stToDate;
    SYSTEMTIME stToday;

    SPMARGS *pSPMArgs = NULL;
    MNTRDATA *pMNTRDataOut = NULL;

    HANDLE hFTThread = NULL;

    switch (message) {
        case WM_INITDIALOG:
        {
            RECT	rcTab = { 0,0,0,0 };

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

            // save handle in global variable
            hSGMRetDlg = hDlg;

            // add the application list to the list view

            if ((hList = GetDlgItem(hDlg, IDC_SGM_RET_APPLIST)) == NULL) {
                // error		
            }

            // Set up list view headers
            lvColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;    //type of mask

            // first column
            lvColumn.pszText = TEXT("Name");                      //First Header Text
            lvColumn.cx = 0x72;                                   //width of column
            SendMessage(hList, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn); //Insert the column

            // second column
            lvColumn.pszText = TEXT("Shot Type");
            lvColumn.cx = 0x72;
            SendMessage(hList, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

            return TRUE;
        }


        case WM_CTLCOLORDLG:		// fall-through

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

            if ((hFTThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)FTGetSnapShots,
                (LPVOID)pPData, 0, NULL)) == NULL) {
                // display error
                return FALSE;
            }

            return TRUE;
        }

        case WM_NOTIFY:
        {
            // handle notifications from child controls
            switch (LOWORD(wParam)) {
                case IDC_SGM_RET_APPLIST:
                {
                    if (((LPNMHDR)lParam)->code == LVN_ITEMCHANGED)
                        iSelected = ((LPNMITEMACTIVATE)lParam)->iItem;

                    return TRUE;
                }
            }
            return FALSE;
        }


        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                case IDB_SGM_RET_RETRIEVE:
                {
                    BOOL	fInvalidFromDate = FALSE;
                    BOOL	fInvalidToDate = FALSE;
                    HWND	hListView = NULL;
                    LVITEM	lviSelectedItem = { 0 };

                    WCHAR wszEXEName[MAX_FILE_NAME + 1] = L"";

                    if ((hListView = GetDlgItem(hDlg, IDC_SGM_RET_APPLIST)) == NULL) {
                    }

                    // retrieve the application chosen
                    lviSelectedItem.mask = LVIF_TEXT;
                    lviSelectedItem.iItem = iSelected;
                    lviSelectedItem.pszText = wszEXEName;
                    lviSelectedItem.cchTextMax = MAX_FILE_NAME + 1;

                    // send message to retrieve the item
                    if (SendMessage(hListView, LVM_GETITEM, 0, (LPARAM)&lviSelectedItem) == FALSE) {
                        iRetVal = GetLastError();
                        MessageBox(0, L"Could not get chosen application name", 0, 0);
                        return TRUE;
                    }

                    if ((iRetVal = DateTime_GetSystemtime(GetDlgItem(hDlg, IDC_KLM_RET_FROMDATE),
                        &stFromDate)) != GDT_VALID)
                        MessageBox(hDlg, L"Error reading From date", 0, 0);

                    if ((iRetVal = DateTime_GetSystemtime(GetDlgItem(hDlg, IDC_KLM_RET_TODATE),
                        &stToDate)) != GDT_VALID)
                        MessageBox(hDlg, L"Error reading To date", 0, 0);

                    GetLocalTime(&stToday);

                    // validate the dates

                    // Check if ToDate is > Today's Date.
                    if (stToDate.wYear > stToday.wYear) {
                        fInvalidToDate = TRUE;
                    } else if (stToDate.wYear == stToday.wYear) {
                        if (stToDate.wMonth > stToday.wMonth) {
                            fInvalidToDate = TRUE;
                        } else if (stToDate.wMonth == stToday.wMonth) {
                            if (stToDate.wDay > stToday.wDay) {
                                fInvalidToDate = TRUE;
                            }
                        }
                    }
                    if (fInvalidToDate == TRUE) {
                        MessageBox(hDlg, L"To Date is greater than Current Date", L"Invalid Date", MB_ICONINFORMATION);
                        return TRUE;
                    }

                    // check if FromDate is > ToDate
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
                        MessageBox(hDlg, L"From Date is greater than To Date", L"Invalid Date", MB_ICONINFORMATION);
                        return TRUE;
                    }

                    __try {
                        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                            PrintToStatusWindow(L"SGM: malloc error: pSPMArgs");
                            return FALSE;
                        }
                        memset(pSPMArgs, 0, sizeof(SPMARGS));

                        // Once all information has been retrieved, fill the 
                        // SGMData structure with relevant command and data.
                        pSPMArgs->MNTRData.SGMData.cmd = SGIF_GET_SHOTS;
                        wcscpy_s(pSPMArgs->MNTRData.SGMData.SGMStructs.SGMRetSnaps.wszAppName, MAX_FILE_NAME + 1, wszEXEName);
                        memcpy(&pSPMArgs->MNTRData.SGMData.SGMStructs.SGMRetSnaps.stFromDate, &stFromDate, sizeof(SYSTEMTIME));
                        memcpy(&pSPMArgs->MNTRData.SGMData.SGMStructs.SGMRetSnaps.stToDate, &stToDate, sizeof(SYSTEMTIME));

                        // add command meant for SPM
                        pSPMArgs->cmd = SPM_SENDDATA;
                        pSPMArgs->iFromPlugin = SCREENGRABBER;

                        iRetVal = ServerPluginManager(NULL, pSPMArgs, NULL);

                        return TRUE;
                    }
                    __finally {
                        if (pSPMArgs)
                            free(pSPMArgs);
                    }
                }

                case IDB_SGM_RET_STOP:
                {
                    SendMessage(hMsgOnlyWnd, WM_FT_TERM, 0, 0);
                    return TRUE;
                }

            }// switch(LOWORD(wParam))
            return FALSE;
        }

        case WM_CHILD_DLG_ACTIVATE:
        {
            if (SGMonUpdateRet(hDlg) == FALSE)
                MessageBox(hDlg, L"Error updating application list", L"SGM Retrieve Dialog", MB_ICONINFORMATION);

            return TRUE;
        }

    }// switch(message)
    return FALSE;

}


BOOL CALLBACK SGMonConfigDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    HBRUSH hbrBackground;

    int	i;
    int	iRetVal;

    WCHAR *pwsz = NULL;

    WORD	wLen;
    WCHAR	wszRTInt[16];
    WCHAR	wszOffInt[16];
    WCHAR	wszEXEName[MAX_FILE_NAME + 1];
    WCHAR	wszRadioBtnText[MAX_RADIO_BTN_NAME + 1];

    int		nItems;
    static HWND hList;

    LVCOLUMN	lvColumn = { 0 };
    LVITEM		lviToInsert = { 0 };

    // config structures
    SGCONFIG	SGConfig = { 0 };
    SGREALAPP	SGRealApp = { 0 };

    SPMARGS *pSPMArgs = NULL;

    __try {
        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
            PrintToStatusWindow(L"SGM: malloc error: SPMArgs");
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

                // default radio button
                SendMessage(GetDlgItem(hDlg, IDC_SGM_CONFIG_AS), BM_SETCHECK, BST_CHECKED, 0);

                if ((hList = GetDlgItem(hDlg, IDC_SGM_CONFIG_APPLIST)) == NULL) {
                    // error
                    return TRUE;
                }
                // Set up list view headers
                lvColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;    //type of mask

                // first column
                lvColumn.pszText = TEXT("Name");                      //First Header Text
                lvColumn.cx = 0x100;                                   //width of column
                SendMessage(hList, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn); //Insert the column

                // second column
                lvColumn.pszText = TEXT("Shot Type");
                lvColumn.cx = 0x100;
                SendMessage(hList, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

                if (UpdateSGConfigUI(hDlg) == FALSE)
                    MessageBox(hDlg, L"Error updating UI", L"Update UI", 0);

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

            case WM_NOTIFY:
            {
                switch (((LPNMHDR)lParam)->code) {
                    case NM_RCLICK:
                    {
                        int i;
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
                                    if (ReadSGConfigFile(&SGConfig) == FALSE) {
                                        MessageBox(hDlg, L"Error reading from configuration file", L"Application Removal", 0);
                                        return TRUE;
                                    }

                                    // update config structure

                                    // search for the entry with the selected application exe name
                                    for (i = 0; i < SGConfig.nApps; ++i) {
                                        pwsz = wcsstr(SGConfig.SGAppList[i].wszEXEName, wszSelectedItemText);
                                        if (pwsz == NULL)
                                            continue;

                                        // delete the entry
                                        memset(&SGConfig.SGAppList[i], 0, sizeof(SGConfig.SGAppList[i]));

                                        // shift the last entry to the deleted position
                                        memcpy(&SGConfig.SGAppList[i], &SGConfig.SGAppList[SGConfig.nApps - 1],
                                            sizeof(SGConfig.SGAppList[i]));

                                        // delete the last entry
                                        memset(&SGConfig.SGAppList[SGConfig.nApps - 1], 0, sizeof(SGConfig.SGAppList[0]));

                                        --SGConfig.nApps;

                                        // write back the config to file
                                        if (WriteSGConfigFile(&SGConfig) == FALSE) {
                                            MessageBox(hDlg, L"Error writing to configuration file", L"Application Removal", 0);
                                            return TRUE;
                                        } else {
                                            // update the UI
                                            if (UpdateSGConfigUI(hDlg) == FALSE)
                                                MessageBox(hDlg, L"Error updating UI",
                                                    L"Application Removal", 0);
                                        }
                                        break;
                                    }// for i
                                }
                            }// if( iSelectedItem != -1 )

                        }// if( ((LPNMHDR)lParam)->hwndFrom == hList )

                        return 0;

                    }// case NM_RCLICK

                }// switch (((LPNMHDR)lParam)->code)
                return FALSE;

            }// case WM_NOTIFY


            case WM_COMMAND:
            {
                switch (LOWORD(wParam)) {
                    case IDB_SGM_CONFIG_ADD:
                    {
                        // retrieve exe name and validate

                        // get number of characters
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_ADD_APP, EM_LINELENGTH, 0, 0);
                        if (wLen > MAX_FILE_NAME) {
                            MessageBox(hDlg, L"Application name too long.", L"Error", MB_ICONERROR);
                            return TRUE;
                        } else if (wLen == 0) {
                            MessageBox(hDlg, L"Please enter the application name", L"Error", MB_ICONERROR);
                            return TRUE;
                        }

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszEXEName) = wLen;

                        // Get the characters. 
                        SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_ADD_APP, EM_GETLINE,
                            (WPARAM)0,       // line 0 
                            (LPARAM)wszEXEName);

                        // Null-terminate the string.
                        wszEXEName[wLen] = 0;

                        pwsz = wcsstr(wszEXEName, L".exe");
                        if (pwsz == NULL) {
                            MessageBox(hDlg, L"Invalid Application Name", L"Error", MB_ICONERROR);
                            return TRUE;
                        }

                        // retrieve RTinterval and Offline interval
                        // get number of characters
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_OFF_INT, EM_LINELENGTH, 0, 0);

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszOffInt) = wLen;

                        // Get the characters. 
                        SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_OFF_INT, EM_GETLINE,
                            (WPARAM)0,       // line 0 
                            (LPARAM)wszOffInt);

                        // Null-terminate the string.
                        wszOffInt[wLen] = 0;

                        // get number of characters
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_RT_INT, EM_LINELENGTH, 0, 0);

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszRTInt) = wLen;

                        // Get the characters. 
                        SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_RT_INT, EM_GETLINE,
                            (WPARAM)0,       // line 0 
                            (LPARAM)wszRTInt);

                        // Null-terminate the string.
                        wszRTInt[wLen] = 0;

                        // retrieve radio buttons' status
                        for (i = IDC_SGM_CONFIG_AS; i <= IDC_SGM_CONFIG_FS; ++i) {
                            if (IsDlgButtonChecked(hDlg, i) == BST_CHECKED) {
                                // get the window text
                                if ((wLen = GetWindowText(GetDlgItem(hDlg, i),
                                    wszRadioBtnText, MAX_RADIO_BTN_NAME)) == 0) {
                                    MessageBox(hDlg, L"Cannot retrieve radio button text",
                                        L"Error", MB_ICONERROR);
                                    return TRUE;
                                }
                                break;
                            }
                        }

                        // update list view
                        nItems = ListView_GetItemCount(hList);

                        lviToInsert.iItem = nItems;
                        lviToInsert.mask = LVIF_TEXT;

                        // insert the application name
                        lviToInsert.iSubItem = 0;
                        lviToInsert.pszText = wszEXEName;
                        lviToInsert.cchTextMax = MAX_FILE_NAME + 1;
                        iRetVal = SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&lviToInsert);

                        // insert the shot type
                        lviToInsert.iSubItem = 1;
                        lviToInsert.pszText = wszRadioBtnText;
                        lviToInsert.cchTextMax = MAX_RADIO_BTN_NAME + 1;
                        iRetVal = SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&lviToInsert);

                        // read config file and get current config
                        if (ReadSGConfigFile(&SGConfig) == FALSE) {
                            MessageBox(hDlg, L"Error reading configuration file", L"Error", MB_ICONERROR);
                            return TRUE;
                        }

                        // check for duplicate items
                        for (i = 0; i < SGConfig.nApps; ++i) {
                            if (wcscmp(SGConfig.SGAppList[i].wszEXEName, wszEXEName) == 0) {
                                MessageBox(hDlg, L"Application already exists.", L"Add Application",
                                    MB_ICONEXCLAMATION | MB_OK);
                                return TRUE;
                            }
                        }

                        // update the values in the structure
                        SGConfig.nApps += 1;
                        SGConfig.ulOffInterval = (unsigned long)CHANGE_TO_MS_FROM_MIN(_wtoi(wszOffInt));
                        SGConfig.ulRTInterval = (unsigned long)CHANGE_TO_MS_FROM_SEC(_wtoi(wszRTInt));

                        if (wcscpy_s(SGConfig.SGAppList[SGConfig.nApps - 1].wszEXEName,
                            MAX_FILE_NAME, wszEXEName) != 0) {
                            // error 
                        }

                        // shot type
                        if (wcscmp(wszRadioBtnText, ASHOT_TEXT) == 0)
                            SGConfig.SGAppList[SGConfig.nApps - 1].iShotType = (SHOT_TYPE)ApplicationShot;
                        else if (wcscmp(wszRadioBtnText, MSHOT_TEXT) == 0)
                            SGConfig.SGAppList[SGConfig.nApps - 1].iShotType = (SHOT_TYPE)MouseShot;
                        else if (wcscmp(wszRadioBtnText, FSHOT_TEXT) == 0)
                            SGConfig.SGAppList[SGConfig.nApps - 1].iShotType = (SHOT_TYPE)FullscreenShot;

                        // write the changed config to file
                        if ((iRetVal = WriteSGConfigFile(&SGConfig)) == FALSE) {
                            MessageBox(hDlg, L"Error writing configuration file", L"Error", MB_ICONERROR);
                            return TRUE;
                        }

                        // clear the application name edit box
                        SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_ADD_APP, EM_SETSEL, 0, -1);
                        SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_ADD_APP, EM_REPLACESEL,
                            FALSE, //operation cannot be undone
                            (LPARAM)(LPCSTR)L"");

                        return TRUE;
                    }

                    case IDC_SGM_CONFIG_APPLIST:

                        return TRUE;


                    case IDB_SGM_CONFIG_UREMOTE:
                    {
                        // retrieve RTinterval and Offline interval
                        // get number of characters
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_OFF_INT, EM_LINELENGTH, 0, 0);

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszOffInt) = wLen;

                        // Get the characters. 
                        SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_OFF_INT, EM_GETLINE,
                            (WPARAM)0,       // line 0 
                            (LPARAM)wszOffInt);

                        // Null-terminate the string.
                        wszOffInt[wLen] = 0;

                        // get number of characters
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_RT_INT, EM_LINELENGTH, 0, 0);

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszRTInt) = wLen;

                        // Get the characters. 
                        SendDlgItemMessage(hDlg, IDC_SGM_CONFIG_RT_INT, EM_GETLINE,
                            (WPARAM)0,       // line 0 
                            (LPARAM)wszRTInt);

                        // Null-terminate the string.
                        wszRTInt[wLen] = 0;

                        // read config from structure
                        if (ReadSGConfigFile(&SGConfig) == FALSE) {
                            MessageBox(hDlg, L"Error reading configuration file", L"Error", MB_ICONERROR);
                            return TRUE;
                        }

                        // update time interval values
                        SGConfig.ulOffInterval = CHANGE_TO_MS_FROM_MIN(_wtoi(wszOffInt));
                        SGConfig.ulRTInterval = CHANGE_TO_MS_FROM_SEC(_wtoi(wszRTInt));

                        // write updated config to file
                        if (WriteSGConfigFile(&SGConfig) == FALSE) {
                            MessageBox(hDlg, L"Error writing configuration file", L"Error", MB_ICONERROR);
                            return TRUE;
                        }

                        pSPMArgs->cmd = SPM_SENDDATA;
                        pSPMArgs->iFromPlugin = SCREENGRABBER;
                        pSPMArgs->MNTRData.SGMData.cmd = SGIF_UPDATE_CONFIG;
                        memcpy(&pSPMArgs->MNTRData.SGMData.SGMStructs.SGConfig, &SGConfig, sizeof(SGCONFIG));

                        if ((iRetVal = ServerPluginManager(0, pSPMArgs, NULL)) == FALSE)
                            PrintToStatusWindow(L"SGM: Update config data not sent");
                        else
                            PrintToStatusWindow(L"SGM: Update config data sent");

                        return TRUE;
                    }

                }// switch(LOWORD(wParam))
                return FALSE;
            }


            case WM_CLOSE:
            {
                EndDialog(hDlg, 0);
                return TRUE;
            }

        }// switch(message)
        return FALSE;
    }
    __finally {
        if (pSPMArgs)
            free(pSPMArgs);
    }
}

BOOL DisplayImage(HWND hPictureCtrl, WCHAR *wszFilePath, int cxDesired, int cyDesired) {

    static HANDLE hOldBitmapImage;
    HANDLE hNewBitmapImage;
    int iRetVal;

    FILE *fp = NULL;
    BMPFILE_MAGIC BMPMagic;
    BMPFILE_HEADER BMPHeader;
    BMPFILE_DIBINFO DIBInfo;

    if (hOldBitmapImage)
        DeleteObject(hOldBitmapImage);

    // get image size

    // open file in binary mode
    if ((iRetVal = _wfopen_s(&fp, wszFilePath, L"rb")) != 0)
        return FALSE;

    // read the magic number
    fread(&BMPMagic, sizeof(BMPFILE_MAGIC), 1, fp);

    // read the header
    fread(&BMPHeader, sizeof(BMPFILE_HEADER), 1, fp);

    // read the info
    fread(&DIBInfo, sizeof(BMPFILE_DIBINFO), 1, fp);

    // close the file
    fclose(fp);
    fp = NULL;

    // resize only if image is larger than display area
    if ((int)DIBInfo.width > cxDesired || (int)DIBInfo.height > cyDesired) {
        hNewBitmapImage = LoadImage(NULL,			// stand-alone resource
            wszFilePath,	// file path
            IMAGE_BITMAP,	// type of image IMAGE_BITMAP/IMAGE_CURSOR/IMAGE_ICON
            cxDesired,
            cyDesired,
            LR_LOADFROMFILE);
    } else {
        hNewBitmapImage = LoadImage(NULL,			// stand-alone resource
            wszFilePath,	// file path
            IMAGE_BITMAP,	// type of image IMAGE_BITMAP/IMAGE_CURSOR/IMAGE_ICON
            0,
            0,
            LR_LOADFROMFILE);
    }

    if (hNewBitmapImage == NULL) {
        iRetVal = GetLastError();
        return FALSE;
    }
    hOldBitmapImage = hNewBitmapImage;

    iRetVal = SendMessage(hPictureCtrl, STM_SETIMAGE, IMAGE_BITMAP, LPARAM(hNewBitmapImage));

    return TRUE;
}

BOOL DisplayNodeImage(HWND hTree, HTREEITEM hItem, HWND hPicCtrl, int cx, int cy) {
    int iRetVal;
    WCHAR *pwsz = NULL;
    TVITEM tviSelectedItem = { 0 };
    WCHAR wszSelImage[MAX_PATH + 1];
    WCHAR wszSelImagePath[MAX_PATH + 1];

    if (fConnected == FALSE) {
        MessageBox(0, L"Not connected to client!", L"Admin GUI", MB_ICONEXCLAMATION);
        return FALSE;
    }

    //get info about selected item
    memset(&tviSelectedItem, 0, sizeof(tviSelectedItem));
    tviSelectedItem.mask = TVIF_PARAM | TVIF_TEXT;
    tviSelectedItem.hItem = hItem;
    tviSelectedItem.pszText = wszSelImage;
    tviSelectedItem.cchTextMax = MAX_PATH + 1;

    if (SendMessage(hTree, TVM_GETITEM, 0, (LPARAM)&tviSelectedItem) == FALSE) {
        // error
        return FALSE;
    } else {
        if (tviSelectedItem.lParam == NULL) {
            // realtime shot
            // construct the path
            swprintf_s(wszSelImagePath, MAX_PATH + 1, L"%s\\%s\\%s", g_wszCurCliDIR, FP_CLIENT_RT_SHOTS_DIR, wszSelImage);

            // Call function DisplayImage() to display the 
            // BITMAP image in the picture control.
            if ((iRetVal = DisplayImage(hPicCtrl, wszSelImagePath, cx, cy)) == FALSE)
                MessageBox(0, L"Error in displaying the image", L"Display Node Image", MB_ICONINFORMATION);

            return TRUE;
        }

        // check if the node is an image node
        else if ((pwsz = wcsstr((WCHAR*)tviSelectedItem.lParam,
            L".bmp")) == NULL) {
            return TRUE;
        }

        // Call function DisplayImage() to display the 
        // BITMAP image in the picture control.
        if ((iRetVal = DisplayImage(hPicCtrl, (WCHAR*)tviSelectedItem.lParam, cx, cy)) == FALSE)
            MessageBox(0, L"Error in displaying the image", L"Display Node Image", MB_ICONINFORMATION);
    }

    return TRUE;
}

BOOL ReadSGConfigFile(SGCONFIG *pSGConfig) {
    extern WCHAR g_wszCurCliIP[];

    int		iConfigFile = 0;
    int		iRetVal;
    WCHAR	wszConfigFilePath[MAX_PATH + 1];

    // path
    swprintf_s(wszConfigFilePath, MAX_PATH + 1, L"%s\\%s\\%s", FP_CLIENTS_DIR, g_wszCurCliIP, FP_CLIENT_SGM_CONF);

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
        if ((iRetVal = _read(iConfigFile, pSGConfig, sizeof(SGCONFIG))) <= 0) {
            if (iConfigFile) {
                _close(iConfigFile);
                iConfigFile = 0;
            }
            if (GetLastError() == 0)
                return TRUE;
            return FALSE;
        }

        // close the file
        _close(iConfigFile);
        iConfigFile = 0;
    }
    return TRUE;
}

BOOL WriteSGConfigFile(const SGCONFIG *pSGConfig) {
    extern WCHAR g_wszCurCliIP[];

    int		iConfigFile = 0;
    int		iRetVal;
    WCHAR	wszConfigFilePath[MAX_PATH + 1];

    SGREALAPP SGRealApp = { 0 };

    // path
    wsprintf(wszConfigFilePath, L"%s\\%s\\%s", FP_CLIENTS_DIR, g_wszCurCliIP, FP_CLIENT_SGM_CONF);

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
        } else
            return FALSE;
    }

    // write offline config structure
    if (iConfigFile != 0) {
        if ((iRetVal = _write(iConfigFile, pSGConfig, sizeof(SGCONFIG))) == -1) {
            if (iConfigFile) {
                _close(iConfigFile);
                iConfigFile = 0;
            }
            return FALSE;
        }

        if ((iRetVal = _write(iConfigFile, &SGRealApp, sizeof(SGREALAPP))) == -1) {
            if (iConfigFile) {
                _close(iConfigFile);
                iConfigFile = 0;
            } else
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

BOOL WriteSGRealTimeConfig(const SGREALAPP *pSGRealApp) {
    extern WCHAR g_wszCurCliIP[];

    int		iConfigFile = 0;
    int		iRetVal;
    WCHAR	wszConfigFilePath[MAX_PATH + 1];

    SGCONFIG SGConfig = { 0 };

    if (pSGRealApp == NULL)
        return FALSE;

    // path
    wsprintf(wszConfigFilePath, L"%s\\%s\\%s", FP_CLIENTS_DIR, g_wszCurCliIP, FP_CLIENT_SGM_CONF);

    // get existing offline config structure
    if (ReadSGConfigFile(&SGConfig) == FALSE)
        return FALSE;

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
        } else
            return FALSE;
    }

    // write offline config structure
    if (iConfigFile != 0) {
        if ((iRetVal = _write(iConfigFile, &SGConfig, sizeof(SGCONFIG))) == -1) {
            if (iConfigFile) {
                _close(iConfigFile);
                iConfigFile = 0;
            }
            return FALSE;
        }

        if ((iRetVal = _write(iConfigFile, pSGRealApp, sizeof(SGREALAPP))) == -1) {
            if (iConfigFile) {
                _close(iConfigFile);
                iConfigFile = 0;
            } else
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


BOOL UpdateSGConfigUI(HWND hDlg) {
    int i;
    int iOffInt = 0;
    int iRTInt = 0;

    SGCONFIG SGConfig = { 0 };

    HWND	hList = NULL;
    LVITEM	lviToInsert = { 0 };

    WCHAR	wszRTInt[16];
    WCHAR	wszOffInt[16];

    // get handle to list view
    if ((hList = GetDlgItem(hDlg, IDC_SGM_CONFIG_APPLIST)) == NULL)
        return FALSE;

    // read from file
    if (ReadSGConfigFile(&SGConfig) == FALSE)
        return FALSE;

    // Intervals
    iOffInt = (int)CHANGE_TO_MIN_FROM_MS(SGConfig.ulOffInterval);
    iRTInt = (int)CHANGE_TO_SEC_FROM_MS(SGConfig.ulRTInterval);

    wsprintf(wszOffInt, L"%d", iOffInt);
    wsprintf(wszRTInt, L"%d", iRTInt);
    SetWindowText(GetDlgItem(hDlg, IDC_SGM_CONFIG_OFF_INT), wszOffInt);
    SetWindowText(GetDlgItem(hDlg, IDC_SGM_CONFIG_RT_INT), wszRTInt);

    ListView_DeleteAllItems(hList);

    lviToInsert.mask = LVIF_TEXT;

    // Application List
    for (i = 0; i < SGConfig.nApps; ++i) {
        lviToInsert.iItem = i;
        lviToInsert.iSubItem = 0;
        lviToInsert.pszText = SGConfig.SGAppList[i].wszEXEName;
        SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&lviToInsert);

        lviToInsert.iSubItem = 1;
        switch (SGConfig.SGAppList[i].iShotType) {
            case (SHOT_TYPE)ApplicationShot:
            {
                lviToInsert.pszText = ASHOT_TEXT;
                break;
            }

            case (SHOT_TYPE)MouseShot:
            {
                lviToInsert.pszText = MSHOT_TEXT;
                break;
            }

            case (SHOT_TYPE)FullscreenShot:
            {
                lviToInsert.pszText = FSHOT_TEXT;
                break;
            }

            default:
            {
                lviToInsert.pszText = L"Invalid shot type";
            }
        }

        SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&lviToInsert);

    }// for i

    return TRUE;
}

DWORD WINAPI FTGetSnapShots(LPVOID lpArgs) {
    extern WCHAR g_wszCurCliIP[];
    extern WCHAR g_wszCurCliDIR[];

    DWORD dwEventRetVal;
    int i, j, iRetVal, nCount, nFiles;

    HWND hPMsg = NULL;
    HWND hProgressBar = NULL;
    HANDLE hFTCompleteEvent = NULL;

    PDATA *pPData = NULL;
    SGFILEDATA *pSGFileData = NULL;

    WCHAR wszSWMsg[MAX_SW_MSG + 1] = L"";
    WCHAR wszLogFileMsg[MAX_LOGFILE_LINE + 1] = L"";

    WCHAR wszDayDir[MAX_PATH + 1];
    WCHAR wszAppDir[MAX_PATH + 1];
    WCHAR wszDestPath[MAX_PATH + 1];
    WCHAR wszPMsg[MAX_PROGRESS_MSG + 1];
    WCHAR wszSnapShotsDir[MAX_PATH + 1];
    WCHAR wszFileName[MAX_FILE_NAME + 1];
    WCHAR wszCurWorkDir[MAX_PATH + 1] = L"";
    WCHAR *pwszPrevToken = NULL, *pwszCurToken = NULL;

    pPData = (PDATA*)lpArgs;
    pSGFileData = &pPData->PDataType.SGFileData;

    nFiles = pSGFileData->nFiles;

    if (nFiles == 0) {
        MessageBox(hSGMRetDlg, L"No files to download", L"Get Snapshots", MB_ICONINFORMATION | MB_OK);
        return 0;
    }

    if (GetCurrentWorkDir(wszCurWorkDir) == FALSE) {
        PrintToStatusWindow(L"Unable to retrieve current working directory.");
        return FALSE;
    }

    // create an event which will be used to get notification of file transfer completion
    hFTCompleteEvent = CreateEvent(NULL,	// default security attributes
        TRUE,	// manual reset using ResetEvent()
        FALSE,	// non-signaled initial state
        EVENT_FT_COMPLETE);	// name of the event object

    if (hFTCompleteEvent == NULL) {
        iRetVal = GetLastError();
        return FALSE;
    }

    // disable the Retrieve button
    EnableWindow(GetDlgItem(hSGMRetDlg, IDB_SGM_RET_RETRIEVE), FALSE);

    // enable Stop button
    EnableWindow(GetDlgItem(hSGMRetDlg, IDB_SGM_RET_STOP), TRUE);

    // handle of the static text above the progress bar
    if ((hPMsg = GetDlgItem(hSGMRetDlg, IDC_SGM_RET_STATIC)) == NULL) {
        iRetVal = GetLastError();
    }
    ShowWindow(hPMsg, SW_SHOW);

    // progress bar control
    if ((hProgressBar = GetDlgItem(hSGMRetDlg, IDC_SGM_RET_PROGRESS)) == NULL) {
        iRetVal = GetLastError();
    }

    // set the range
    iRetVal = SendMessage(hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, nFiles));

    // set the step
    iRetVal = SendMessage(hProgressBar, PBM_SETSTEP, 1, NULL);

    // set the initial value
    iRetVal = SendMessage(hProgressBar, PBM_SETPOS, 0, NULL);

    // Example path: <C:\...>\Plugins\SG\SnapShots\12-5-2010\firefox.exe12-34-54-123\Blue Lace 16.bmp

    // set fFTDeleteFile to delete the log file on the client machine after successful download
    fFTDeleteFile = TRUE;

    swprintf_s(wszSnapShotsDir, MAX_PATH + 1, L"%s\\%s", g_wszCurCliDIR, FP_CLIENT_SHOTS_DIR);
    for (i = 0; i < nFiles; ++i) {
        // get the current file's file name, along with the application directory
        // and day directory from the absolute path.
        pwszCurToken = pSGFileData->ListOfFiles[i].wszFileName;
        while (*pwszCurToken++ != 0);

        // Starting from the end, traverse in reverse until you count 3 back-slashes.
        // Once done, skip over the 3rd '\' and now you have the hierarchy of
        // DayDir\AppDir\Filename.bmp
        nCount = 0;
        while (nCount < 3) {
            --pwszCurToken;
            if (*pwszCurToken == L'\\')
                ++nCount;
        }

        // move past the '\'
        ++pwszCurToken;

        // copy the DayDir
        for (j = 0; *pwszCurToken != '\\' && j < MAX_PATH + 1; ++j, ++pwszCurToken) {
            wszDayDir[j] = *pwszCurToken;
        }
        wszDayDir[j] = 0;

        // move past the '\'
        ++pwszCurToken;

        // copy the AppDir
        for (j = 0; *pwszCurToken != '\\' && j < MAX_PATH + 1; ++j, ++pwszCurToken) {
            wszAppDir[j] = *pwszCurToken;
        }
        wszAppDir[j] = 0;

        // move past the '\'
        ++pwszCurToken;

        // copy the Filename
        for (j = 0; *pwszCurToken != 0 && j < MAX_PATH + 1; ++j, ++pwszCurToken) {
            wszFileName[j] = *pwszCurToken;
        }
        wszFileName[j] = 0;

        // create the DayDir
        swprintf_s(wszDestPath, MAX_PATH + 1, L"%s\\%s", wszSnapShotsDir, wszDayDir);
        iRetVal = CreateDirectory(wszDestPath, NULL);

        // create the AppDir
        swprintf_s(wszDestPath, MAX_PATH + 1, L"%s\\%s\\%s", wszSnapShotsDir, wszDayDir, wszAppDir);
        iRetVal = CreateDirectory(wszDestPath, NULL);

        // show status in static text
        swprintf_s(wszPMsg, MAX_PROGRESS_MSG + 1, L"Downloading: %s...", wszFileName);
        SetWindowText(hPMsg, wszPMsg);

        // construct the absolute path for the destination file (local path)
        swprintf_s(wszDestPath, MAX_PATH + 1, L"%s\\%s\\%s\\%s", wszSnapShotsDir, wszDayDir, wszAppDir, wszFileName);

        if ((iRetVal = GetFile(pSGFileData->ListOfFiles[i].wszFileName, wszDestPath)) == FALSE) {
            swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"Unable to retrieve file: %s", wszFileName);
            MessageBox(hSGMRetDlg, wszSWMsg, L"Retrieve Files", MB_ICONINFORMATION);
        }
        // wait for notification that the file has been downloaded
        dwEventRetVal = WaitForSingleObject(hFTCompleteEvent, INFINITE);
        if (dwEventRetVal == WAIT_OBJECT_0)
            ResetEvent(hFTCompleteEvent);

        if (fFTSuccess == FALSE) {
            swprintf_s(wszSWMsg, MAX_SW_MSG + 1,
                L"There was an error while downloading %s. See the log file for more details.", wszFileName);
            MessageBox(hSGMRetDlg, wszSWMsg, L"Retrieve Files", MB_ICONINFORMATION);

            switch (iFTStatus) {
                case FT_ERROR_FILE_CORRUPTED:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. File Corrupted.", wszFileName);
                    break;
                }

                case FT_ERROR_CONN_CLOSED:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. Connection closed.", wszFileName);
                    break;
                }

                case FT_ERROR_FNF:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. File not found not client machine.", wszFileName);
                    break;
                }

                case FT_ERROR_OPEN_DEST_FILE:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. Open failed.", wszFileName);
                    break;
                }

                case FT_ERROR_WRITE_TO_FILE:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. Write failed.", wszFileName);
                    break;
                }

                default:
                {
                    swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s", wszFileName);
                    break;
                }
            }

            WriteToLogFile(FP_SERVER_LOGFILE, L"SGMonDP", L"FTGetSnapShots", wszLogFileMsg, 0);
        }

        // update progress bar
        iRetVal = SendMessage(hProgressBar, PBM_STEPIT, 0, 0);

        // continue to next file in the list

    }// for

    // show status in static text
    swprintf_s(wszPMsg, MAX_PROGRESS_MSG + 1, L"All Files Download Complete");
    SetWindowText(hPMsg, wszPMsg);

    Sleep(1500);

    // hide the static text and clear the progress bar
    ShowWindow(hPMsg, SW_HIDE);
    iRetVal = SendMessage(hProgressBar, PBM_SETPOS, 0, NULL);

    // Re-enable the Retrieve button
    EnableWindow(GetDlgItem(hSGMRetDlg, IDB_SGM_RET_RETRIEVE), TRUE);

    // disable Stop button
    EnableWindow(GetDlgItem(hSGMRetDlg, IDB_SGM_RET_STOP), FALSE);

    if (pPData) {
        free(pPData);
        pPData = NULL;
    }

    return TRUE;
}

BOOL SGMonOffDPListImages(HWND hDlg) {
    extern WCHAR g_wszCurCliDIR[];

    int	i, iRetVal;
    int	nImages = 0;

    HWND			hTree = NULL;
    TVITEM			tviCurNode = { 0 };
    TVINSERTSTRUCT	tviToInsert = { 0 };
    HTREEITEM		hCurDayNode = NULL;
    HTREEITEM		hCurAppNode = NULL;
    HTREEITEM		hInsertedItem = NULL;
    FILELIST		flFileList[MAX_FILES];

    WCHAR *pwszPath = NULL;
    WCHAR wszCurDayDir[MAX_PATH + 1] = L"";
    WCHAR wszCurAppDir[MAX_PATH + 1] = L"";
    WCHAR wszCurNodeText[MAX_PATH + 1] = L"";
    WCHAR wszSnapShotsDir[MAX_PATH + 1] = L"";
    WCHAR wszCurNodeDayDir[MAX_PATH + 1] = L"";

    hTree = GetDlgItem(hDlg, IDC_SGM_OFF_TREE);

    // erase tree
    TreeView_DeleteAllItems(hTree);

    // construct and store the snap shots directory path
    swprintf_s(wszSnapShotsDir, MAX_PATH + 1, L"%s\\%s", g_wszCurCliDIR, FP_CLIENT_SHOTS_DIR);

    // first, insert nodes for the day directories

    // get the list of sub directories in the SnapShots directory
    if ((nImages = ListDir(wszSnapShotsDir, L"*.*", flFileList, MAX_FILES)) < 0) {
        // error				
        iRetVal = GetLastError();
    } else if (nImages == 0) {
        // no files to display
    } else {
        // retrieve handle to the tree view control
        if ((hTree = GetDlgItem(hDlg, IDC_SGM_OFF_TREE)) == NULL) {
            // error
        }
        tviToInsert.hInsertAfter = TVI_LAST;
        tviToInsert.hParent = NULL;
        tviToInsert.item.cChildren = 1;
        tviToInsert.item.mask = TVIF_TEXT | TVIF_PARAM;

        // insert the day directory names into the tree view
        for (i = 0; i < nImages; ++i) {
            if (wcscmp(flFileList[i].wszFileName, L".") == 0 ||
                wcscmp(flFileList[i].wszFileName, L"..") == 0)
                continue;

            // allocate memory for holding the path of a node's directory / file
            if ((pwszPath = (WCHAR*)malloc(sizeof(WCHAR) * (MAX_PATH + 1))) == NULL) {
                PrintToStatusWindow(L"SGM: malloc error: node path");
                return FALSE;
            }
            wsprintf(pwszPath, L"%s\\%s", wszSnapShotsDir, flFileList[i].wszFileName);
            tviToInsert.item.pszText = flFileList[i].wszFileName;
            tviToInsert.item.cchTextMax = wcslen(flFileList[i].wszFileName) + 1;
            tviToInsert.item.lParam = (LPARAM)pwszPath;
            //WriteToAddressFile(pwszPath, 0);

            //send message TVM_INSERTITEM
            if ((hInsertedItem = (HTREEITEM)SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tviToInsert)) == NULL)
                continue;
        }
    }

    // loop for day nodes
    for (hCurDayNode = TreeView_GetRoot(hTree);
        hCurDayNode != NULL;
        hCurDayNode = TreeView_GetNextSibling(hTree, hCurDayNode)) {
        // get the text of the current node
        tviCurNode.hItem = hCurDayNode;
        tviCurNode.mask = TVIF_TEXT;
        tviCurNode.pszText = wszCurNodeDayDir;
        tviCurNode.cchTextMax = MAX_PATH + 1;

        if (TreeView_GetItem(hTree, &tviCurNode) == FALSE) {
            return FALSE;
        }

        // construct the path
        swprintf_s(wszCurDayDir, MAX_PATH + 1, L"%s\\%s", wszSnapShotsDir, wszCurNodeDayDir);

        // get the list of sub directories in the SnapShots directory
        if ((nImages = ListDir(wszCurDayDir, L"*.*", flFileList, MAX_FILES)) < 0) {
            // error				
            iRetVal = GetLastError();
        } else if (nImages == 0) {
            // no files to display
        } else {
            tviToInsert.hInsertAfter = TVI_LAST;
            tviToInsert.hParent = hCurDayNode;
            tviToInsert.item.cChildren = 1;
            tviToInsert.item.mask = TVIF_TEXT | TVIF_PARAM;

            // insert the file names into the tree view
            for (i = 0; i < nImages; ++i) {
                if (wcscmp(flFileList[i].wszFileName, L".") == 0 ||
                    wcscmp(flFileList[i].wszFileName, L"..") == 0)
                    continue;

                // allocate memory for holding the path of a node's directory / file
                if ((pwszPath = (WCHAR*)malloc(sizeof(WCHAR) * (MAX_PATH + 1))) == NULL) {
                    PrintToStatusWindow(L"SGM: malloc error: node path");
                    return FALSE;
                }
                wsprintf(pwszPath, L"%s\\%s", wszCurDayDir, flFileList[i].wszFileName);

                tviToInsert.item.pszText = flFileList[i].wszFileName;
                tviToInsert.item.cchTextMax = wcslen(flFileList[i].wszFileName) + 1;
                tviToInsert.item.lParam = (LPARAM)pwszPath;
                //WriteToAddressFile(pwszPath, 0);

                //send message TVM_INSERTITEM
                if ((hInsertedItem = (HTREEITEM)SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tviToInsert)) == NULL)
                    continue;
            }
        }
    }

    // loop for day directory nodes
    for (hCurDayNode = TreeView_GetRoot(hTree);
        hCurDayNode != NULL;
        hCurDayNode = TreeView_GetNextSibling(hTree, hCurDayNode)) {
        // loop for application directories under a day directory
        for (hCurAppNode = TreeView_GetChild(hTree, hCurDayNode);
            hCurAppNode != NULL;
            hCurAppNode = TreeView_GetNextSibling(hTree, hCurAppNode)) {
            // get the path of the current application directory
            memset(&tviCurNode, 0, sizeof(TVITEM));
            tviCurNode.mask = TVIF_PARAM;
            tviCurNode.hItem = hCurAppNode;
            if (TreeView_GetItem(hTree, &tviCurNode) == FALSE) {
                // error
                continue;
            }

            // get the list of sub directories in the SnapShots directory
            if ((nImages = ListDir((WCHAR*)tviCurNode.lParam, L"*.bmp", flFileList, MAX_FILES)) < 0) {
                // error				
                iRetVal = GetLastError();
            } else if (nImages == 0) {
                // no files to display
            } else {
                // store the current application directory path
                wcscpy_s(wszCurAppDir, MAX_PATH + 1, (WCHAR*)tviCurNode.lParam);

                tviToInsert.hInsertAfter = TVI_LAST;
                tviToInsert.hParent = hCurAppNode;
                tviToInsert.item.cChildren = 1;
                tviToInsert.item.mask = TVIF_TEXT | TVIF_PARAM;

                // insert the file names into the tree view
                for (i = 0; i < nImages; ++i) {
                    if (wcscmp(flFileList[i].wszFileName, L".") == 0 ||
                        wcscmp(flFileList[i].wszFileName, L"..") == 0)
                        continue;

                    // allocate memory for holding the path of a node's directory / file
                    if ((pwszPath = (WCHAR*)malloc(sizeof(WCHAR) * (MAX_PATH + 1))) == NULL) {
                        PrintToStatusWindow(L"SGM: malloc error: node path");
                        return FALSE;
                    }
                    wsprintf(pwszPath, L"%s\\%s", wszCurAppDir, flFileList[i].wszFileName);

                    tviToInsert.item.pszText = flFileList[i].wszFileName;
                    tviToInsert.item.cchTextMax = wcslen(flFileList[i].wszFileName) + 1;
                    tviToInsert.item.lParam = (LPARAM)pwszPath;
                    //WriteToAddressFile(pwszPath, 0);

                    //send message TVM_INSERTITEM
                    if ((hInsertedItem = (HTREEITEM)SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tviToInsert)) == NULL)
                        continue;

                }// for i

            }// else - if

        }

    }

    return TRUE;
}

BOOL ListRTImages() {
    extern WCHAR g_wszCurCliDIR[];

    int i, iRetVal, nFiles;
    HANDLE hSearch = NULL;

    HWND			hTree = NULL;
    TVITEM			tviCurNode = { 0 };
    TVINSERTSTRUCT	tviToInsert = { 0 };
    WCHAR			*pwszPath = NULL;
    WCHAR			wszDirPath[MAX_PATH + 1];
    FILELIST		flFileList[MAX_FILES];

    if ((hTree = GetDlgItem(hSGMRTDlg, IDC_SGM_RT_TREE)) == NULL) {
        // error
        return FALSE;
    }

    // realtime shots directory path
    swprintf_s(wszDirPath, MAX_PATH + 1, L"%s\\%s", g_wszCurCliDIR, FP_CLIENT_RT_SHOTS_DIR);

    // list files in the directory
    nFiles = -1;

    // list the files, call ListDir()
    if ((nFiles = ListDir(wszDirPath, L"*.bmp", flFileList, MAX_FILES)) <= 0)
        return TRUE;

    TreeView_DeleteAllItems(hTree);

    // setup the TVINSERTSTRUCT
    tviToInsert.hInsertAfter = TVI_LAST;
    tviToInsert.hParent = NULL;
    tviToInsert.item.cChildren = 0;
    tviToInsert.item.mask = TVIF_TEXT | TVIF_PARAM;

    for (i = 0; i < nFiles; ++i) {
        // insert the first item
        tviToInsert.item.pszText = flFileList[i].wszFileName;
        tviToInsert.item.cchTextMax = wcslen(flFileList[i].wszFileName) + 1;

        // allocate memory for holding the path of a node's directory / file
        if ((pwszPath = (WCHAR*)malloc(sizeof(WCHAR) * (MAX_PATH + 1))) == NULL) {
            PrintToStatusWindow(L"SGM: malloc error: node path");
            return FALSE;
        }
        swprintf_s(pwszPath, MAX_PATH + 1, L"%s\\%s", wszDirPath, flFileList[i].wszFileName);

        tviToInsert.item.lParam = (LPARAM)pwszPath;

        if ((iRetVal = SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tviToInsert)) == NULL) {
            continue;
        }
    }

    return TRUE;
}

DWORD WINAPI GetRTShots(LPVOID lpArgs) {
    extern WCHAR g_wszCurCliDIR[];

    int i, iRetVal;
    WCHAR wszDestPath[MAX_PATH + 1];
    WCHAR *pwszPrevToken = NULL, *pwszCurToken = NULL;

    WCHAR wszSWMsg[MAX_SW_MSG + 1];
    WCHAR wszLogFileMsg[MAX_LOGFILE_LINE + 1] = L"";

    DWORD dwEventRetVal;
    HANDLE hFTCompleteEvent = NULL;

    PDATA *pFilesToDownload = NULL;

    HWND hStatusText = NULL;

    if ((hStatusText = GetDlgItem(hSGMRTDlg, IDC_SGM_RT_STATUS)) == NULL) {
        return 1;
    }

    if ((pFilesToDownload = (PDATA*)lpArgs) == NULL) {
        // error
        return 1;
    }

    if (pFilesToDownload->PDataType.SGFileData.nFiles <= 0) {
        return 1;
    }

    // create an event which will be used to get notification of file transfer completion
    hFTCompleteEvent = CreateEvent(NULL,	// default security attributes
        TRUE,	// manual reset using ResetEvent()
        FALSE,	// non-signaled initial state
        EVENT_FT_COMPLETE);	// name of the event object

    if (hFTCompleteEvent == NULL) {
        iRetVal = GetLastError();
        return FALSE;
    }

    // show status
    ShowWindow(hStatusText, SW_SHOW);

    // set fFTDeleteFile to delete the log file on the client machine after successful download
    fFTDeleteFile = TRUE;

    for (i = 0; i < pFilesToDownload->PDataType.SGFileData.nFiles; ++i) {
        // get the current file's file name from the absolute path.
        pwszPrevToken = pFilesToDownload->PDataType.SGFileData.ListOfFiles[i].wszFileName;
        while ((pwszCurToken = wcsstr(pwszPrevToken, L"\\")) != NULL) {
            pwszPrevToken = pwszCurToken;

            //skip over '\'
            ++pwszPrevToken;
        }

        swprintf_s(wszDestPath, MAX_PATH + 1, L"%s\\%s\\%s", g_wszCurCliDIR, FP_CLIENT_RT_SHOTS_DIR, pwszPrevToken);
        if ((iRetVal = GetFile(pFilesToDownload->PDataType.SGFileData.ListOfFiles[i].wszFileName, wszDestPath)) == FALSE) {
            swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"Unable to retrieve file: %s", pwszPrevToken);
            MessageBox(0, wszSWMsg, L"Retrieve Files", MB_ICONINFORMATION);
        }

        // wait for notification that the file has been downloaded
        dwEventRetVal = WaitForSingleObject(hFTCompleteEvent, INFINITE);
        if (dwEventRetVal == WAIT_OBJECT_0)
            ResetEvent(hFTCompleteEvent);

        if (fFTSuccess == FALSE) {
            swprintf_s(wszSWMsg, MAX_SW_MSG + 1,
                L"There was an error while downloading %s. See the log file for more details.", pwszPrevToken);
            MessageBox(0, wszSWMsg, L"Retrieve Files", MB_ICONINFORMATION);

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

            WriteToLogFile(FP_SERVER_LOGFILE, L"SGMonDP", L"GetRTShots", wszLogFileMsg, 0);
        }

        UpdateRTTree(hSGMRTDlg, pwszPrevToken);

        // continue to next file in the list

    }

    // hide status text
    ShowWindow(hStatusText, SW_HIDE);


    // free
    if (pFilesToDownload) {
        free(pFilesToDownload);
        pFilesToDownload = NULL;
    }

    return 0;
}

BOOL UpdateRTTree(HWND hDlg, const WCHAR *pwszImageFilePath) {
    int iRetVal;

    HWND hTree;
    TVINSERTSTRUCT tviToInsert = { 0 };

    WCHAR wszImageFile[MAX_PATH + 1] = L"";

    if (wcscpy_s(wszImageFile, MAX_PATH + 1, pwszImageFilePath) != 0) {
        return FALSE;
    }

    hTree = GetDlgItem(hSGMRTDlg, IDC_SGM_RT_TREE);

    tviToInsert.hInsertAfter = TVI_LAST;
    tviToInsert.hParent = NULL;
    tviToInsert.item.mask = TVIF_TEXT;
    tviToInsert.item.pszText = wszImageFile;
    tviToInsert.item.cchTextMax = wcsnlen_s(wszImageFile, MAX_PATH + 1) + 1;
    if ((iRetVal = SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tviToInsert)) == NULL) {
        return FALSE;
    }

    return TRUE;
}

BOOL SGMonUpdateRet(HWND hDlg) {
    int i, iRetVal;
    HWND hList = NULL;
    SHOT_TYPE ShotType;
    LVITEM lviToInsert = { 0 };
    SGCONFIG *pSGConfig = NULL;


    if ((hList = GetDlgItem(hDlg, IDC_SGM_RET_APPLIST)) == NULL)
        return FALSE;

    // clear the list first
    ListView_DeleteAllItems(hList);

    lviToInsert.mask = LVIF_TEXT;

    __try {
        if ((pSGConfig = (SGCONFIG*)malloc(sizeof(SGCONFIG))) == NULL) {
            // show error message and close dialog box
            return TRUE;
        }
        memset(pSGConfig, 0, sizeof(SGCONFIG));

        // read config file
        if ((iRetVal = ReadSGConfigFile(pSGConfig)) == FALSE) {
            // error
            return TRUE;
        }

        for (i = 0; i < pSGConfig->nApps; ++i) {
            // application name
            lviToInsert.iItem = i;
            lviToInsert.iSubItem = 0;
            lviToInsert.pszText = pSGConfig->SGAppList[i].wszEXEName;
            lviToInsert.cchTextMax = wcsnlen_s(pSGConfig->SGAppList[i].wszEXEName,
                MAX_FILE_NAME + 1) + 1;
            SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&lviToInsert);

            // shot type
            ShotType = (SHOT_TYPE)pSGConfig->SGAppList[i].iShotType;
            if (ShotType == (SHOT_TYPE)ApplicationShot) {
                lviToInsert.pszText = ASHOT_TEXT;
                lviToInsert.cchTextMax = wcslen(ASHOT_TEXT) + 1;
            } else if (ShotType == (SHOT_TYPE)MouseShot) {
                lviToInsert.pszText = MSHOT_TEXT;
                lviToInsert.cchTextMax = wcslen(MSHOT_TEXT) + 1;
            } else if (ShotType == (SHOT_TYPE)FullscreenShot) {
                lviToInsert.pszText = FSHOT_TEXT;
                lviToInsert.cchTextMax = wcslen(FSHOT_TEXT) + 1;
            } else {
                lviToInsert.pszText = L"Invalid shot type";
                lviToInsert.cchTextMax = wcslen(L"Invalid shot type") + 1;
            }

            lviToInsert.iSubItem = 1;
            SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&lviToInsert);
        }
        return TRUE;
    }
    __finally {
        if (pSGConfig) {
            free(pSGConfig);
            pSGConfig = NULL;
        }
    }
}
