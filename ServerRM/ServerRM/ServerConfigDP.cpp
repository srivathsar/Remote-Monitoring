
// * ServerRM project *
// ServerConfigDP.cpp : Contains all the Dialog procks for menu items under 'Server Config'

#include "ServerInclude.h"

// ListView Column Width #defines
#define LVC_PLUGINDB_UI_PID		0x20
#define LVC_PLUGINDB_UI_NAME	0x70
#define LVC_PLUGINDB_UI_ITFNAME	0x70
#define LVC_PLUGINDB_UI_PATH	0x70

extern HWND hMainWnd;
extern HINSTANCE hMainInstance;


BOOL UpdateCliUI(HWND hDlg);
BOOL UpdatePluginDP(HWND hDlg);

BOOL CALLBACK AddCliDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    int iRetVal = 0;

    switch (message) {
        case WM_INITDIALOG:
        {
            SendMessage(hDlg, DM_SETDEFID, (WPARAM)IDC_ADD_CLIENT_IP, (LPARAM)NULL);
            CenterWindow(hDlg);
            return TRUE;
        }

        case WM_COMMAND:
        {
            // handle commands sent from child window controls
            switch (LOWORD(wParam)) {

                case IDB_ADD_CLIENT_ADD:
                {
                    WORD wLen;
                    DWORD dwEnteredIP;
                    CLIENTINFO *pNewClient = NULL;
                    BYTE bFirst, bSecond, bThird, bFourth;

                    WCHAR wszName[MAX_NICK_NAME + 1];

                    __try {
                        // first, the IP address
                        iRetVal = SendDlgItemMessage(hDlg, IDC_ADD_CLIENT_IP, IPM_GETADDRESS,
                            0, (LPARAM)(LPDWORD)&dwEnteredIP);
                        // unpack the IP address
                        bFirst = FIRST_IPADDRESS(dwEnteredIP);
                        bSecond = SECOND_IPADDRESS(dwEnteredIP);
                        bThird = THIRD_IPADDRESS(dwEnteredIP);
                        bFourth = FOURTH_IPADDRESS(dwEnteredIP);

                        // now, the nick name
                        // get number of characters
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_ADD_CLIENT_NAME, EM_LINELENGTH, 0, 0);
                        if (wLen > MAX_NICK_NAME - 1) {
                            MessageBox(hDlg, L"Nickname too long. Must be less than 32 characters", 0, 0);
                            return FALSE;
                        }
                        if (wLen == 0) {
                            MessageBox(hDlg, L"Please enter a nickname", 0, 0);
                            return FALSE;
                        }

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszName) = wLen;

                        // Get the characters. 
                        SendDlgItemMessage(hDlg, IDC_ADD_CLIENT_NAME, EM_GETLINE,
                            (WPARAM)0,       // line 0 
                            (LPARAM)wszName);

                        // Null-terminate the string. 
                        wszName[wLen] = 0;

                        if ((pNewClient = (CLIENTINFO*)malloc(sizeof(CLIENTINFO))) == NULL) {
                            WriteToLogFile(FP_SERVER_LOGFILE, L"ServerConfigDP", L"AddCliDP", L"malloc() error: pNewClient",
                                GetLastError());
                            MessageBox(hDlg, L"malloc() Error. See the log file for more details.", L"Add Client", MB_ICONINFORMATION);
                            return TRUE;
                        } else {
                            memset(pNewClient, 0, sizeof(CLIENTINFO));

                            // copy IP and nick name to CLIENTINFO structure
                            swprintf_s(pNewClient->wszIP, IPADDR_LEN + 1, L"%d.%d.%d.%d", bFirst, bSecond, bThird, bFourth);
                            wcscpy_s(pNewClient->wszName, MAX_NICK_NAME + 1, wszName);
                            if (AddClientToDBFile(pNewClient) == FALSE) {
                                MessageBox(hDlg, L"Addition unsuccessful", L"Add Client", MB_ICONINFORMATION);
                                return TRUE;
                            }

                            MessageBox(hDlg, L"Addition successful", L"Add Client", MB_ICONINFORMATION);

                            // close dialog box
                            SendMessage(hDlg, WM_CLOSE, 0, 0);
                            return TRUE;

                        }
                    }

                    __finally {
                        if (pNewClient)
                            free(pNewClient);
                    }
                }

                case IDB_ADD_CLIENT_RESET:
                {
                    // clear IP address control
                    SendDlgItemMessage(hDlg, IDC_ADD_CLIENT_IP, IPM_CLEARADDRESS, 0, 0);

                    // clear edit control
                    SendDlgItemMessage(hDlg, IDC_ADD_CLIENT_NAME, EM_SETSEL, 0, -1);
                    SendDlgItemMessage(hDlg, IDC_ADD_CLIENT_NAME, EM_REPLACESEL,
                        FALSE, //operation cannot be undone
                        (LPARAM)(LPCSTR)L"");

                    return TRUE;
                }

                case IDB_ADD_CLIENT_CANCEL:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }

            }// switch(LOWORD(wParam)
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

