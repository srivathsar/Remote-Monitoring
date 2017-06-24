
// * ClientRM Project *
// Library.cpp: Commonly used user-defined functions

#include "ClientInclude.h"

SOCKET FTListenSocket = INVALID_SOCKET;

BOOL TransferFile(const WCHAR *pwszSrcPath) {
    extern HWND hMsgOnlyWnd;

    int iRetVal;
    BOOL fError = FALSE;

    WSADATA wsaData;

    char	szHostName[MAX_HOST_NAME + 1];
    char	*szHostIP;
    struct	hostent *HostDetails;

    static struct sockaddr_in MyAddr;

    WCHAR wszSWMsg[MAX_SW_MSG + 1];

    CCMARGS *pCCMArgs = NULL;

    __try {
        // initialize WSA
        if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
            iRetVal = WSAGetLastError();
            fError = TRUE;
            return FALSE;
        }
        PrintToStatusWindow(L"TransferFile() WSAStartup() called");

        // get local hostname and IPaddress assigned
        // first, the hostname
        gethostname(szHostName, MAX_HOST_NAME);

        // get host details
        HostDetails = gethostbyname(szHostName);
        szHostIP = inet_ntoa(*(struct in_addr *)*HostDetails->h_addr_list);

        // create socket
        if ((FTListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
            iRetVal = WSAGetLastError();
            fError = TRUE;
            return FALSE;
        }

        // call WSAAsyncSelect() to force functions to be non-blocking
        if (WSAAsyncSelect(FTListenSocket, hMsgOnlyWnd,
            WM_FT_SOCKET_NOTIFY, FD_READ |			// notification of readiness for reading
            FD_WRITE |		// notification of readiness for writing
            FD_CLOSE |		// notification of socket closure
            FD_ACCEPT)		// notification of incoming connections
            == SOCKET_ERROR) {
            iRetVal = WSAGetLastError();
            swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"WSAAsyncSelect() error %d", iRetVal);
            PrintToStatusWindow(wszSWMsg);
            fError = TRUE;
            return FALSE;
        }

        // bind socket to port
        MyAddr.sin_family = AF_INET;
        MyAddr.sin_port = htons(FTP_PORT);
        MyAddr.sin_addr.S_un.S_addr = inet_addr(szHostIP);

        if (bind(FTListenSocket, (SOCKADDR*)&MyAddr, sizeof(MyAddr)) != 0) {
            iRetVal = WSAGetLastError();
            fError = TRUE;
            return FALSE;
        }

        // listen
        if (listen(FTListenSocket, 3) != 0) {
            iRetVal = WSAGetLastError();
            fError = TRUE;
            return FALSE;
        }

        // notify client that it is ready
        if ((pCCMArgs = (CCMARGS*)malloc(sizeof(CCMARGS))) == NULL)
            return FALSE;

        pCCMArgs->cmd = CCMN_FT_READY;
        ClientComModule(CCM_SEND_NOTIFICATION, pCCMArgs, NULL);

        PrintToStatusWindow(L"FT: Waiting for an incoming connection...");

        return TRUE;
    }
    __finally {
        if (fError == TRUE)
            closesocket(FTListenSocket);

        if (pCCMArgs)
            free(pCCMArgs);
    }

}// TransferFile()

BOOL GetNextDate(WORD *pDate, WORD *pMonth, WORD *pYear) {

    WORD wDate, wMonth, wYear;

    int iMaxMonthDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    int iLeapMaxMonthDays[] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    wDate = *pDate;
    wMonth = *pMonth;
    wYear = *pYear;

    wDate++;

    // check leap year
    if ((wYear % 400 == 0) || ((wYear % 4 == 0) && (wYear % 100 != 0))) {
        if (wDate > iLeapMaxMonthDays[wMonth - 1])		// overflow
        {
            wDate = 1;
            wMonth++;
            if (wMonth > 12) {
                wMonth = 1;
                wYear++;
            }
        }
    } else {
        if (wDate > iMaxMonthDays[wMonth - 1])		// overflow
        {
            wDate = 1;
            wMonth++;
            if (wMonth > 12) {
                wMonth = 1;
                wYear++;
            }
        }
    }

    // set the _in_out arguments
    *pDate = wDate;
    *pMonth = wMonth;
    *pYear = wYear;

    return TRUE;

}// GetNextDate()