
// * ServerRM project *
// MonitorDP.cpp: Contains all Dialog procks for menu items under 'Monitor'

#include "ServerInclude.h"

// Global variables

// Scan IPs
int nActiveClients = NULL;
CLIENTCONF *pccScanResult = NULL;

BOOL CALLBACK ConnectToDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    LVCOLUMN    LvCol;
    LVITEM      lviSelectedItem;
    static HWND hClientList;

    int iSel = -1;
    static int iSelected;
    WCHAR   wszSelIP[IPADDR_LEN + 1];
    WCHAR   wszSelNick[MAX_NICK_NAME + 1];

    SPMARGS *pSPMArgs = NULL;

    int i, iRetVal;
    int iCliDBFile = 0;
    CLIENTDB ClientsDB;

    switch (message) {

        case WM_INITDIALOG:
        {
            CenterWindow(hDlg);

            // populate the list
            hClientList = GetDlgItem(hDlg, IDC_CONNECT_TO_LIST);

            // Set up list view column headers
            memset(&LvCol, 0, sizeof(LvCol));                // init to 0
            LvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;    // type of mask

            LvCol.pszText = L"IP Address";							 // header Text
            LvCol.cx = 0x80;										 // width of column
            SendMessage(hClientList, LVM_INSERTCOLUMN, 0, (LPARAM)&LvCol); // Insert the column

            // subsequent columns
            LvCol.pszText = L"Nick Name";
            LvCol.cx = 0x80;
            SendMessage(hClientList, LVM_INSERTCOLUMN, 1, (LPARAM)&LvCol);

            // setup to display text under the columns
            memset(&lviSelectedItem, 0, sizeof(lviSelectedItem));	//init members to 0

            // setting properties Of members:
            lviSelectedItem.mask = LVIF_TEXT;   //Text Style
            lviSelectedItem.cchTextMax = 256;   //Max size of text

            // read client info from file and display in the list view control
            memset(&ClientsDB, 0, sizeof(ClientsDB));
            if ((iRetVal = ReadClientsDBFile(&ClientsDB)) == FALSE) {
                MessageBox(hDlg, L"Error reading clients database file.", L"Add Client", MB_OK);
                return TRUE;
            }
            for (i = 0; i < ClientsDB.nClients; ++i) {
                // IPaddress
                lviSelectedItem.iItem = i;
                lviSelectedItem.iSubItem = 0;
                lviSelectedItem.pszText = ClientsDB.ClientInfo[i].wszIP;

                SendMessage(hClientList, LVM_INSERTITEM, 0, (LPARAM)&lviSelectedItem);

                // Nick name
                lviSelectedItem.iSubItem = 1;
                lviSelectedItem.pszText = ClientsDB.ClientInfo[i].wszName;

                SendMessage(hClientList, LVM_SETITEM, 0, (LPARAM)&lviSelectedItem);
            }


            return TRUE;
        }


        case WM_COMMAND:
        {
            // handle command sent from child window controls
            switch (LOWORD(wParam)) {

                case IDB_CONNECT_TO:
                {
                    // retrieve the selection and call SPM to establish connection
                    memset(&lviSelectedItem, 0, sizeof(lviSelectedItem));
                    lviSelectedItem.iItem = iSelected;
                    lviSelectedItem.mask = LVIF_TEXT;
                    lviSelectedItem.pszText = wszSelIP;
                    lviSelectedItem.cchTextMax = IPADDR_LEN + 1;

                    iRetVal = ListView_GetItem(hClientList, &lviSelectedItem);

                    // get the nick name from the selected row
                    lviSelectedItem.iItem = iSelected;
                    lviSelectedItem.iSubItem = 1;
                    lviSelectedItem.pszText = wszSelNick;
                    lviSelectedItem.cchTextMax = MAX_NICK_NAME + 1;

                    iRetVal = ListView_GetItem(hClientList, &lviSelectedItem);

                    // fill the SPMARGS structure and call ServerPluginManager()
                    // to connect to the remote machine.
                    __try {
                        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL)
                            return TRUE;

                        pSPMArgs->cmd = SPM_STARTSESSION;
                        pSPMArgs->iFromPlugin = ADMINGUI;
                        lstrcpy(pSPMArgs->wszCliIP, wszSelIP);
                        lstrcpy(pSPMArgs->wszCliNick, wszSelNick);

                        iRetVal = ServerPluginManager(NULL, pSPMArgs, NULL);

                        SendMessage(hDlg, WM_CLOSE, 0, 0);
                        return TRUE;
                    }
                    __finally {
                        if (pSPMArgs) {
                            free(pSPMArgs);
                            pSPMArgs = NULL;
                        }
                    }
                }

                case IDB_CONNECT_CANCEL:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }

            }// switch(LOWORD(wParam)
            return FALSE;
        }

        case WM_NOTIFY:
        {
            // handle notifications from child controls
            switch (LOWORD(wParam)) {
                case IDC_CONNECT_TO_LIST:
                {
                    if (((LPNMHDR)lParam)->code == NM_CLICK) {
                        iSel = SendMessage(hClientList, LVM_GETNEXTITEM,
                            -1, LVNI_FOCUSED);					// return item selected

                        if (iSel != -1)
                            iSelected = iSel;
                    } else if (((LPNMHDR)lParam)->code == LVN_ITEMCHANGED) {
                        iSelected = ((LPNMITEMACTIVATE)lParam)->iItem;
                    }

                    return TRUE;
                }
            }
            return FALSE;
        }

        case WM_CLOSE:
        {
            EndDialog(hDlg, 0);
            return TRUE;
        }

    }// switch(message)
    return FALSE;

}// ConnectToDP()