BOOL CALLBACK RemCliDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    LVCOLUMN LvCol;
    static int iSelected;
    static HWND hClientList = NULL;

    int i, iRetVal;
    CLIENTDB ClientsDB;

    WCHAR wszSelIP[IPADDR_LEN + 1] = L"";

    switch (message) {
        case WM_INITDIALOG:
        {
            CenterWindow(hDlg);

            // get list view handle
            hClientList = GetDlgItem(hDlg, IDC_VIEW_REM_CLIENTS_LIST);

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

            UpdateCliUI(hDlg);

            return TRUE;
        }

        case WM_COMMAND:
        {
            // handle commands sent from child window controls
            switch (LOWORD(wParam)) {
                case IDB_REMOVE_CLIENT:
                {
                    LVITEM lviSelectedItem = { 0 };

                    // retrieve the selection
                    lviSelectedItem.iItem = iSelected;
                    lviSelectedItem.mask = LVIF_TEXT;
                    lviSelectedItem.pszText = wszSelIP;
                    lviSelectedItem.cchTextMax = 256;

                    iRetVal = ListView_GetItem(hClientList, &lviSelectedItem);

                    // read existing clients from file
                    memset(&ClientsDB, 0, sizeof(ClientsDB));
                    if ((iRetVal = ReadClientsDBFile(&ClientsDB)) == FALSE) {
                        MessageBox(hDlg, L"Error reading clients database file.", L"Remove Client", MB_OK);
                        return TRUE;
                    }

                    for (i = 0; i < ClientsDB.nClients; ++i) {
                        if (wcscmp(ClientsDB.ClientInfo[i].wszIP, wszSelIP) != 0)
                            continue;

                        // shift the last entry to current position
                        memset(&ClientsDB.ClientInfo[i], 0, sizeof(CLIENTINFO));
                        memcpy(&ClientsDB.ClientInfo[i],
                            &ClientsDB.ClientInfo[ClientsDB.nClients - 1],
                            sizeof(CLIENTINFO));
                        memset(&ClientsDB.ClientInfo[ClientsDB.nClients - 1], 0, sizeof(CLIENTINFO));

                        // decrement nClients
                        ClientsDB.nClients--;

                        break;
                    }

                    // write updated struct to file
                    if ((iRetVal = WriteClientsDBFile(&ClientsDB)) == FALSE) {
                        MessageBox(hDlg, L"Error writing clients database file.", L"Add Client", MB_OK);
                        return TRUE;
                    }

                    // update UI
                    UpdateCliUI(hDlg);
                    return TRUE;
                }

                case IDB_REM_CLIENT_CANCEL:
                {
                    EndDialog(hDlg, 0);
                    return TRUE;
                }

            }// switch(LOWORD(wParam)
            return FALSE;
        }

        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
                case NM_CLICK:
                {
                    // check whether the click was inside the list view
                    if (((LPNMHDR)lParam)->hwndFrom == hClientList &&
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
            EndDialog(hDlg, 0);
            return TRUE;
        }

    }
    return FALSE;

}

