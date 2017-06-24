
// * ServerRM Project *
// MsgOnlyWndProc.cpp: Window procedure function to handle messages sent to message-only windows.

#include "ServerInclude.h"

// Global variables
WCHAR g_FTwszDestPath[MAX_PATH + 1];	// local machine
WCHAR g_FTwszSourcePath[MAX_PATH + 1];	// remote machine

BOOL fConnected = FALSE;
BOOL fFTSuccess = FALSE;
int iFTStatus = 0;
BOOL fFTInProgress = FALSE;
BOOL fFTDeleteFile = FALSE;

// static variables
static int iCCMDataSize = sizeof(CCMDATA);
static int iSCMDataSize = sizeof(SCMDATA);
static int iFTPacketSize = sizeof(FTPACKET);

LRESULT CALLBACK MsgOnlyWndProc(HWND hMOWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    int iRetVal;

    WSADATA wsaData;
    WORD wEvent, wError;
    struct sockaddr_in ClientAddr;
    char szCliIP[IPADDR_LEN + 1];
    static SOCKET ComSocket = INVALID_SOCKET;

    BOOL DefCharSet;
    WCHAR wszCliIP[IPADDR_LEN + 1] = L"";
    WCHAR wszSWMsg[MAX_SW_MSG + 1] = L"";

    int iBytesReceived;
    static int iTotalBytesReceived;
    char szRecvBuf[RECV_BUFSIZE + 1];
    static char szAggRecvBuf[sizeof(CCMDATA) + 1];

    int iBytesSent;
    //static DWORD dwTotalBytesSent;
    // char szSendBuf[SENDBUF_SIZE + 1];
    static char szAggSendBuf[sizeof(SCMDATA) + 1];

    SCMDATA *pSCMData;
    CCMDATA *pCCMDataIn = NULL;

    // File Transfer communication
    extern SOCKET FTPSocket;

    int			iFTSentBytes;
    //static int	iFTTotalSentBytes;
    char szFTSendBuf[sizeof(FTPACKET) + 1];
    //static char szFTAggSendBuf[sizeof(FTPACKET) + 1];

    int			iFTReceivedBytes;
    static int	iFTTotalReceivedBytes = 0;
    //char		szFTRecvBuf[sizeof(FTPACKET) + 1];
    static char szFTAggRecvBuf[sizeof(FTPACKET) + 1];

    static FTPACKET FTPacket;

    // Destination File management
    static int		iDestFD = 0;
    int				iBytesWritten;
    static int		iFileSize;
    CHECKSUM		RecvChecksum;
    CHECKSUM		NewChecksum;

    DWORD dwEventRetVal;
    static HANDLE hWriteReadyEvent = NULL;

    switch (message) {

        case WM_SESSION_START:
        {
            // lParam points to client IP, copy that into our own variable
            lstrcpy(wszCliIP, (WCHAR*)lParam);

            // initialize WSA
            if ((WSAStartup(MAKEWORD(2, 2), &wsaData)) != 0) {
                iRetVal = WSAGetLastError();
                PrintToStatusWindow(L"SCM: WSAStartup() error");
                return FALSE;
            }
            PrintToStatusWindow(L"WSAStartup() called");

            // create socket
            if ((ComSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
                PrintToStatusWindow(L"SCM: Socket creation error");
                iRetVal = WSAGetLastError();
                return FALSE;
            }

            // call WSAAsyncSelect() to force functions to be non-blocking
            if (WSAAsyncSelect(ComSocket, hMOWnd,
                WM_SOCKET_NOTIFY, FD_CONNECT |		// notification of completed connection
                FD_READ |			// notification of readiness for reading
                FD_WRITE |		// notification of readiness for writing
                FD_CLOSE)		// notification of socket closure
                == SOCKET_ERROR) {
                iRetVal = WSAGetLastError();
                PrintToStatusWindow(L"SCM: WSAAsyncSelect() error");
                closesocket(ComSocket);
                WSACleanup();
                PrintToStatusWindow(L"WSAAsyncSelect error WSACleanup() called");
                return FALSE;
            }

            // fill ClientAddr structure
            //iRetVal = GetClientIP(szCliIP);
            ClientAddr.sin_family = AF_INET;
            ClientAddr.sin_port = htons(COM_PORT);
            iRetVal = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wszCliIP,
                wcsnlen_s(wszCliIP, IPADDR_LEN + 1), szCliIP, IPADDR_LEN + 1, "?", &DefCharSet);
            if (iRetVal == 0) {
                iRetVal = GetLastError();
                swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"SCM: Error converting wide char to multibyte. Cannot proceed. %d", iRetVal);
                PrintToStatusWindow(wszSWMsg);
                closesocket(ComSocket);
                WSACleanup();
                PrintToStatusWindow(L"WM_SESSION_START WCMB WSACleanup() called");
                return FALSE;
            }

            // null-terminate the string
            szCliIP[iRetVal] = 0;
            ClientAddr.sin_addr.S_un.S_addr = inet_addr(szCliIP);

            // attempt to connect to socket
            iRetVal = connect(ComSocket, (sockaddr*)&ClientAddr,
                sizeof(ClientAddr));
            if ((iRetVal = WSAGetLastError()) != WSAEWOULDBLOCK) {
                PrintToStatusWindow(L"SCM: Could not establish communication");
                ServerComModule(SCMN_DISCONNECTED, NULL, NULL);
                closesocket(ComSocket);
                ComSocket = INVALID_SOCKET;
                WSACleanup();
                PrintToStatusWindow(L"connect() != WSAEWOULDBLOCK WSACleanup() called");
                return FALSE;
            }
            return FALSE;	// case WM_SESSION_START
        }


        case WM_SESSION_STOP:
        {
            if (fFTInProgress) {
                iRetVal = MessageBox(NULL, L"File Transfer is in progress. Do you still want to disconnect?",
                    L"Disconnect", MB_YESNO);
                if (iRetVal == IDYES)
                    SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                else
                    return 0;
            }

            // clear static data
            iTotalBytesReceived = 0;
            memset(szAggRecvBuf, 0, sizeof(szAggRecvBuf));
            memset(szAggSendBuf, 0, sizeof(szAggSendBuf));

            if (!fConnected)
                return 0;

            closesocket(ComSocket);
            ComSocket = INVALID_SOCKET;
            fConnected = FALSE;
            if (WSACleanup() == SOCKET_ERROR) {
                // error
                // write to log file
                iRetVal = WSAGetLastError();
                PrintToStatusWindow(L"SCM: WSACleanup() returned SOCKET_ERROR");
            }
            PrintToStatusWindow(L"WM_SESSION_STOP WSACleanup() called");
            ServerComModule(SCMN_DISCONNECTED, NULL, NULL);
            return 0;
        }


        case WM_SEND_DATA:
        {
            //first, retrieve the SCMData
            pSCMData = (SCMDATA*)lParam;

            if (fConnected == FALSE)
                return FALSE;

            memset(szAggSendBuf, 0, iSCMDataSize);

            // copy data to be sent into SCMSendData structure
            memcpy(szAggSendBuf, pSCMData, sizeof(SCMDATA));

            //dwTotalBytesSent = 0;

            // send the data
            /*while(iTotalBytesSent < iSCMDataSize)
            {*/
            // send the buffer contents
            iBytesSent = send(ComSocket, szAggSendBuf, iSCMDataSize, 0);
            if (iBytesSent <= 0) {
                iRetVal = WSAGetLastError();
                return FALSE;
            }
            //iTotalBytesSent += iBytesSent;
        //}
            return TRUE;
        }


        case WM_SOCKET_NOTIFY:
        {
            // wParam indicates which socket the event has been received on.
            // lParam details are as below.
            wEvent = WSAGETSELECTEVENT(lParam);	// LOWORD
            wError = WSAGETSELECTERROR(lParam);	// HIWORD

            switch (wEvent) {
                case FD_CONNECT:
                {
                    if (wError) {
                        ServerComModule(E_CONNFAIL, NULL, NULL);
                        closesocket(ComSocket);
                        ComSocket = INVALID_SOCKET;
                        if (WSACleanup() == SOCKET_ERROR) {
                            // error
                            // write to log file
                            iRetVal = WSAGetLastError();
                            PrintToStatusWindow(L"SCM: FD_CONNECT: WSACleanup() returned SOCKET_ERROR");
                        }
                        PrintToStatusWindow(L"FD_CONNECT WSACleanup() called");
                        return FALSE;
                    }
                    ServerComModule(SCMN_CONNECTED, NULL, NULL);
                    fConnected = TRUE;
                    return 0;
                }

                case FD_READ:
                {
                    if (wError) {
                        iRetVal = WSAGetLastError();
                        return FALSE;
                    }
                    if (iTotalBytesReceived < iCCMDataSize) {
                        iBytesReceived = recv(ComSocket, szRecvBuf, RECV_BUFSIZE, 0);
                        if (iBytesReceived <= 0) {
                            iRetVal = WSAGetLastError();
                            return FALSE;
                        }
                        memcpy(&szAggRecvBuf[iTotalBytesReceived], szRecvBuf, iBytesReceived);
                        iTotalBytesReceived += iBytesReceived;

                        if (iTotalBytesReceived == iCCMDataSize) {
                            iTotalBytesReceived = 0;

                            __try {
                                if ((pCCMDataIn = (CCMDATA*)malloc(sizeof(CCMDATA))) == NULL) {
                                    PrintToStatusWindow(L"malloc() error: pCCMDataIn");
                                    WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlyWndProc", L"malloc() error: pCCMDataIn",
                                        GetLastError());
                                    return 0;
                                }
                                memset(pCCMDataIn, 0, sizeof(CCMDATA));

                                // retrieve data sent by CCM
                                memcpy(pCCMDataIn, szAggRecvBuf, iCCMDataSize);

                                // send the received data to ServerComModule()				
                                ServerComModule(SCMN_DATA_IN, NULL, pCCMDataIn);
                            }
                            __finally {
                                if (pCCMDataIn)
                                    free(pCCMDataIn);
                            }
                        }
                    }
                    return 0;
                }


                case FD_WRITE:
                {
                    if (wError) {
                        iRetVal = WSAGetLastError();
                        return FALSE;
                    }
                    return 0;
                }


                case FD_CLOSE:
                {
                    if (wError == 0)
                        PrintToStatusWindow(L"SCM: Graceful close");
                    else if (wError == WSAECONNRESET)
                        PrintToStatusWindow(L"SCM: Connection reset");
                    else
                        PrintToStatusWindow(L"SCM: Connection terminated");

                    fConnected = FALSE;
                    closesocket(ComSocket);
                    ComSocket = INVALID_SOCKET;
                    WSACleanup();
                    PrintToStatusWindow(L"SCM FD_CLOSE WSACleanup() called");
                    ServerComModule(SCMN_DISCONNECTED, NULL, NULL);
                    return 0;
                }

            }// switch(wEvent)
            return FALSE;	//case WM_SOCKET_NOTIFY
        }


        case WM_FT_SOCKET_NOTIFY:
        {
            // wParam indicates which socket the event has been received on.
            // lParam details are as below.
            wEvent = WSAGETSELECTEVENT(lParam);	// LOWORD
            wError = WSAGETSELECTERROR(lParam);	// HIWORD

            switch (wEvent) {

                case FD_CONNECT:
                {
                    if (wError) {
                        swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"FT: FD_CONNECT error: %d", wError);
                        PrintToStatusWindow(wszSWMsg);
                        WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlywndProc", L"FD_CONNECT error", wError);

                        if (fFTSuccess == FALSE)
                            iFTStatus = FT_ERROR_CONN_FAIL;

                        SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                        return FALSE;
                    }
                    PrintToStatusWindow(L"FT: Connection to ClientFT successful");

                    // create event to handle send() failing with WSAEWOULDBLOCK
                    hWriteReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_WRITE_READY);
                    if (hWriteReadyEvent == NULL) {
                        PrintToStatusWindow(L"FT: WriteReady Event creation error");
                        WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlywndProc", L"WriteReady Event creation error", GetLastError());
                        fFTSuccess = FALSE;
                        SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                    }

                    fFTInProgress = TRUE;

                    // send command FT_INIT
                    SendMessage(hMOWnd, WM_FT_INIT, 0, 0);
                    return TRUE;
                }

                case FD_READ:
                {
                    if (wError) {
                        wsprintf(wszSWMsg, L"FD_READ error: %d", wError);
                        WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlywndProc", L"FD_READ error", GetLastError());
                        PrintToStatusWindow(wszSWMsg);
                        return 0;
                    }

                    if ((iFTReceivedBytes = recv(FTPSocket, szFTAggRecvBuf,
                        iFTPacketSize, MSG_PEEK)) != iFTPacketSize) {
                        // wait until iFTPacketSize amount of data is available in the input stream
                        return 0;
                    }
                    iFTReceivedBytes = recv(FTPSocket, szFTAggRecvBuf, iFTPacketSize, 0);
                    if (iFTReceivedBytes <= 0) {
                        WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlywndProc", L"recv() error", GetLastError());
                        wsprintf(wszSWMsg, L"FT: FD_READ error: %d", WSAGetLastError());
                        PrintToStatusWindow(wszSWMsg);
                        return 0;
                    }
                    //memcpy(&szFTAggRecvBuf[iFTTotalReceivedBytes], szFTRecvBuf, iFTReceivedBytes);
                    //iFTTotalReceivedBytes += iFTReceivedBytes;

                    if (iFTReceivedBytes == iFTPacketSize) {
                        //iFTTotalReceivedBytes = 0;
                        memcpy(&FTPacket, szFTAggRecvBuf, iFTPacketSize);

                        // check packet type
                        switch (FTPacket.iPacketType) {
                            case FILE_MARKER:
                            {
                                // check if it is SOF
                                if (strcmp(FTPacket.szData, RM_SOF) == 0) {
                                    PrintToStatusWindow(L"FT: Received SOF");

                                    // open the destination file
                                    iRetVal = _wsopen_s(&iDestFD, g_FTwszDestPath,
                                        _O_CREAT | _O_BINARY | _O_WRONLY,
                                        _SH_DENYWR,
                                        _S_IREAD | _S_IWRITE);

                                    if (iRetVal != 0) {
                                        fFTSuccess = FALSE;
                                        iFTStatus = FT_ERROR_OPEN_DEST_FILE;
                                        WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlyWndProc", L"FT: Error creating destination file",
                                            iRetVal);
                                        SendMessage(hMOWnd, WM_FT_TERM, 0, 0);

                                        // set FT complete event
                                        SendMessage(hMOWnd, WM_FT_SET_COMPLETE_EVENT, 0, 0);
                                        return 0;
                                    }

                                    // send command FT_START
                                    SendMessage(hMOWnd, WM_FT_START, 0, 0);

                                    return 0;
                                }
                                // check if it is FNF (File Not Found)
                                else if (strcmp(FTPacket.szData, RM_FNF) == 0) {
                                    fFTSuccess = FALSE;
                                    iFTStatus = FT_ERROR_FNF;
                                    PrintToStatusWindow(L"FT: File not found on client machine");
                                    SendMessage(hMOWnd, WM_FT_TERM, 0, 0);

                                    // set FT complete event
                                    SendMessage(hMOWnd, WM_FT_SET_COMPLETE_EVENT, 0, 0);

                                    return 0;
                                }
                                // check if it is EOF
                                else if (strcmp(FTPacket.szData, RM_EOF) == 0) {
                                    PrintToStatusWindow(L"FT: Received EOF");
                                    wsprintf(wszSWMsg, L"FT: Filesize: %d", iFileSize);
                                    PrintToStatusWindow(wszSWMsg);
                                    if (iDestFD > 0) {
                                        _close(iDestFD);
                                        iDestFD = 0;
                                    }

                                    // send command FT_SEND_CHECKSUM
                                    SendMessage(hMOWnd, WM_FT_SEND_CHECKSUM, 0, 0);

                                    return 0;
                                }
                                return 0;
                            }

                            case FILE_DATA:
                            {
                                // file contents has file data, write to file
                                iBytesWritten = _write(iDestFD, FTPacket.szData,
                                    FTPacket.iDataSize);
                                if (iBytesWritten == -1) {
                                    fFTSuccess = FALSE;
                                    iFTStatus = FT_ERROR_WRITE_TO_FILE;
                                    PrintToStatusWindow(L"FT: Error writing to destination file");
                                    WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlywndProc", L"error writing to destination file", GetLastError());
                                    SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                                } else
                                    iFileSize += iBytesWritten;

                                return 0;
                            }

                            case FILE_CHECKSUM:
                            {
                                // copy the FTPacket.szData contents to the CHECKSUM structure
                                memcpy(&RecvChecksum, &FTPacket.szData, FTPacket.iDataSize);

                                // compute checksum
                                if ((iRetVal = ComputeChecksum(g_FTwszDestPath, &NewChecksum)) < 0) {
                                    // assume file corruption
                                    fFTSuccess = FALSE;
                                    iFTStatus = FT_ERROR_FILE_CORRUPTED;
                                    PrintToStatusWindow(L"FT: Checksum could not be computed");
                                } else {
                                    // verify the checksums
                                    if ((iRetVal = VerifyChecksums(&RecvChecksum, &NewChecksum)) < 0) {
                                        PrintToStatusWindow(L"FT: File corrupted");
                                        fFTSuccess = FALSE;
                                        iFTStatus = FT_ERROR_FILE_CORRUPTED;
                                    } else {
                                        PrintToStatusWindow(L"FT: Checksum verified. File transfer successful.");
                                        fFTSuccess = TRUE;
                                    }
                                }

                                // send ACK
                                SendMessage(hMOWnd, WM_FT_SEND_ACK, 0, 0);

                                // terminate the transfer
                                SendMessage(hMOWnd, WM_FT_TERM, 0, 0);

                                return 0;
                            }
                        }// switch(FTPacket.iPacketType)
                        return 0;
                    } else {
                        PrintToStatusWindow(L"FT: recv() return size unexpected!");
                        return 0;
                    }
                }

                case FD_WRITE:
                {
                    if (wError) {
                        wsprintf(wszSWMsg, L"FT: FD_WRITE error: %d", wError);
                        WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlywndProc", L"FD_WRITE error", GetLastError());
                        PrintToStatusWindow(wszSWMsg);
                        return FALSE;
                    }
                    SetEvent(hWriteReadyEvent);
                    return 0;
                }


                case FD_CLOSE:
                {
                    if (wError == 0)
                        PrintToStatusWindow(L"FT: Graceful close");
                    else if (wError == WSAECONNRESET)
                        PrintToStatusWindow(L"FT: Connection reset");
                    else {
                        swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"FD_CLOSE error %d", wError);
                        PrintToStatusWindow(wszSWMsg);
                    }
                    if (fFTSuccess == FALSE)
                        iFTStatus = FT_ERROR_CONN_CLOSED;

                    SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                    PrintToStatusWindow(L"FT: CCM_FT connection closed.");
                    return 0;
                }

            }// switch(wEvent)
            return 0;
        }

        case WM_FT_INIT:
        {
            if (!fFTInProgress)
                return 0;

            // send FT_INIT command to client FT module
            FTPacket.cmd = FT_INIT;
            FTPacket.iPacketType = COMMAND;
            FTPacket.iDataSize = 0;
            strcpy_s(FTPacket.szData, FT_BUFSIZE + 1, "");
            memcpy(szFTSendBuf, &FTPacket, iFTPacketSize);

            ResetEvent(hWriteReadyEvent);

            // send the buffer contents
            while (TRUE) {
                iFTSentBytes = send(FTPSocket, szFTSendBuf, iFTPacketSize, 0);
                if ((iRetVal = WSAGetLastError()) == WSAEWOULDBLOCK) {
                    PrintToStatusWindow(L"FT: send() failed with WSAEWOULDBLOCK, waiting...");
                    dwEventRetVal = WaitForSingleObject(hWriteReadyEvent, INFINITE);
                    if (dwEventRetVal == WAIT_OBJECT_0)
                        ResetEvent(hWriteReadyEvent);
                    continue;
                }

                if (iFTSentBytes <= 0) {
                    iRetVal = GetLastError();
                    WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlywndProc", L"send() failed", GetLastError());

                    // terminate transfer
                    SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                    PrintToStatusWindow(L"FT: WM_FT_INIT: send() failed");
                    return FALSE;
                }

                break;

            }//while(TRUE)

            return 0;
        }

        case WM_FT_START:
        {
            if (!fFTInProgress)
                return 0;

            // send FT_START command to client FT module
            FTPacket.cmd = FT_START;
            FTPacket.iPacketType = COMMAND;
            FTPacket.iDataSize = 0;
            strcpy_s(FTPacket.szData, FT_BUFSIZE + 1, "");
            memcpy(szFTSendBuf, &FTPacket, iFTPacketSize);

            ResetEvent(hWriteReadyEvent);

            // send the buffer contents
            while (TRUE) {
                iFTSentBytes = send(FTPSocket, szFTSendBuf, iFTPacketSize, 0);
                if ((iRetVal = WSAGetLastError()) == WSAEWOULDBLOCK) {
                    PrintToStatusWindow(L"FT: send() failed with WSAEWOULDBLOCK, waiting...");
                    dwEventRetVal = WaitForSingleObject(hWriteReadyEvent, INFINITE);
                    if (dwEventRetVal == WAIT_OBJECT_0)
                        ResetEvent(hWriteReadyEvent);
                    continue;
                }

                if (iFTSentBytes <= 0) {
                    WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlywndProc", L"send() failed", GetLastError());

                    // terminate transfer
                    SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                    PrintToStatusWindow(L"FT: WM_FT_START: send() failed");
                    return FALSE;
                }

                break;
            }// while(TRUE)

            return 0;
        }

        case WM_FT_SEND_CHECKSUM:
        {
            if (!fFTInProgress)
                return 0;

            // send FT_SEND_CHECKSUM command to client FT module
            FTPacket.cmd = FT_SEND_CHECKSUM;
            FTPacket.iPacketType = COMMAND;
            FTPacket.iDataSize = 0;
            strcpy_s(FTPacket.szData, FT_BUFSIZE + 1, "");
            memcpy(szFTSendBuf, &FTPacket, iFTPacketSize);

            ResetEvent(hWriteReadyEvent);

            // send the buffer contents
            while (TRUE) {
                iFTSentBytes = send(FTPSocket, szFTSendBuf, iFTPacketSize, 0);
                if ((iRetVal = WSAGetLastError()) == WSAEWOULDBLOCK) {
                    PrintToStatusWindow(L"FT: send() failed with WSAEWOULDBLOCK, waiting...");
                    dwEventRetVal = WaitForSingleObject(hWriteReadyEvent, INFINITE);
                    if (dwEventRetVal == WAIT_OBJECT_0)
                        ResetEvent(hWriteReadyEvent);
                    continue;
                }

                if (iFTSentBytes <= 0) {
                    WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlywndProc", L"send() failed", GetLastError());

                    // terminate transfer
                    SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                    PrintToStatusWindow(L"FT: WM_FT_SEND_CHECKSUM: send() failed");
                    return 0;
                }

                break;

            }// while(TRUE)

            return 0;
        }

        case WM_FT_SEND_ACK:
        {
            if (!fFTInProgress)
                return 0;

            // send status notification to client FT module
            if (fFTSuccess) {
                if (fFTDeleteFile)
                    FTPacket.cmd = FT_ERROR_SUCCESS_DELETE;
                else
                    FTPacket.cmd = FT_ERROR_SUCCESS_NO_DELETE;
            } else
                FTPacket.cmd = FT_ERROR_FILE_CORRUPTED;

            FTPacket.iPacketType = NOTIFICATION;
            FTPacket.iDataSize = 0;
            strcpy_s(FTPacket.szData, FT_BUFSIZE + 1, "");
            memcpy(szFTSendBuf, &FTPacket, iFTPacketSize);

            ResetEvent(hWriteReadyEvent);

            // send the buffer contents
            while (TRUE) {
                iFTSentBytes = send(FTPSocket, szFTSendBuf, iFTPacketSize, 0);
                if ((iRetVal = WSAGetLastError()) == WSAEWOULDBLOCK) {
                    PrintToStatusWindow(L"FT: send() failed with WSAEWOULDBLOCK, waiting...");
                    dwEventRetVal = WaitForSingleObject(hWriteReadyEvent, INFINITE);
                    if (dwEventRetVal == WAIT_OBJECT_0)
                        ResetEvent(hWriteReadyEvent);
                    continue;
                }

                if (iFTSentBytes <= 0) {
                    WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlywndProc", L"send() failed", GetLastError());

                    // terminate transfer
                    SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                    PrintToStatusWindow(L"FT: WM_FT_SEND_CHECKSUM: send() failed");
                    return 0;
                }

                break;

            }// while(TRUE)

            return 0;
        }

        case WM_FT_TERM:
        {
            fFTInProgress = FALSE;

            // reset static data
            iFileSize /*= iFTTotalSentBytes*/ = iFTTotalReceivedBytes = 0;
            hWriteReadyEvent = NULL;
            memset(szFTAggRecvBuf, 0, sizeof(szFTAggRecvBuf));
            if (iDestFD > 0) {
                _close(iDestFD);
                iDestFD = 0;
            }

            Sleep(1000);

            closesocket(FTPSocket);
            FTPSocket = INVALID_SOCKET;
            WSACleanup();
            PrintToStatusWindow(L"WM_FT_TERM WSACleanup() called");


            // set FTcomplete event
            // obtain handle to the FTcomplete event
            HANDLE hFTCompleteEvent = CreateEvent(NULL,	// default security attributes
                TRUE,	// manual reset using ResetEvent()
                FALSE,	// non-signaled initial state
                EVENT_FT_COMPLETE);	// name of the event object

            if (hFTCompleteEvent == NULL) {
                WriteToLogFile(FP_SERVER_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlywndProc", L"FT: FTCompleteEvent handle error", GetLastError());
                PrintToStatusWindow(L"FT: FTCompleteEvent handle error");
                return FALSE;
            } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
                // set the event
                SetEvent(hFTCompleteEvent);
                CloseHandle(hFTCompleteEvent);
            }

            return 0;
        }


    }// switch(message)
    return DefWindowProc(hMOWnd, message, wParam, lParam);

}// MsgOnlyWndProc()