BOOL CALLBACK ScanIPAddrDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    extern HWND hMainWnd;

    int iRetVal;

    switch (message) {
        case WM_INITDIALOG:
        {
            SendMessage(hDlg, DM_SETDEFID, (WPARAM)IDC_SCAN_IP_ADDR_FROM, (LPARAM)NULL);
            CenterWindow(hDlg);
            return TRUE;
        }

        case WM_COMMAND:
        {
            // handle commands sent from child window controls
            switch (LOWORD(wParam)) {
                case IDB_SCAN_IP_ADDR_SCAN:
                {
                    // for IP address retrieval
                    DWORD dwEnteredIP;
                    SCAN_IPADDR_ARGS *pScanIPArgs = NULL;
                    BYTE bFirst, bSecond, bThird, bFourth;

                    // freeing this memory is the responsibility of the ScanIPAddresses() function
                    if ((pScanIPArgs = (SCAN_IPADDR_ARGS*)malloc(sizeof(SCAN_IPADDR_ARGS))) == NULL)
                        return TRUE;

                    // first, the from IP address
                    iRetVal = SendDlgItemMessage(hDlg, IDC_SCAN_IP_ADDR_FROM, IPM_GETADDRESS,
                        0, (LPARAM)(LPDWORD)&dwEnteredIP);

                    // unpack the IP address
                    bFirst = FIRST_IPADDRESS(dwEnteredIP);
                    bSecond = SECOND_IPADDRESS(dwEnteredIP);
                    bThird = THIRD_IPADDRESS(dwEnteredIP);
                    bFourth = FOURTH_IPADDRESS(dwEnteredIP);

                    swprintf_s(pScanIPArgs->wszFromIP, IPADDR_LEN + 1, L"%d.%d.%d.%d", bFirst, bSecond, bThird, bFourth);

                    // now, the to IP address
                    iRetVal = SendDlgItemMessage(hDlg, IDC_SCAN_IP_ADDR_TO, IPM_GETADDRESS,
                        0, (LPARAM)(LPDWORD)&dwEnteredIP);

                    // unpack the IP address
                    bFirst = FIRST_IPADDRESS(dwEnteredIP);
                    bSecond = SECOND_IPADDRESS(dwEnteredIP);
                    bThird = THIRD_IPADDRESS(dwEnteredIP);
                    bFourth = FOURTH_IPADDRESS(dwEnteredIP);
                    swprintf_s(pScanIPArgs->wszToIP, IPADDR_LEN + 1, L"%d.%d.%d.%d", bFirst, bSecond, bThird, bFourth);

                    HANDLE hScanCompleteEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_IP_SCAN_COMPLETE);
                    if (hScanCompleteEvent == NULL) {
                        iRetVal = GetLastError();
                        WriteToLogFile(FP_SERVER_LOGFILE, L"MonitorDP", L"ScanIPAddrDP", L"ScanCompleteEvent handle creation error",
                            iRetVal);
                        return TRUE;
                    }

                    HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ScanIPAddresses, pScanIPArgs,
                        0, NULL);
                    if (hThread == NULL) {
                        // error
                        return TRUE;
                    }

                    //SendMessage(hDlg, WM_CLOSE, 0, 0);
                    EndDialog(hDlg, IDB_SCAN_IP_ADDR_SCAN);
                    return TRUE;
                }

                case IDB_SCAN_IP_ADDR_RESET:
                {
                    // clear IP address control
                    SendDlgItemMessage(hDlg, IDC_SCAN_IP_ADDR_FROM, IPM_CLEARADDRESS, 0, 0);
                    SendDlgItemMessage(hDlg, IDC_SCAN_IP_ADDR_TO, IPM_CLEARADDRESS, 0, 0);
                    return TRUE;
                }

                case IDB_SCAN_IP_ADDR_CANCEL:
                {
                    //SendMessage(hDlg, WM_CLOSE, 0, 0);
                    EndDialog(hDlg, IDCANCEL);
                    return TRUE;
                }

            }
            return FALSE;
        }

        case WM_CLOSE:
        {
            EndDialog(hDlg, 0);
            return TRUE;
        }


    }
    return FALSE;
}