BOOL CALLBACK ViewCliDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    LVCOLUMN LvCol;

    int i, iRetVal;
    static int iSelected = -1;
    static HWND hClientList = NULL;
    LPNMLISTVIEW lpNMListView = NULL;

    WCHAR **pwszNewNick = NULL;

    CLIENTDB *pClientDB = NULL;

    switch (message) {
        case WM_INITDIALOG:
        {
            CenterWindow(hDlg);

            // get list view handle
            hClientList = GetDlgItem(hDlg, IDC_VIEW_REM_CLIENTS_LIST);

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

            UpdateCliUI(hDlg);

            return TRUE;
        }

        case WM_COMMAND:
        {
            // handle commands sent from child window controls
            switch (LOWORD(wParam)) {
                case IDB_VIEW_CLIENTS_EDIT:
                {
                    LVITEM lvItem = { 0 };

                    if (iSelected < 0) {
                        MessageBox(hDlg, L"Please select a client", L"View Clients", MB_ICONINFORMATION);
                        return TRUE;
                    }

                    __try {
                        if ((pwszNewNick = (WCHAR**)malloc(sizeof(WCHAR*) * 2)) == NULL) {
                            //WriteToLogFile()
                            return TRUE;
                        }
                        pwszNewNick[0] = NULL;	// selected IP
                        pwszNewNick[1] = NULL;	// new nick name

                        for (i = 0; i < 2; ++i) {
                            if ((pwszNewNick[i] = (WCHAR*)malloc(sizeof(WCHAR) * (MAX_NICK_NAME + 1))) == NULL) {
                                // WriteToLogFile()
                                return TRUE;
                            }
                            pwszNewNick[i][0] = 0;
                        }

                        // get the selected IPaddress
                        lvItem.mask = LVIF_TEXT;
                        lvItem.iItem = iSelected;
                        lvItem.pszText = pwszNewNick[0];
                        lvItem.cchTextMax = MAX_NICK_NAME + 1;
                        if ((iRetVal = SendMessage(hClientList, LVM_GETITEM, 0, (LPARAM)&lvItem)) == FALSE) {
                            MessageBox(hDlg, L"Unable to retrieve selected item", L"View Clients", MB_ICONINFORMATION);
                            return TRUE;
                        }

                        // call Edit Nick Name dialog box
                        iRetVal = DialogBoxParam(hMainInstance, MAKEINTRESOURCE(IDD_EDIT_NICKNAME), hDlg, EditNickNameDP, (LPARAM)pwszNewNick);

                        if (wcscmp(pwszNewNick[1], L"") == 0)
                            return TRUE;

                        if ((pClientDB = (CLIENTDB*)malloc(sizeof(CLIENTDB))) == NULL) {
                            // WriteToLogFile()
                            return TRUE;
                        }
                        memset(pClientDB, 0, sizeof(CLIENTDB));

                        // read the client DB file
                        if (ReadClientsDBFile(pClientDB) == FALSE) {
                            MessageBox(hDlg, L"Error reading client database file", L"View Clients", MB_ICONEXCLAMATION);
                            return TRUE;
                        }

                        // search for the selected client (by IPaddress)
                        for (i = 0; i < pClientDB->nClients; ++i) {
                            if (wcscmp(pClientDB->ClientInfo[i].wszIP, lvItem.pszText) == 0) {
                                // edit the nickname
                                wcscpy_s(pClientDB->ClientInfo[i].wszName, MAX_NICK_NAME + 1, pwszNewNick[1]);
                                break;
                            }
                        }

                        // write to database file
                        if (WriteClientsDBFile(pClientDB) == FALSE) {
                            MessageBox(hDlg, L"Error writing client database file", L"View Clients", MB_ICONEXCLAMATION);
                            return TRUE;
                        }

                        // update the UI
                        UpdateCliUI(hDlg);

                        iSelected = -1;

                        return TRUE;

                    }
                    __finally {
                        for (i = 0; i < 2; ++i)
                            if (pwszNewNick[i]) {
                                free(pwszNewNick[i]);
                                pwszNewNick[i] = NULL;
                            }

                        if (pwszNewNick) {
                            free(pwszNewNick);
                            pwszNewNick = NULL;
                        }

                        if (pClientDB) {
                            free(pClientDB);
                            pClientDB = NULL;
                        }
                    }
                }

                case IDB_VIEW_CLIENTS_OK:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }

            }// switch(LOWORD(wParam)
            return FALSE;
        }

        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
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

        case WM_CLOSE:
        {
            EndDialog(hDlg, 0);
            return TRUE;
        }
    }
    return FALSE;

}

