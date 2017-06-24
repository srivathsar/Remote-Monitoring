
// * ServerRM project *
// FBMon.cpp : Dialog Proc for the FBM dialog box

#include "ServerInclude.h"

#define FBM_UI_DLG 0

#define FBM_UI_NETDRIVE_IMG	0
#define FBM_UI_DRIVE_IMG	1
#define FBM_UI_UNSEL_IMG	2
#define FBM_UI_SEL_IMG		3

#define KILOBYTE	1024
#define MEGABYTE	(KILOBYTE * 1024)
#define GIGABYTE	(MEGABYTE * 1024)

extern BOOL fFTDeleteFile;
extern HINSTANCE hMainInstance;
extern PMONITORS g_PMonitors;
extern WCHAR g_wszCurCliDIR[];
extern WCHAR g_wszCurCliIP[];
extern WCHAR g_wszCurCliNick[];

// Global variables
static HWND hFBMUserInterfaceDlg;

// ** Function prototypes **
void WINAPI OnFBMDialogInit(HWND hDlg);
BOOL CALLBACK FBMonUserInterfaceDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// UI Management
BOOL		InsertDrives(HWND hTree, const FBDATA *pFBData);
BOOL		UpdateList(HWND hList, const FBDATA *pFBData);
BOOL		UpdateTree(HWND hTree, HTREEITEM hParentNode, const FBDATA *pFBData);
HTREEITEM	CreateChild(HWND hTree, HTREEITEM hParent, const WCHAR *pwszPath,
    const WCHAR *pwszParentPath);

// File Copy
static DWORD WINAPI FBMFileCopy(LPVOID lpArgs);

// ** File Browser Monitor Dialog Proc **
BOOL CALLBACK FBMonDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

    switch (message) {
        case WM_INITDIALOG:
        {
            // store this dialog box information in the global PMONITORS structure
            g_PMonitors.PluginMonitor[g_PMonitors.nPlugins].hDlgBox = hDlg;
            g_PMonitors.PluginMonitor[g_PMonitors.nPlugins].iPluginType = FILEBROWSER;
            g_PMonitors.nPlugins++;

            OnFBMDialogInit(hDlg);
            CenterWindow(hDlg);
            return TRUE;
        }


        case WM_COMMAND:
        {
            // handle commands from child controls
            switch (LOWORD(wParam)) {
                case IDB_FBM_CANCEL:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }

            }
            return FALSE;
        }

        case WM_CLIENT_RESPONSE:
        {
            DLGHDR *pDlgHdr = NULL;

            pDlgHdr = (DLGHDR*)GetWindowLong(hDlg, GWL_USERDATA);
            if (pDlgHdr != NULL) {
                switch (((PDATA*)lParam)->iMessage) {
                    case FBM_DRIVES:	// fall through intentional
                    case FBM_DIRS:
                    case FBM_FILES:
                    {
                        SendMessage(pDlgHdr->hAllDlgs[FBM_UI_DLG], WM_CLIENT_RESPONSE, 0, lParam);
                        return TRUE;
                    }
                    default:
                    {
                        if (lParam)
                            free((void*)lParam);
                        break;
                    }

                }
                return FALSE;
            }
            return TRUE;
        }

        case WN_DLG_NOTIFICATION:
        {
            if (wParam == WN_DLG_DISCONNECTED) {
                SetWindowText(hDlg, L"File Browser Monitor: Disconnected");
            }
            return TRUE;
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

}// FBMonDP()