BOOL CALLBACK ScanResultsDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    int i, j, iRetVal;
    static int iSelected;

    LVITEM lvItem = { 0 };

    static HWND hActiveClientsList = NULL;

    static SCANRESULTS *pScanResults = NULL;

    // add client from results
    WCHAR wszIP[IPADDR_LEN + 1];
    SPMARGS *pSPMArgs = NULL;
    CLIENTCONF *pNewClientConf = NULL;
    CLIENTINFO *pNewClientInfo = NULL;

    switch (message) {
        case WM_INITDIALOG:
        {
            LVCOLUMN lvColumn;

            CenterWindow(hDlg);

            // get list view handle
            hActiveClientsList = GetDlgItem(hDlg, IDC_SCAN_IP_RESULT_LIST);

            // Set up list view column headers
            memset(&lvColumn, 0, sizeof(lvColumn));
            lvColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

            lvColumn.pszText = L"IP Address";
            lvColumn.cx = 0x50;
            SendMessage(hActiveClientsList, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

            lvColumn.pszText = L"Plugin1";
            lvColumn.cx = 0x70;
            SendMessage(hActiveClientsList, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);
            lvColumn.pszText = L"Plugin2";
            lvColumn.cx = 0x70;

            SendMessage(hActiveClientsList, LVM_INSERTCOLUMN, 2, (LPARAM)&lvColumn);
            lvColumn.pszText = L"Plugin3";
            lvColumn.cx = 0x70;

            SendMessage(hActiveClientsList, LVM_INSERTCOLUMN, 3, (LPARAM)&lvColumn);
            lvColumn.pszText = L"Plugin4";
            lvColumn.cx = 0x70;
            iRetVal = SendMessage(hActiveClientsList, LVM_INSERTCOLUMN, 4, (LPARAM)&lvColumn);

            pScanResults = (SCANRESULTS*)lParam;

            PrintToStatusBar(L"Scan Complete", 1);

            // add active clients to list
            for (i = 0; i < pScanResults->nActive; ++i) {
                lvItem.mask = LVIF_TEXT | LVIF_PARAM;
                lvItem.iItem = i;
                lvItem.iSubItem = 0;
                lvItem.pszText = pScanResults->ccClientConf[i].wszIP;
                lvItem.cchTextMax = wcsnlen_s(pScanResults->ccClientConf[i].wszIP, IPADDR_LEN + 1) + 1;
                lvItem.lParam = (LPARAM)&pScanResults->ccClientConf[i];
                if ((iRetVal = SendMessage(hActiveClientsList, LVM_INSERTITEM, 0, (LPARAM)&lvItem)) == -1) {
                    iRetVal = GetLastError();
                    WriteToLogFile(FP_SERVER_LOGFILE, L"MonitorDP", L"ScanResultsDP", L"Error inserting list view item",
                        iRetVal);
                }

                for (j = 0; j < pScanResults->ccClientConf[i].nPlugins; ++j) {
                    lvItem.mask = LVIF_TEXT;
                    lvItem.iSubItem = j + 1;
                    lvItem.pszText = pScanResults->ccClientConf[i].PluginInfo[j].wszName;
                    lvItem.cchTextMax = wcsnlen_s(pScanResults->ccClientConf[i].PluginInfo[j].wszName, MAX_FILE_NAME + 1);
                    if ((iRetVal = SendMessage(hActiveClientsList, LVM_SETITEM, 0, (LPARAM)&lvItem)) == FALSE) {
                        iRetVal = GetLastError();
                        WriteToLogFile(FP_SERVER_LOGFILE, L"MonitorDP", L"ScanResultsDP", L"Error LVM_SETITEM",
                            iRetVal);
                    }
                }
            }

            return TRUE;
        }

        case WM_COMMAND:
        {
            // handle commands sent from child window controls
            switch (LOWORD(wParam)) {
                case IDB_SCAN_IP_RESULT_ADD:
                {
                    // get the selected item from list view
                    if (iSelected == -1)
                        return TRUE;

                    lvItem.iItem = iSelected;
                    lvItem.mask = LVIF_TEXT | LVIF_PARAM;
                    lvItem.pszText = wszIP;
                    lvItem.cchTextMax = IPADDR_LEN + 1;
                    if ((iRetVal = SendMessage(hActiveClientsList, LVM_GETITEM, 0, (LPARAM)&lvItem)) == FALSE) {
                        MessageBox(hDlg, L"Error retrieving selection", L"Scan Results", MB_ICONINFORMATION);
                        return TRUE;
                    }
                    __try {
                        if ((pNewClientInfo = (CLIENTINFO*)malloc(sizeof(CLIENTINFO))) == NULL) {
                            WriteToLogFile(FP_SERVER_LOGFILE, L"MonitorDP", L"ScanResultsDP", L"malloc() error: pNewClientInfo",
                                GetLastError());
                            return TRUE;
                        }
                        memset(pNewClientInfo, 0, sizeof(CLIENTINFO));

                        pNewClientConf = (CLIENTCONF*)lvItem.lParam;

                        // fill the new CLIENTINFO structure
                        pNewClientInfo->nPlugins = pNewClientConf->nPlugins;
                        memcpy(&pNewClientInfo->PluginInfo, &pNewClientConf->PluginInfo, sizeof(PLUGININFO));
                        wcscpy_s(pNewClientInfo->wszIP, IPADDR_LEN + 1, wszIP);
                        wcscpy_s(pNewClientInfo->wszName, MAX_NICK_NAME + 1, L"IP Scan");

                        if (AddClientToDBFile(pNewClientInfo) == FALSE) {
                            MessageBox(hDlg, L"Error adding client to database", L"Scan Results", MB_ICONINFORMATION);
                            return TRUE;
                        }
                        MessageBox(hDlg, L"Addition Successful", L"Scan Results", MB_ICONINFORMATION);
                        return TRUE;
                    }
                    __finally {
                        if (pNewClientInfo)
                            free(pNewClientInfo);
                    }
                }

                case IDB_SCAN_IP_RESULT_CONNECT:
                {
                    // get the selected item from list view
                    if (iSelected == -1)
                        return TRUE;

                    lvItem.iItem = iSelected;
                    lvItem.mask = LVIF_TEXT | LVIF_PARAM;
                    lvItem.pszText = wszIP;
                    lvItem.cchTextMax = IPADDR_LEN + 1;
                    if ((iRetVal = SendMessage(hActiveClientsList, LVM_GETITEM, 0, (LPARAM)&lvItem)) == FALSE) {
                        MessageBox(hDlg, L"Error retrieving selection", L"Scan Results", MB_ICONINFORMATION);
                        return TRUE;
                    }

                    __try {
                        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
                            iRetVal = GetLastError();
                            WriteToLogFile(FP_SERVER_LOGFILE, L"MonitorDP", L"ScanResultsDP", L"malloc() error: pSPMArgs",
                                iRetVal);
                            return TRUE;
                        }
                        memset(pSPMArgs, 0, sizeof(SPMARGS));

                        pSPMArgs->cmd = SPM_STARTSESSION;
                        pSPMArgs->iFromPlugin = ADMINGUI;
                        wcscpy_s(pSPMArgs->wszCliIP, IPADDR_LEN + 1, wszIP);

                        if ((iRetVal = ServerPluginManager(0, pSPMArgs, NULL)) == FALSE) {
                            MessageBox(hDlg, L"Error calling SPM", L"Scan Results", MB_ICONEXCLAMATION);
                            return TRUE;
                        }

                        SendMessage(hDlg, WM_CLOSE, 0, 0);
                        return TRUE;
                    }
                    __finally {
                        if (pSPMArgs)
                            free(pSPMArgs);
                    }

                    return TRUE;
                }

                case IDB_SCAN_IP_RESULT_CANCEL:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }

            }
            return FALSE;

        }// case WM_COMMAND

        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
                case NM_CLICK:
                {
                    // check whether the click was inside the list view
                    if (((LPNMHDR)lParam)->hwndFrom == hActiveClientsList &&
                        ((LPNMITEMACTIVATE)lParam)->iItem != -1) {
                        iSelected = ((LPNMITEMACTIVATE)lParam)->iItem;
                    }
                    return TRUE;
                }
            }
            return FALSE;
        }

        case WM_CLOSE:
        {
            PrintToStatusBar(L"", 1);

            // free the ScanResults memory
            if (pScanResults)
                free(pScanResults);

            EndDialog(hDlg, 0);
            return TRUE;
        }

    }
    return FALSE;
}

BOOL CALLBACK AboutBoxDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hPicCtrl;
    static HANDLE hNewBitmapImage;
    int iRetVal;

    switch (message) {
        case WM_INITDIALOG:
        {
            hPicCtrl = GetDlgItem(hDlg, IDC_ABOUT_LOGO);

            hNewBitmapImage = LoadImage(NULL,
                L"bitmap5.bmp",
                IMAGE_BITMAP,
                0,
                0,
                LR_LOADFROMFILE);
            if (hNewBitmapImage == NULL) {
                iRetVal = GetLastError();
                return FALSE;
            }

            iRetVal = SendMessage(hPicCtrl, STM_SETIMAGE, IMAGE_BITMAP, LPARAM(hNewBitmapImage));

            return TRUE;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                case IDB_ABOUT_OK:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }
            }
            break;
        }

        case WM_CLOSE:
        {
            DeleteObject(hNewBitmapImage);
            EndDialog(hDlg, 0);
            return TRUE;
        }

    }
    return FALSE;
}