BOOL CALLBACK AddPluginDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {

    switch (message) {

        case WM_INITDIALOG:
        {
            CenterWindow(hDlg);
            return TRUE;
        }

        case WM_COMMAND:
        {
            // handle commands sent from child window controls
            switch (LOWORD(wParam)) {
                case IDB_ADD_PLUGIN_ADD:
                {
                    WORD wLen;
                    WIN32_FIND_DATA fdFileData;
                    PLUGININFO *pNewPluginInfo = NULL;
                    WCHAR wszSearchPath[MAX_PATH + 1];
                    WCHAR wszPluginID[MAX_PLUGIN_ID] = L"";

                    __try {
                        if ((pNewPluginInfo = (PLUGININFO*)malloc(sizeof(PLUGININFO))) == NULL) {
                            // WriteToLogFile()
                            return TRUE;
                        }
                        memset(pNewPluginInfo, 0, sizeof(PLUGININFO));

                        // get the entered strings

                        // Plugin ID
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_ADD_PLUGIN_PID, EM_LINELENGTH, 0, 0);
                        if (wLen == 0) {
                            MessageBox(hDlg, L"Please enter the Plugin ID", L"Add New Plugin", MB_ICONINFORMATION);
                            return TRUE;
                        }
                        if (wLen > MAX_PLUGIN_ID) {
                            MessageBox(hDlg, L"Entered PluginID Too Long", L"Add New Plugin", MB_ICONINFORMATION);
                            return TRUE;
                        }

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)wszPluginID) = wLen;

                        // Get the characters.		// check for errors
                        SendDlgItemMessage(hDlg, IDC_ADD_PLUGIN_PID, EM_GETLINE, (WPARAM)0, (LPARAM)wszPluginID);

                        // Null-terminate the string. 
                        wszPluginID[wLen] = 0;

                        pNewPluginInfo->iPluginID = _wtoi(wszPluginID);

                        // path
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_ADD_PLUGIN_PATH, EM_LINELENGTH, 0, 0);
                        if (wLen == 0) {
                            MessageBox(hDlg, L"Please enter the path where the DLL files(s) can be found", L"Add New Plugin",
                                MB_ICONINFORMATION);
                            return FALSE;
                        }
                        if (wLen > MAX_PATH) {
                            MessageBox(hDlg, L"Path entered is too long", L"Add New Plugin", MB_ICONINFORMATION);
                            return TRUE;
                        }

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)pNewPluginInfo->wszPath) = wLen;

                        // Get the characters.		// check for errors
                        SendDlgItemMessage(hDlg, IDC_ADD_PLUGIN_PATH, EM_GETLINE, (WPARAM)0,
                            (LPARAM)pNewPluginInfo->wszPath);

                        // Null-terminate the string. 
                        pNewPluginInfo->wszPath[wLen] = 0;

                        // plugin name
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_ADD_PLUGIN_NAME, EM_LINELENGTH, 0, 0);
                        if (wLen > MAX_FILE_NAME) {
                            MessageBox(hDlg, L"Name entered is too long", L"Add New Plugin", MB_ICONINFORMATION);
                            return TRUE;
                        }

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)pNewPluginInfo->wszName) = wLen;

                        // Get the characters.		// check for errors
                        SendDlgItemMessage(hDlg, IDC_ADD_PLUGIN_NAME, EM_GETLINE, (WPARAM)0,
                            (LPARAM)pNewPluginInfo->wszName);

                        // Null-terminate the string. 
                        pNewPluginInfo->wszName[wLen] = 0;

                        // Interface DLL name
                        wLen = (WORD)SendDlgItemMessage(hDlg, IDC_ADD_PLUGIN_IF_NAME, EM_LINELENGTH, 0, 0);
                        if (wLen == 0) {
                            MessageBox(hDlg, L"Please enter the InterfaceDLL Name", L"Add New Plugin", MB_ICONINFORMATION);
                            return FALSE;
                        }
                        if (wLen > MAX_FILE_NAME) {
                            MessageBox(hDlg, L"InterfaceDLL Name entered is too long", L"Add New Plugin", MB_ICONINFORMATION);
                            return TRUE;
                        }

                        // Put the number of characters into first word of buffer. 
                        *((LPWORD)pNewPluginInfo->wszDLLInterfaceName) = wLen;

                        // Get the characters.		// check for errors
                        SendDlgItemMessage(hDlg, IDC_ADD_PLUGIN_IF_NAME, EM_GETLINE, (WPARAM)0,
                            (LPARAM)pNewPluginInfo->wszDLLInterfaceName);

                        // Null-terminate the string. 
                        pNewPluginInfo->wszDLLInterfaceName[wLen] = 0;

                        // check whether the interface DLL exists
                        if (wcscmp(pNewPluginInfo->wszDLLInterfaceName, L"") != 0) {
                            swprintf_s(wszSearchPath, MAX_PATH + 1, L"%s\\%s.dll", pNewPluginInfo->wszPath, pNewPluginInfo->wszDLLInterfaceName);
                            if (FindFirstFile(wszSearchPath, &fdFileData) == INVALID_HANDLE_VALUE) {
                                MessageBox(hDlg, L"InterfaceDLL not found", L"Add New Plugin", MB_ICONEXCLAMATION);
                                return TRUE;
                            }
                        }

                        // check whether the plugin DLL exists
                        if (wcscmp(pNewPluginInfo->wszName, L"") != 0) {
                            swprintf_s(wszSearchPath, MAX_PATH + 1, L"%s\\%s.dll", pNewPluginInfo->wszPath, pNewPluginInfo->wszName);
                            if (FindFirstFile(wszSearchPath, &fdFileData) == INVALID_HANDLE_VALUE) {
                                MessageBox(hDlg, L"PluginDLL not found", L"Add New Plugin", MB_ICONEXCLAMATION);
                                return TRUE;
                            }
                        }

                        // add to database
                        if (AddPluginToDBFile(pNewPluginInfo) == FALSE)
                            MessageBox(hDlg, L"Addition unsuccessful", L"Add Plugin", MB_ICONINFORMATION | MB_OK);
                        else {
                            MessageBox(hDlg, L"Addition successful", L"Add Plugin", MB_ICONINFORMATION | MB_OK);
                            SendMessage(hDlg, WM_CLOSE, 0, 0);
                        }

                        return TRUE;
                    }
                    __finally {
                        if (pNewPluginInfo) {
                            free(pNewPluginInfo);
                            pNewPluginInfo = NULL;
                        }
                    }
                }

                case IDB_ADD_PLUGIN_CANCEL:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }

            }// switch(LOWORD(wParam)
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