void WINAPI OnFBMDialogInit(HWND hDlg) {
    int i, iRetVal;

    TCITEM tie;

    WCHAR wszDirPath[MAX_PATH + 1] = L"";
    WCHAR wszWndTitle[MAX_WND_TITLE + 1] = L"";

    DLGHDR *pDlgHdr = (DLGHDR*)LocalAlloc(LPTR, sizeof(DLGHDR));

    // Set window title
    wsprintf(wszWndTitle, L"File Browser Monitor: Connected to %s / %s", g_wszCurCliIP, g_wszCurCliNick);
    SetWindowText(hDlg, wszWndTitle);

    // create the FBM directory under Plugins

    // path
    wsprintf(wszDirPath, L"%s\\%s", g_wszCurCliDIR, FP_CLIENT_FBM_DIR);
    iRetVal = CreateDirectory(wszDirPath, NULL);

    memset(pDlgHdr, 0, sizeof(DLGHDR));

    // Save a pointer to the DLGHDR structure. 
    SetWindowLong(hDlg, GWL_USERDATA, (LONG)pDlgHdr);

    // retrieve and save handle of the tab control
    pDlgHdr->hTab = GetDlgItem(hDlg, IDC_FBM_TAB);
    if (pDlgHdr->hTab == NULL) {
        // handle error
        MessageBox(0, L"cannot find tab", 0, 0);
        return;

    }

    // Add a tab for the display of folders and files 
    tie.mask = TCIF_TEXT | TCIF_IMAGE;
    tie.iImage = -1;

    tie.pszText = L"Remote File Browser";
    iRetVal = TabCtrl_InsertItem(pDlgHdr->hTab, 0, &tie);

    // create the dialog box and save the handle in the DLGHDR structure
    pDlgHdr->hAllDlgs[FBM_UI_DLG] = CreateDialog(hMainInstance, MAKEINTRESOURCE(IDD_FBM_UI),
        hDlg, FBMonUserInterfaceDP);

    // check for errors
    for (i = 0; i < 1; ++i) {
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

BOOL CALLBACK FBMonUserInterfaceDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    int iRetVal;

    static int iSelected;
    static BOOL fExpand = TRUE;

    HBRUSH hbrBackground;

    HANDLE hFTThread = NULL;

    HTREEITEM hExpandingNode;
    static HWND hTree, hList;
    static HTREEITEM hExpandedNode;
    LPNMLISTVIEW lpNMListView = NULL;

    static SPMARGS *pSPMArgs = NULL;

    switch (message) {
        case WM_INITDIALOG:
        {
            extern HINSTANCE hMainInstance;

            RECT rcTab = { 0,0,0,0 };
            LVCOLUMN lvColumn = { 0 };

            HBITMAP hBitmap = NULL;
            HIMAGELIST hImageList = NULL;

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

            hFBMUserInterfaceDlg = hDlg;
            hList = GetDlgItem(hDlg, IDC_FBM_UI_LIST);
            hTree = GetDlgItem(hDlg, IDC_FBM_UI_TREE);

            // List view headers
            lvColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;    //type of mask

            // first column
            lvColumn.pszText = L"Name";                      //First Header Text
            lvColumn.cx = 0x80;                                   //width of column
            SendMessage(hList, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn); //Insert the column

            // second column
            lvColumn.pszText = L"Size";
            lvColumn.cx = 0x60;
            SendMessage(hList, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

            // third column
            lvColumn.pszText = L"Type";
            lvColumn.cx = 0x90;
            SendMessage(hList, LVM_INSERTCOLUMN, 2, (LPARAM)&lvColumn);

            // fourth column
            lvColumn.pszText = L"Attributes";
            lvColumn.cx = 0x90;
            SendMessage(hList, LVM_INSERTCOLUMN, 3, (LPARAM)&lvColumn);

            // fifth column
            lvColumn.pszText = L"Last Access";
            lvColumn.cx = 0x90;
            SendMessage(hList, LVM_INSERTCOLUMN, 4, (LPARAM)&lvColumn);

            // tree view image list
            hImageList = ImageList_Create(16,				// cx
                16,				// cy
                ILC_COLOR8,		// type
                2,				// initial number of images
                10);			// maximum number of images
            if (hImageList == NULL) {
                // error
                return TRUE;
            }

            // network drive image
            hBitmap = LoadBitmap(hMainInstance, MAKEINTRESOURCE(IDB_BITMAP4));
            ImageList_Add(hImageList, hBitmap, NULL);
            DeleteObject(hBitmap);

            // harddisk volume image
            hBitmap = LoadBitmap(hMainInstance, MAKEINTRESOURCE(IDB_BITMAP3));
            ImageList_Add(hImageList, hBitmap, NULL);
            DeleteObject(hBitmap);

            // selected folder image
            hBitmap = LoadBitmap(hMainInstance, MAKEINTRESOURCE(IDB_BITMAP1));
            ImageList_Add(hImageList, hBitmap, NULL);
            DeleteObject(hBitmap);

            // unselected folder image
            hBitmap = LoadBitmap(hMainInstance, MAKEINTRESOURCE(IDB_BITMAP2));
            ImageList_Add(hImageList, hBitmap, NULL);
            DeleteObject(hBitmap);

            iRetVal = SendMessage(hTree, TVM_SETIMAGELIST, 0, (LPARAM)hImageList);

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

            __try {
                pPData = (PDATA*)lParam;

                switch (pPData->iMessage) {
                    case FBM_DRIVES:
                    {
                        InsertDrives(hTree, &pPData->PDataType.FBData);

                        // disable the Start button
                        // FALSE indicates that window must be disabled
                        EnableWindow(GetDlgItem(hFBMUserInterfaceDlg, IDB_FBM_UI_START), FALSE);

                        return TRUE;
                    }

                    case FBM_DIRS:
                    {
                        UpdateTree(hTree, hExpandedNode, &pPData->PDataType.FBData);
                        iRetVal = TreeView_EnsureVisible(hTree, TreeView_GetChild(hTree, hExpandedNode));
                        return TRUE;
                    }

                    case FBM_FILES:
                    {
                        UpdateList(hList, &pPData->PDataType.FBData);
                        return TRUE;
                    }
                }
                return TRUE;
            }
            __finally {
                if (pPData) {
                    free(pPData);
                    pPData = NULL;
                }
            }
        }


        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
                case TVN_ITEMEXPANDING:
                {
                    TVITEM tviItem = { 0 };
                    hExpandingNode = NULL;

                    //get handle to expanding/collapsing item
                    hExpandingNode = ((LPNMTREEVIEW)lParam)->itemNew.hItem;

                    if (hExpandingNode == TreeView_GetRoot(hTree))
                        return FALSE;		// FALSE allows expand or collapse

                    // get info about selected item
                    tviItem.mask = TVIF_STATE | TVIF_HANDLE | TVIF_PARAM;
                    tviItem.hItem = hExpandingNode;
                    if ((iRetVal = SendMessage(hTree, TVM_GETITEM, 0, (LPARAM)&tviItem)) == FALSE) {
                        // error
                        return TRUE;
                    }

                    if (((LPNMTREEVIEW)lParam)->action == TVE_EXPAND) {
                        if (hExpandingNode == hExpandedNode)
                            return FALSE;

                        // check the TVIS_EXPANDEDONCE state bit
                        if (tviItem.state & TVIS_EXPANDEDONCE) {
                            return FALSE;
                        }

                        hExpandedNode = hExpandingNode;

                        __try {
                            // allocate memory for SPMARGS structure
                            if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                                MessageBox(hDlg, L"FBM: malloc error: pSPMArgs. Cannot Continue.", L"FBM", MB_ICONINFORMATION);
                                return FALSE;
                            }
                            memset(pSPMArgs, 0, sizeof(SPMARGS));

                            // send command to FB plugin
                            memset(pSPMArgs, 0, sizeof(SPMARGS));
                            pSPMArgs->cmd = SPM_SENDDATA;
                            pSPMArgs->iFromPlugin = FILEBROWSER;
                            pSPMArgs->MNTRData.FBMData.cmd = FBIF_EXPAND;
                            wcscpy_s(pSPMArgs->MNTRData.FBMData.wszPath, MAX_PATH + 1, (WCHAR*)tviItem.lParam);

                            if ((iRetVal = ServerPluginManager(0, pSPMArgs, NULL)) == FALSE) {
                                MessageBox(hDlg, L"SPM returned error while sending command to client", L"File Browser", MB_ICONINFORMATION);
                                return TRUE;
                            }

                            return FALSE;	// FALSE allows expand or collapse
                        }
                        __finally {
                            if (pSPMArgs)
                                free(pSPMArgs);
                        }
                    } else if (((LPNMTREEVIEW)lParam)->action == TVE_COLLAPSE) {
                        return FALSE;

                        HTREEITEM h;
                        HTREEITEM hCollapsingNode;
                        hCollapsingNode = ((LPNMTREEVIEW)lParam)->itemNew.hItem;

                        // free the allocated memory (address is in lParam field)
                        for (h = TreeView_GetChild(hTree, hCollapsingNode);
                            h != NULL;
                            h = TreeView_GetNextSibling(hTree, h)) {
                            tviItem.hItem = h;
                            tviItem.mask = TVIF_PARAM | TVIF_HANDLE;
                            if ((iRetVal = SendMessage(hTree, TVM_GETITEM, 0,
                                (LPARAM)&tviItem)) == FALSE) {
                                // error
                                continue;
                            }

                            free((WCHAR*)tviItem.lParam);
                        }

                        //use TVE_COLLAPSERESET to delete the child nodes
                        iRetVal = SendMessage(hTree, TVM_EXPAND, TVE_COLLAPSERESET | TVE_COLLAPSE,
                            (LPARAM)hCollapsingNode);

                        hExpandedNode = NULL;

                    }

                    return FALSE;
                }


                case TVN_SELCHANGED:
                {
                    TVITEM tviItem = { 0 };
                    HTREEITEM hSelectedNode = NULL;

                    //get handle to selected node
                    hSelectedNode = ((LPNMTREEVIEW)lParam)->itemNew.hItem;

                    if (hSelectedNode == TreeView_GetRoot(hTree))
                        return FALSE;

                    // delete the list view items
                    ListView_DeleteAllItems(hList);

                    // get info about selected item
                    tviItem.mask = TVIF_STATE | TVIF_HANDLE | TVIF_PARAM;
                    tviItem.hItem = hSelectedNode;
                    if ((iRetVal = SendMessage(hTree, TVM_GETITEM, 0, (LPARAM)&tviItem)) == FALSE) {
                        // error
                        return TRUE; // return value ignored
                    }

                    __try {
                        // allocate memory for SPMARGS structure
                        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                            MessageBox(hDlg, L"FBM: malloc error: SPMArgs. Cannot Continue.", L"FBM", MB_ICONINFORMATION);
                            return FALSE;
                        }
                        memset(pSPMArgs, 0, sizeof(SPMARGS));

                        // send command to FB plugin
                        memset(pSPMArgs, 0, sizeof(SPMARGS));
                        pSPMArgs->cmd = SPM_SENDDATA;
                        pSPMArgs->iFromPlugin = FILEBROWSER;
                        pSPMArgs->MNTRData.FBMData.cmd = FBIF_LIST_FILES;
                        wcscpy_s(pSPMArgs->MNTRData.FBMData.wszPath, MAX_PATH + 1, (WCHAR*)tviItem.lParam);

                        ServerPluginManager(0, pSPMArgs, NULL);

                        return FALSE;	// return value ignored
                    }
                    __finally {
                        if (pSPMArgs)
                            free(pSPMArgs);
                    }
                }

                case LVN_ITEMCHANGED:
                {
                    if ((lpNMListView = (LPNMLISTVIEW)lParam) == NULL) {
                        // error
                        return 0;
                    }
                    if (lpNMListView->uNewState & LVIS_SELECTED)
                        if (lpNMListView->iItem != -1)
                            iSelected = lpNMListView->iItem;

                    return 0;	// actually, no return value is required
                }
            }
            return FALSE;
        }


        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                case IDB_FBM_UI_START:
                {
                    // send command to client to get drive list
                    __try {
                        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                            MessageBox(hDlg, L"malloc error: SPMArgs", 0, 0);
                            return FALSE;
                        }
                        memset(pSPMArgs, 0, sizeof(SPMARGS));

                        pSPMArgs->cmd = SPM_SENDDATA;
                        pSPMArgs->iFromPlugin = FILEBROWSER;
                        pSPMArgs->MNTRData.FBMData.cmd = FBIF_GETDRIVES;

                        ServerPluginManager(0, pSPMArgs, NULL);

                        return TRUE;
                    }
                    __finally {
                        if (pSPMArgs)
                            free(pSPMArgs);
                    }

                    return TRUE;
                }

                case IDB_FBM_UI_COPY:
                {
                    TVITEM tvItem = { 0 };
                    LVITEM lvItem = { 0 };
                    HTREEITEM hSel = NULL;
                    WCHAR *pwszFullPath = NULL;
                    WCHAR wszSelFile[MAX_PATH + 1] = L"";

                    if (iSelected < 0)
                        return TRUE;

                    // get the selected filename from the list view
                    lvItem.mask = LVIF_TEXT;
                    lvItem.iItem = iSelected;
                    lvItem.pszText = wszSelFile;
                    lvItem.cchTextMax = MAX_PATH + 1;
                    if ((iRetVal = SendMessage(hList, LVM_GETITEM, 0, (LPARAM)&lvItem)) == FALSE) {
                        // display error
                        return TRUE;
                    }

                    // get the selected directory under which this file exists
                    if ((hSel = TreeView_GetSelection(hTree)) == NULL) {
                        // display error
                        return TRUE;
                    }

                    // get the absolute path of the directory
                    tvItem.mask = TVIF_PARAM;
                    tvItem.hItem = hSel;
                    if ((iRetVal = SendMessage(hTree, TVM_GETITEM, 0, (LPARAM)&tvItem)) == FALSE) {
                        // display error
                        return TRUE;
                    }

                    if ((WCHAR*)tvItem.lParam == NULL)
                        return TRUE;

                    // freeing this memory is the responsibility of the function FBMFileCopy()
                    if ((pwszFullPath = (WCHAR*)malloc(sizeof(WCHAR) * (MAX_PATH + 1))) == NULL) {
                        PrintToStatusWindow(L"FBM: malloc error: pwszFullPath");
                        return TRUE;
                    }

                    // construct the path
                    swprintf_s(pwszFullPath, MAX_PATH + 1, L"%s\\%s", (WCHAR*)tvItem.lParam, wszSelFile);

                    if ((hFTThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)FBMFileCopy,
                        (LPVOID)pwszFullPath, 0, NULL)) == NULL) {
                        // display error
                        MessageBox(hDlg, L"File Copy Thread Creation Error", L"File Copy", MB_ICONINFORMATION);
                        return FALSE;
                    }

                    return TRUE;
                }

                case IDB_FBM_UI_CANCEL_COPY:
                {
                    ServerPluginManager(SPM_FT_TERM, NULL, NULL);
                    return TRUE;
                }
            }
            return FALSE;
        }

        case WM_CHILD_DLG_CLOSE:
        {
            // delete all memory allocated during tree creation
            FreeTreeViewMemory(hTree);
            TreeView_DeleteAllItems(hTree);
            return TRUE;
        }

    }
    return FALSE;

}// FBMonUserInterfaceDP()


