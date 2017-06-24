
// * ServerRM project *
// ScanIPAddresses.cpp : Functions implementing the scanning of IPaddresses in search of ClientRMs

#include "ServerInclude.h"

extern HWND hMainWnd;
extern CLIENTCONF ccPingReply;
extern BOOL fClientActive;
extern BOOL fPingInProgress;

DWORD WINAPI ScanIPAddresses(LPVOID lpArgs) {
    int iRetVal;
    int iOctet0, iOctet1, iOctet2, iOctet3;
    int awToIP[4], awFromIP[4], awScanIP[4];

    WCHAR wszSBMsg[MAX_SB_MSG + 1];
    WCHAR wszScanIP[IPADDR_LEN + 1] = L"";

    DWORD dwEventRetVal;
    SPMARGS *pSPMArgs = NULL;
    SCANRESULTS *pScanResults = NULL;
    HANDLE hPingInfoReadyEvent = NULL;
    HANDLE hIPScanCompleteEvent = NULL;

    SCAN_IPADDR_ARGS *pMyArgs = (SCAN_IPADDR_ARGS*)lpArgs;

    __try {
        if (StringToIPAddress(pMyArgs->wszFromIP, awFromIP) == FALSE)
            return FALSE;

        if (StringToIPAddress(pMyArgs->wszToIP, awToIP) == FALSE)
            return FALSE;

        if ((pSPMArgs = (SPMARGS*)malloc(sizeof(SPMARGS))) == NULL) {
            WriteToLogFile(FP_SERVER_LOGFILE, L"SPM", L"ScanIPAddresses", L"malloc() error: pSPMArgs",
                GetLastError());
            return FALSE;
        }
        memset(pSPMArgs, 0, sizeof(SPMARGS));
        pSPMArgs->cmd = SPM_STARTSESSION;

        // freeing this memory is the responsibility of the ScanResults dialog procedure
        if ((pScanResults = (SCANRESULTS*)malloc(sizeof(SCANRESULTS))) == NULL) {
            WriteToLogFile(FP_SERVER_LOGFILE, L"SPM", L"ScanIPAddresses", L"malloc() error: pScanResults",
                GetLastError());
            return FALSE;
        }
        memset(pScanResults, 0, sizeof(SCANRESULTS));

        // create a PingInfoReadyEvent
        hPingInfoReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_PING_INFO_READY);
        if (hPingInfoReadyEvent == NULL) {
            WriteToLogFile(FP_SERVER_LOGFILE, L"SPM", L"ScanIPAddresses", L"PingInfoReadyEvent creation error",
                GetLastError());
            return FALSE;
        }

        // set the fPingInProgress variables
        fPingInProgress = TRUE;
        swprintf_s(wszSBMsg, MAX_SB_MSG + 1, L"Scanning in progress. This could take a long time...");
        PrintToStatusBar(wszSBMsg, 1);

        for (iOctet0 = awFromIP[0]; iOctet0 <= awToIP[0]; ++iOctet0) {
            awScanIP[0] = iOctet0;
            for (iOctet1 = awFromIP[1]; iOctet1 <= awToIP[1]; ++iOctet1) {
                awScanIP[1] = iOctet1;
                for (iOctet2 = awFromIP[2]; iOctet2 <= awToIP[2]; ++iOctet2) {
                    awScanIP[2] = iOctet2;
                    for (iOctet3 = awFromIP[3]; iOctet3 <= awToIP[3]; ++iOctet3) {
                        awScanIP[3] = iOctet3;
                        if (IPAddressToString(awScanIP, wszScanIP, IPADDR_LEN + 1) == FALSE) {
                            // error
                            continue;
                        }

                        // Send command to ServerPluginManager() to
                        // ping the client machine with the current IP address.
                        pSPMArgs->iFromPlugin = ADMINGUI;
                        wcscpy_s(pSPMArgs->wszCliIP, IPADDR_LEN + 1, wszScanIP);
                        if ((iRetVal = ServerPluginManager(0, pSPMArgs, NULL)) == FALSE) {
                            // error
                            return FALSE;
                        }

                        // wait for PingInfoReadyEvent
                        dwEventRetVal = WaitForSingleObject(hPingInfoReadyEvent, INFINITE);
                        if (dwEventRetVal == WAIT_OBJECT_0)
                            ResetEvent(hPingInfoReadyEvent);

                        if (fClientActive == FALSE)
                            continue;

                        // Read the ClientInfo structure from the static global variable
                        // and update the ScanResults structure
                        pScanResults->nActive++;
                        memcpy(&pScanResults->ccClientConf[pScanResults->nActive - 1], &ccPingReply,
                            sizeof(CLIENTCONF));

                        // continue to next IP address
                    }
                }
            }
        }

        // reset the fPingInProgress variable
        fPingInProgress = FALSE;
        swprintf_s(wszSBMsg, MAX_SB_MSG + 1, L"Scan Complete");
        PrintToStatusBar(wszSBMsg, 1);

        // Send message to WndProc to open the Scan Results DialogBox
        SendNotifyMessage(hMainWnd, WM_START_SCAN_RESULTS, 0, (LPARAM)pScanResults);
        return TRUE;
    }
    __finally {
        if (hPingInfoReadyEvent)
            CloseHandle(hPingInfoReadyEvent);
        if (pMyArgs)
            free(pMyArgs);
        if (pSPMArgs)
            free(pSPMArgs);
    }

    return TRUE;
}

BOOL StringToIPAddress(const WCHAR *pwszIPAddr, int *awIPAddr) {
    int i;

    WCHAR *pwszCur, *pwszNext = NULL;
    WCHAR *pDotAt = NULL;
    WCHAR wszIP[IPADDR_LEN + 1] = L"";

    if (wcscpy_s(wszIP, IPADDR_LEN + 1, pwszIPAddr) != 0)
        return FALSE;

    pwszCur = wcstok_s(wszIP, L".", &pwszNext);
    awIPAddr[0] = _wtoi(pwszCur);
    for (i = 1; i <= 3; ++i) {
        pwszCur = wcstok_s(NULL, L".", &pwszNext);
        awIPAddr[i] = _wtoi(pwszCur);
    }

    return TRUE;
}

BOOL IPAddressToString(const int *awIPAddr, WCHAR *pwszIPAddr, int iLen) {
    int i;

    if (pwszIPAddr == NULL)
        return FALSE;

    // validate IP address
    for (i = 0; i < 4; ++i)
        if (awIPAddr[i] < 0 || awIPAddr[i] > 255)
            return FALSE;

    swprintf_s(pwszIPAddr, iLen, L"%d.%d.%d.%d", awIPAddr[0], awIPAddr[1], awIPAddr[2], awIPAddr[3]);

    return TRUE;
}