BOOL CALLBACK RemPluginDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    int iRetVal;

    static int iSelected;
    static HWND hAppList;

    switch (message) {

        case WM_INITDIALOG:
        {
            LVCOLUMN lvColumn = { 0 };

            CenterWindow(hDlg);

            // get list view handle
            hAppList = GetDlgItem(hDlg, IDC_VIEW_REM_PLUGINS_LIST);

            // Set up list view column headers
            memset(&lvColumn, 0, sizeof(lvColumn));
            lvColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

            lvColumn.pszText = L"PID";										// header Text
            lvColumn.cx = LVC_PLUGINDB_UI_PID;												// width of column
            SendMessage(hAppList, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

            lvColumn.pszText = L"DLL Name";
            lvColumn.cx = LVC_PLUGINDB_UI_NAME;
            SendMessage(hAppList, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

            lvColumn.pszText = L"Interface Name";
            lvColumn.cx = LVC_PLUGINDB_UI_ITFNAME;
            SendMessage(hAppList, LVM_INSERTCOLUMN, 2, (LPARAM)&lvColumn);

            lvColumn.pszText = L"Path";
            lvColumn.cx = LVC_PLUGINDB_UI_PATH;
            SendMessage(hAppList, LVM_INSERTCOLUMN, 3, (LPARAM)&lvColumn);

            UpdatePluginDP(hDlg);

            return TRUE;
        }


        case WM_COMMAND:
        {
            // handle commands sent from child window controls
            switch (LOWORD(wParam)) {
                case IDB_REMOVE_PLUGIN:
                {
                    int i, iPID;

                    LVITEM lviSelected = { 0 };
                    WCHAR wszSelectedPID[MAX_PLUGIN_ID_TEXT + 1];

                    PLUGINDB PluginDB;

                    // retrieve the selection
                    lviSelected.iItem = iSelected;
                    lviSelected.mask = LVIF_TEXT;
                    lviSelected.pszText = wszSelectedPID;
                    lviSelected.cchTextMax = MAX_PLUGIN_ID_TEXT + 1;

                    iRetVal = ListView_GetItem(hAppList, &lviSelected);

                    // read existing plugins from file
                    memset(&PluginDB, 0, sizeof(PluginDB));
                    if ((iRetVal = ReadPluginDBFile(&PluginDB)) == FALSE) {
                        MessageBox(hDlg, L"Error reading clients database file.", L"Remove Client", MB_OK);
                        return TRUE;
                    }

                    iPID = _wtoi(wszSelectedPID);
                    for (i = 0; i < PluginDB.nPlugins; ++i) {
                        if (iPID != PluginDB.PluginInfo[i].iPluginID)
                            continue;

                        // shift the last entry to current position
                        memset(&PluginDB.PluginInfo[i], 0, sizeof(PLUGININFO));
                        memcpy(&PluginDB.PluginInfo[i],
                            &PluginDB.PluginInfo[PluginDB.nPlugins - 1],
                            sizeof(PLUGININFO));
                        memset(&PluginDB.PluginInfo[PluginDB.nPlugins - 1], 0, sizeof(PLUGININFO));

                        // decrement nClients
                        PluginDB.nPlugins--;

                        break;
                    }

                    // write updated struct to file
                    if ((iRetVal = WritePluginDBFile(&PluginDB)) == FALSE) {
                        MessageBox(hDlg, L"Error writing plugin database file.", L"Remove Plugin", MB_OK);
                        return TRUE;
                    }

                    // update UI
                    UpdatePluginDP(hDlg);
                    return TRUE;
                }

                case IDB_REM_PLUGIN_CANCEL:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }

            }// switch(LOWORD(wParam)
            return FALSE;
        }

        case WM_NOTIFY:
        {
            switch (((LPNMHDR)lParam)->code) {
                case NM_CLICK:
                {
                    // check whether the click was inside the list view
                    if (((LPNMHDR)lParam)->hwndFrom == hAppList &&
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
            EndDialog(hDlg, 0);
            return TRUE;
        }

    }
    return FALSE;

}

BOOL CALLBACK ViewPluginDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hAppList;

    switch (message) {

        case WM_INITDIALOG:
        {
            LVCOLUMN lvColumn = { 0 };

            CenterWindow(hDlg);

            // get list view handle
            hAppList = GetDlgItem(hDlg, IDC_VIEW_REM_PLUGINS_LIST);

            // Set up list view column headers
            memset(&lvColumn, 0, sizeof(lvColumn));
            lvColumn.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

            lvColumn.pszText = L"PID";
            lvColumn.cx = LVC_PLUGINDB_UI_PID;
            SendMessage(hAppList, LVM_INSERTCOLUMN, 0, (LPARAM)&lvColumn);

            lvColumn.pszText = L"DLL Name";
            lvColumn.cx = LVC_PLUGINDB_UI_NAME;
            SendMessage(hAppList, LVM_INSERTCOLUMN, 1, (LPARAM)&lvColumn);

            lvColumn.pszText = L"Interface Name";
            lvColumn.cx = LVC_PLUGINDB_UI_ITFNAME;
            SendMessage(hAppList, LVM_INSERTCOLUMN, 2, (LPARAM)&lvColumn);

            lvColumn.pszText = L"Path";
            lvColumn.cx = LVC_PLUGINDB_UI_PATH;
            SendMessage(hAppList, LVM_INSERTCOLUMN, 3, (LPARAM)&lvColumn);

            UpdatePluginDP(hDlg);
            return TRUE;
        }


        case WM_COMMAND:
        {
            // handle commands sent from child window controls
            switch (LOWORD(wParam)) {

                case IDB_VIEW_PLUGINS_OK:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }
            }// switch(LOWORD(wParam)
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

BOOL CALLBACK EditNickNameDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static WCHAR **pwszNick = NULL;

    switch (message) {
        case WM_INITDIALOG:
        {
            CenterWindow(hDlg);

            // lParam contains address to where entered nick name must be copied
            pwszNick = (WCHAR**)lParam;

            // display the IPaddress of the client whose nick name is being changed in the title bar
            SetWindowText(hDlg, pwszNick[0]);

            return TRUE;
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
                case IDB_EDIT_NICKNAME_OK:
                {
                    WORD wLen;

                    // get the entered nickname
                    wLen = (WORD)SendDlgItemMessage(hDlg, IDC_EDIT_NICKNAME_EDIT, EM_LINELENGTH, 0, 0);
                    if (wLen > MAX_NICK_NAME) {
                        MessageBox(hDlg, L"Nickname too long. Must be less than 32 characters", 0, 0);
                        return TRUE;
                    }
                    if (wLen == 0) {
                        MessageBox(hDlg, L"Please enter a nickname", 0, 0);
                        return TRUE;
                    }

                    // Put the number of characters into first word of buffer. 
                    *((LPWORD)pwszNick[1]) = wLen;

                    // Get the characters. 
                    SendDlgItemMessage(hDlg, IDC_EDIT_NICKNAME_EDIT, EM_GETLINE, (WPARAM)0, (LPARAM)pwszNick[1]);

                    // Null-terminate the string. 
                    pwszNick[1][wLen] = 0;

                    SendMessage(hDlg, WM_CLOSE, 0, 0);
                    return TRUE;
                }

                case IDB_EDIT_NICKNAME_CANCEL:
                {
                    SendMessage(hDlg, WM_CLOSE, 0, 0);
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
BOOL UpdateCliUI(HWND hDlg) {
    int i, iRetVal;
    int iCliDBFile = 0;
    CLIENTDB ClientsDB;

    LV_ITEM lviItem;
    HWND hClientList = NULL;

    // read client info from file and display in the list view control
    memset(&ClientsDB, 0, sizeof(CLIENTDB));
    if ((iRetVal = ReadClientsDBFile(&ClientsDB)) == FALSE)
        return FALSE;

    hClientList = GetDlgItem(hDlg, IDC_VIEW_REM_CLIENTS_LIST);

    ListView_DeleteAllItems(hClientList);
    lviItem.mask = LVIF_TEXT;
    for (i = 0; i < ClientsDB.nClients; ++i) {
        // IPaddress
        lviItem.iItem = i;
        lviItem.iSubItem = 0;
        lviItem.pszText = ClientsDB.ClientInfo[i].wszIP;
        lviItem.cchTextMax = IPADDR_LEN + 1;

        SendMessage(hClientList, LVM_INSERTITEM, 0, (LPARAM)&lviItem);

        // Nick name
        lviItem.iSubItem = 1;
        lviItem.cchTextMax = MAX_NICK_NAME + 1;
        lviItem.pszText = ClientsDB.ClientInfo[i].wszName;

        SendMessage(hClientList, LVM_SETITEM, 0, (LPARAM)&lviItem);
    }

    return TRUE;
}

BOOL UpdatePluginDP(HWND hDlg) {
    int i, iRetVal;
    int iPluginDBFile = 0;
    PLUGINDB PluginDB;

    LV_ITEM lviItem;
    HWND hPluginList = NULL;

    WCHAR wszPluginID[8];

    // read plugin database file and display in the list view control
    memset(&PluginDB, 0, sizeof(PLUGINDB));
    if ((iRetVal = ReadPluginDBFile(&PluginDB)) == FALSE)
        return FALSE;

    hPluginList = GetDlgItem(hDlg, IDC_VIEW_REM_PLUGINS_LIST);

    ListView_DeleteAllItems(hPluginList);
    lviItem.mask = LVIF_TEXT;
    for (i = 0; i < PluginDB.nPlugins; ++i) {
        // Plugin ID
        swprintf_s(wszPluginID, L"%d", PluginDB.PluginInfo[i].iPluginID);
        lviItem.iItem = i;
        lviItem.iSubItem = 0;
        lviItem.pszText = wszPluginID;
        lviItem.cchTextMax = 8;
        SendMessage(hPluginList, LVM_INSERTITEM, 0, (LPARAM)&lviItem);

        // Main plugin name
        lviItem.iSubItem = 1;
        lviItem.pszText = PluginDB.PluginInfo[i].wszName;
        lviItem.cchTextMax = MAX_FILE_NAME + 1;
        SendMessage(hPluginList, LVM_SETITEM, 0, (LPARAM)&lviItem);

        // Plugin interface name
        lviItem.iSubItem = 2;
        lviItem.pszText = PluginDB.PluginInfo[i].wszDLLInterfaceName;
        lviItem.cchTextMax = MAX_FILE_NAME + 1;
        SendMessage(hPluginList, LVM_SETITEM, 0, (LPARAM)&lviItem);

        // Path
        lviItem.iSubItem = 3;
        lviItem.pszText = PluginDB.PluginInfo[i].wszPath;
        lviItem.cchTextMax = MAX_PATH + 1;
        SendMessage(hPluginList, LVM_SETITEM, 0, (LPARAM)&lviItem);
    }

    return TRUE;
}