BOOL InsertDrives(HWND hTree, const FBDATA *pFBData) {
    extern WCHAR g_wszCurCliIP[];

    int i;

    HTREEITEM hRoot = NULL;
    HTREEITEM hDriveNode = NULL;

    WCHAR *pwszDriveLetter = NULL;

    TVINSERTSTRUCT tviToInsert = { 0 };

    // IP address of the client as root
    tviToInsert.hParent = NULL;
    tviToInsert.hInsertAfter = TVI_ROOT;
    tviToInsert.item.mask = TVIF_TEXT | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tviToInsert.item.iImage = FBM_UI_NETDRIVE_IMG;
    tviToInsert.item.iSelectedImage = FBM_UI_NETDRIVE_IMG;
    tviToInsert.item.cChildren = 1;
    tviToInsert.item.pszText = g_wszCurCliIP;
    tviToInsert.item.cchTextMax = wcsnlen_s(g_wszCurCliIP, IPADDR_LEN + 1);
    if ((hRoot = (HTREEITEM)SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tviToInsert)) == NULL)
        return FALSE;


    // now, the drives
    tviToInsert.hParent = hRoot;
    tviToInsert.hInsertAfter = TVI_LAST;
    tviToInsert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tviToInsert.item.iImage = FBM_UI_DRIVE_IMG;
    tviToInsert.item.iSelectedImage = FBM_UI_DRIVE_IMG;
    tviToInsert.item.cChildren = 1;

    // create nodes for all volumes in the client machine
    for (i = 0; i < pFBData->nPaths; ++i) {
        if ((pwszDriveLetter = (WCHAR*)malloc(sizeof(WCHAR) * (MAX_FILE_NAME + 1))) == NULL)
            return NULL;

        wcscpy_s(pwszDriveLetter, MAX_FILE_NAME + 1, pFBData->FBFileList[i].wszFileName);

        tviToInsert.item.pszText = pwszDriveLetter;
        tviToInsert.item.cchTextMax = wcsnlen_s(pwszDriveLetter, MAX_FILE_NAME + 1);
        tviToInsert.item.lParam = (LPARAM)pwszDriveLetter;
        //WriteToAddressFile(pwszDriveLetter, 0);

        if ((hDriveNode = (HTREEITEM)SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tviToInsert)) == NULL) {
            // error
            if (pwszDriveLetter)
                free(pwszDriveLetter);

            continue;
        }
    }

    return TRUE;
}

BOOL UpdateList(HWND hList, const FBDATA *pFBData) {
    int i, iRetVal, nItems = 0;

    double dFileSize;

    LVITEM lvItem = { 0 };

    DWORD dwFileAttr;

    WCHAR *pwsz = NULL;
    WCHAR wszFileSize[16] = L"";
    WCHAR wszFileType[64] = L"";
    WCHAR wszFileAttr[64] = L"";
    WCHAR wszLastAccess[64] = L"";
    WCHAR wszFileName[MAX_PATH + 1] = L"";

    if (pFBData == NULL)
        return FALSE;

    ListView_DeleteAllItems(hList);

    for (i = 0; i < pFBData->nPaths; ++i) {
        memset(&lvItem, 0, sizeof(LVITEM));

        // file name
        wcscpy_s(wszFileName, MAX_PATH + 1, pFBData->FBFileList[i].wszFileName);
        lvItem.iItem = i;
        lvItem.mask = LVIF_TEXT;
        lvItem.pszText = wszFileName;
        lvItem.cchTextMax = wcsnlen_s(wszFileName, MAX_PATH + 1);
        if ((iRetVal = SendMessage(hList, LVM_INSERTITEM, 0, (LPARAM)&lvItem)) == -1) {
            // error
            iRetVal = GetLastError();
            break;
        }

        // file size
        dFileSize = (double)pFBData->FBFileList[i].dwFileSize;
        if (dFileSize >= GIGABYTE) {
            dFileSize /= GIGABYTE;
            swprintf_s(wszFileSize, 16, L"%.4f GB", dFileSize);
        } else if (dFileSize >= MEGABYTE) {
            dFileSize /= MEGABYTE;
            swprintf_s(wszFileSize, 16, L"%.4f MB", dFileSize);
        } else if (dFileSize >= KILOBYTE) {
            dFileSize /= KILOBYTE;
            swprintf_s(wszFileSize, 16, L"%.4f KB", dFileSize);
        } else
            swprintf_s(wszFileSize, 16, L"%.0f B", dFileSize);

        //wsprintf(wszFileSize, L"%u", pFBData->FBFileList[i].dwFileSize);
        lvItem.iSubItem = 1;
        lvItem.pszText = wszFileSize;
        lvItem.cchTextMax = wcsnlen_s(wszFileSize, 16);
        iRetVal = SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&lvItem);

        // file attributes
        dwFileAttr = pFBData->FBFileList[i].dwFileAttributes;
        wszFileAttr[0] = 0;

        if (dwFileAttr & FILE_ATTRIBUTE_NORMAL)
            wcscpy_s(wszFileAttr, 64, L"File");
        if (dwFileAttr & FILE_ATTRIBUTE_HIDDEN) {
            if (wcsnlen_s(wszFileAttr, 64) == 0)
                wcscat_s(wszFileAttr, 64, L"Hidden");
            else
                wcscat_s(wszFileAttr, 64, L", Hidden");
        }
        if (dwFileAttr & FILE_ATTRIBUTE_SYSTEM) {
            if (wcsnlen_s(wszFileAttr, 64) == 0)
                wcscat_s(wszFileAttr, 64, L"System");
            else
                wcscat_s(wszFileAttr, 64, L", System");
        }
        if (dwFileAttr & FILE_ATTRIBUTE_ENCRYPTED) {
            if (wcsnlen_s(wszFileAttr, 64) == 0)
                wcscat_s(wszFileAttr, 64, L"Encrypted");
            else
                wcscat_s(wszFileAttr, 64, L", Encrypted");
        }
        if (dwFileAttr & FILE_ATTRIBUTE_READONLY) {
            if (wcsnlen_s(wszFileAttr, 64) == 0)
                wcscat_s(wszFileAttr, 64, L"ReadOnly");
            else
                wcscat_s(wszFileAttr, 64, L", ReadOnly");
        }

        // file type
        _wcslwr_s(wszFileName, MAX_PATH + 1);
        wszFileType[0] = 0;

        // Note: filetype scanning code is far from perfect!!

        // scan the file extension
        if (((pwsz = wcsstr(wszFileName, L".c")) && (*(pwsz + 2) == 0)) ||
            (pwsz = wcsstr(wszFileName, L".cpp")))
            wcscpy_s(wszFileType, L"C/C++ Source");
        else if (((pwsz = wcsstr(wszFileName, L".h")) && (*(pwsz + 2) == 0)) ||
            (pwsz = wcsstr(wszFileName, L".hxx")))
            wcscpy_s(wszFileType, L"C/C++ Header");
        else if ((pwsz = wcsstr(wszFileName, L".vbs")))
            wcscpy_s(wszFileType, L"VBScript Script File");
        else if ((pwsz = wcsstr(wszFileName, L".js")))
            wcscpy_s(wszFileType, L"JavaScript Script File");
        else if ((pwsz = wcsstr(wszFileName, L".bat")))
            wcscpy_s(wszFileType, L"MS-DOS Batch File");
        else if ((pwsz = wcsstr(wszFileName, L".com")))
            wcscpy_s(wszFileType, L"MS-DOS Application");
        else if ((pwsz = wcsstr(wszFileName, L".sys")))
            wcscpy_s(wszFileType, L"System File");
        else if ((pwsz = wcsstr(wszFileName, L".drv")))
            wcscpy_s(wszFileType, L"Device Driver");
        else if ((pwsz = wcsstr(wszFileName, L".dll")))
            wcscpy_s(wszFileType, L"Application Extension");
        else if ((pwsz = wcsstr(wszFileName, L".ocx")))
            wcscpy_s(wszFileType, L"ActiveX Control");
        else if ((pwsz = wcsstr(wszFileName, L".msc")))
            wcscpy_s(wszFileType, L"MS Common Console Document");
        else if ((pwsz = wcsstr(wszFileName, L".wsc")))
            wcscpy_s(wszFileType, L"Windows Script Component");
        else if ((pwsz = wcsstr(wszFileName, L".exe")))
            wcscpy_s(wszFileType, L"Application");

        else if ((pwsz = wcsstr(wszFileName, L".txt")) || (pwsz = wcsstr(wszFileName, L".log")))
            wcscpy_s(wszFileType, L"Text Document");
        else if ((pwsz = wcsstr(wszFileName, L".doc")) || (pwsz = wcsstr(wszFileName, L".docx")))
            wcscpy_s(wszFileType, L"MS Office Word Document");
        else if ((pwsz = wcsstr(wszFileName, L".ppt")) || (pwsz = wcsstr(wszFileName, L".pptx")))
            wcscpy_s(wszFileType, L"MS Office PowerPoint Presentation");
        else if ((pwsz = wcsstr(wszFileName, L".xls")))//|| (pwsz = wcsstr(wszFileName, L".xls")) )
            wcscpy_s(wszFileType, L"MS Office Excel Spreadsheet");
        else if ((pwsz = wcsstr(wszFileName, L".odt")))
            wcscpy_s(wszFileType, L"OpenOffice Word Document");
        else if ((pwsz = wcsstr(wszFileName, L".ods")))
            wcscpy_s(wszFileType, L"OpenOffice Spreadsheet");
        else if ((pwsz = wcsstr(wszFileName, L".odp")))
            wcscpy_s(wszFileType, L"OpenOffice Presentation");
        else if ((pwsz = wcsstr(wszFileName, L".pdf")))
            wcscpy_s(wszFileType, L"PDF Document");

        else if ((pwsz = wcsstr(wszFileName, L".htm")) || (pwsz = wcsstr(wszFileName, L".html")))
            wcscpy_s(wszFileType, L"HTML Document");
        else if ((pwsz = wcsstr(wszFileName, L".chm")))
            wcscpy_s(wszFileType, L"Compiled HTML Help File");
        else if ((pwsz = wcsstr(wszFileName, L".hlp")))
            wcscpy_s(wszFileType, L"Help File");

        else if ((pwsz = wcsstr(wszFileName, L".mp3")))
            wcscpy_s(wszFileType, L"MP3 Format Sound");
        else if ((pwsz = wcsstr(wszFileName, L".wma")))
            wcscpy_s(wszFileType, L"Windows Media Audio File");
        else if ((pwsz = wcsstr(wszFileName, L".wmv")))
            wcscpy_s(wszFileType, L"Windows Media Video File");
        else if ((pwsz = wcsstr(wszFileName, L".vob")))
            wcscpy_s(wszFileType, L"DVD Video");
        else if ((pwsz = wcsstr(wszFileName, L".flv")))
            wcscpy_s(wszFileType, L"FLV File");
        else if ((pwsz = wcsstr(wszFileName, L".avi")))
            wcscpy_s(wszFileType, L"AVI File");
        else if ((pwsz = wcsstr(wszFileName, L".3gp")))
            wcscpy_s(wszFileType, L"3GP Mobile Video");
        else if ((pwsz = wcsstr(wszFileName, L".mpg")) || (pwsz = wcsstr(wszFileName, L".mpeg")))
            wcscpy_s(wszFileType, L"MPEG Video");

        else if ((pwsz = wcsstr(wszFileName, L".dat")))
            wcscpy_s(wszFileType, L"DAT File");

        else if ((pwsz = wcsstr(wszFileName, L".nrg")))
            wcscpy_s(wszFileType, L"Nero Image");
        else if ((pwsz = wcsstr(wszFileName, L".iso")))
            wcscpy_s(wszFileType, L"ISO Image");

        else if ((pwsz = wcsstr(wszFileName, L".zip")))
            wcscpy_s(wszFileType, L"ZIP Archive");
        else if ((pwsz = wcsstr(wszFileName, L".rar")))
            wcscpy_s(wszFileType, L"WINRAR RAR Archive");

        else if ((pwsz = wcsstr(wszFileName, L".bmp")))
            wcscpy_s(wszFileType, L"Bitmap Image");
        else if ((pwsz = wcsstr(wszFileName, L".jpg")) || (pwsz = wcsstr(wszFileName, L".jpeg")))
            wcscpy_s(wszFileType, L"JPEG Image");
        else if ((pwsz = wcsstr(wszFileName, L".png")))
            wcscpy_s(wszFileType, L"PNG Image");
        else if ((pwsz = wcsstr(wszFileName, L".gif")))
            wcscpy_s(wszFileType, L"GIF Image");
        else if ((pwsz = wcsstr(wszFileName, L".ico")))
            wcscpy_s(wszFileType, L"Icon File");

        else if ((pwsz = wcsstr(wszFileName, L".torrent")))
            wcscpy_s(wszFileType, L"P2P Torrent File");

        else if ((pwsz = wcsstr(wszFileName, L".ini")))
            wcscpy_s(wszFileType, L"Configuration Settings");
        else if ((pwsz = wcsstr(wszFileName, L".db")))
            wcscpy_s(wszFileType, L"Database File");

        else {
            if ((pwsz = wcsrchr(wszFileName, L'.')) == NULL) {
                wcscpy_s(wszFileType, L"File");
            } else {
                // skip over the '.'
                pwsz++;
                wcscpy_s(wszFileType, pwsz);
                _wcsupr_s(wszFileType, 64);
                wcscat_s(wszFileType, 64, L" File");
            }
        }

        // set the file type in list view
        lvItem.iSubItem = 2;
        lvItem.pszText = wszFileType;
        lvItem.cchTextMax = wcsnlen_s(wszFileType, 64);
        iRetVal = SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&lvItem);

        // set the file attributes in list view
        lvItem.iSubItem = 3;
        lvItem.pszText = wszFileAttr;
        lvItem.cchTextMax = wcsnlen_s(wszFileAttr, 64);
        iRetVal = SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&lvItem);

        // last access time
        swprintf_s(wszLastAccess, 64, L"%d/%d/%d %02d:%02d", pFBData->FBFileList[i].stLastAccess.wDay, pFBData->FBFileList[i].stLastAccess.wMonth,
            pFBData->FBFileList[i].stLastAccess.wYear, pFBData->FBFileList[i].stLastAccess.wHour, pFBData->FBFileList[i].stLastAccess.wMinute);
        lvItem.iSubItem = 4;
        lvItem.pszText = wszLastAccess;
        lvItem.cchTextMax = wcsnlen_s(wszLastAccess, 64);
        iRetVal = SendMessage(hList, LVM_SETITEM, 0, (LPARAM)&lvItem);
    }

    return TRUE;
}

BOOL UpdateTree(HWND hTree, HTREEITEM hParentNode, const FBDATA *pFBData) {
    int i, iRetVal;
    DWORD dwDirCheck;
    BOOL fDirEmpty = TRUE;

    HTREEITEM hChild;
    TVITEM tviItem = { 0 };

    if (hParentNode == NULL)
        return FALSE;

    // get absolute path of parent node from the lParam member
    tviItem.hItem = hParentNode;
    tviItem.mask = TVIF_PARAM;

    if ((iRetVal = SendMessage(hTree, TVM_GETITEM, 0, (LPARAM)&tviItem)) == FALSE)
        return FALSE;

    for (i = 0; i < pFBData->nPaths; ++i) {
        // check whether it is a directory or a file
        dwDirCheck = pFBData->FBFileList[i].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

        if (dwDirCheck) {
            // do not display . and ..
            if ((wcscmp(pFBData->FBFileList[i].wszFileName, L".") == 0) ||
                (wcscmp(pFBData->FBFileList[i].wszFileName, L"..") == 0)) {
                continue;
            }

            fDirEmpty = FALSE;

            if ((hChild = CreateChild(hTree, hParentNode, pFBData->FBFileList[i].wszFileName,
                (WCHAR*)tviItem.lParam)) == NULL) {
                // error
                return FALSE;
            }
        }
    }

    if (fDirEmpty == TRUE) {
        // remove the '+' symbol
        memset(&tviItem, 0, sizeof(TVITEM));
        tviItem.mask = TVIF_CHILDREN | TVIF_HANDLE;
        tviItem.cChildren = 0;
        tviItem.hItem = hParentNode;

        SendMessage(hTree, TVM_SETITEM, 0, (LPARAM)&tviItem);
    }

    return TRUE;
}

HTREEITEM CreateChild(HWND hTree, HTREEITEM hParent, const WCHAR *pwszPath,
    const WCHAR *pwszParentPath) {
    HTREEITEM h = NULL;
    WCHAR *pwszFullPath = NULL;
    WCHAR wszNodeText[MAX_PATH + 1] = L"";
    TVINSERTSTRUCT tviToInsert = { 0 };

    wcscpy_s(wszNodeText, MAX_PATH + 1, pwszPath);

    //construct full path of the child
    if ((pwszFullPath = (WCHAR*)malloc(sizeof(WCHAR) * (MAX_PATH + 1))) == NULL)
        return NULL;

    lstrcpy(pwszFullPath, pwszParentPath);
    lstrcat(pwszFullPath, L"\\");
    lstrcat(pwszFullPath, pwszPath);

    tviToInsert.hParent = hParent;
    tviToInsert.hInsertAfter = TVI_LAST;
    tviToInsert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_CHILDREN | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tviToInsert.item.iImage = FBM_UI_UNSEL_IMG;
    tviToInsert.item.iSelectedImage = FBM_UI_SEL_IMG;
    tviToInsert.item.cChildren = 1;
    tviToInsert.item.pszText = wszNodeText;
    tviToInsert.item.cchTextMax = lstrlen(wszNodeText) + 1;
    tviToInsert.item.lParam = (LPARAM)pwszFullPath;			//store fullpath in lParam
    //WriteToAddressFile(pwszFullPath, 0);

    //send message TVM_INSERTITEM
    if ((h = (HTREEITEM)SendMessage(hTree, TVM_INSERTITEM, 0, (LPARAM)&tviToInsert)) == NULL) {
        free(pwszFullPath);
        return NULL;
    }
    return h;

}//CreateChild()

DWORD WINAPI FBMFileCopy(LPVOID lpArgs) {
    extern BOOL fFTSuccess;
    extern int iFTStatus;
    extern HCURSOR hCursorWait;
    extern WCHAR g_wszCurCliDIR[];

    int iRetVal;
    DWORD dwEventRetVal;

    HWND hStaticText = NULL;
    HANDLE hFTCompleteEvent = NULL;

    WCHAR wszDestPath[MAX_PATH + 1] = L"";
    WCHAR wszLogFileMsg[MAX_LOGFILE_LINE + 1] = L"";

    WCHAR *pwszSrcFilePath = NULL;
    WCHAR *pwszPrevToken = NULL, *pwszCurToken = NULL;

    if ((pwszSrcFilePath = (WCHAR*)lpArgs) == NULL) {
        // display error
        return 2;
    }

    // create an event which will be used to get notification of file transfer completion
    hFTCompleteEvent = CreateEvent(NULL,	// default security attributes
        TRUE,	// manual reset using ResetEvent()
        FALSE,	// non-signaled initial state
        EVENT_FT_COMPLETE);	// name of the event object

    if (hFTCompleteEvent == NULL) {
        iRetVal = GetLastError();
        return 2;
    }

    hStaticText = GetDlgItem(hFBMUserInterfaceDlg, IDC_FBM_UI_STATIC);

    // disable Copy File button
    EnableWindow(GetDlgItem(hFBMUserInterfaceDlg, IDB_FBM_UI_COPY), FALSE);

    // Enable Cancel Copy
    EnableWindow(GetDlgItem(hFBMUserInterfaceDlg, IDB_FBM_UI_CANCEL_COPY), TRUE);

    // show the static text
    ShowWindow(hStaticText, SW_SHOW);

    // get the current file's file name from the absolute path.
    pwszPrevToken = pwszSrcFilePath;
    while ((pwszCurToken = wcsstr(pwszPrevToken, L"\\")) != NULL) {
        pwszPrevToken = pwszCurToken;

        //skip over '\'
        ++pwszPrevToken;
    }

    // construct the absolute path for the destination file (local path)
    swprintf_s(wszDestPath, MAX_PATH + 1, L"%s\\%s\\%s", g_wszCurCliDIR, FP_CLIENT_FBM_DIR, pwszPrevToken);

    SetWindowText(hStaticText, L"Copying File...");

    // set fFTDeleteFile to not delete the log file on the client machine after successful download
    fFTDeleteFile = FALSE;
    fFTSuccess = FALSE;

    // call GetFile()
    if ((iRetVal = GetFile(pwszSrcFilePath, wszDestPath)) == FALSE) {
        MessageBox(hFBMUserInterfaceDlg, L"Unable to copy file", L"Copy File", MB_ICONINFORMATION);
        return 2;

    }

    // wait for notification that the file has been downloaded
    dwEventRetVal = WaitForSingleObject(hFTCompleteEvent, INFINITE);
    if (dwEventRetVal == WAIT_OBJECT_0)
        ResetEvent(hFTCompleteEvent);

    SetWindowText(hStaticText, L"File Copy Completed");

    if (fFTSuccess == FALSE) {
        swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1,
            L"There was an error while downloading %s. See the log file for more details.", pwszPrevToken);
        MessageBox(hFBMUserInterfaceDlg, wszLogFileMsg, L"Retrieve Files", MB_ICONINFORMATION);

        switch (iFTStatus) {
            case FT_ERROR_FILE_CORRUPTED:
            {
                swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. File Corrupted.", pwszPrevToken);
                break;
            }

            case FT_ERROR_CONN_FAIL:
            {
                swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Error downloading file: %s. Connection failed.", pwszPrevToken);
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

        WriteToLogFile(FP_SERVER_LOGFILE, L"FBMonDP", L"FBMFileCopy", wszLogFileMsg, 0);
    } else {
        MessageBox(hFBMUserInterfaceDlg, L"File Copy Complete", L"Copy File", MB_ICONINFORMATION);

        swprintf_s(wszLogFileMsg, MAX_LOGFILE_LINE + 1, L"Downloaded file: %s", pwszPrevToken);
        WriteToLogFile(FP_SERVER_LOGFILE, L"FBMonDP", L"FBMFileCopy", wszLogFileMsg, 0);
    }

    // Re-enable the Copy File button
    EnableWindow(GetDlgItem(hFBMUserInterfaceDlg, IDB_FBM_UI_COPY), TRUE);

    // disable Cancel Copy
    EnableWindow(GetDlgItem(hFBMUserInterfaceDlg, IDB_FBM_UI_CANCEL_COPY), FALSE);

    // hide the static text
    ShowWindow(hStaticText, SW_HIDE);

    if (pwszSrcFilePath)
        free(pwszSrcFilePath);

    return TRUE;
}
