
// * ClientRM project *
// MsgOnlyWndProc.cpp: Window proedure for the message only window

#include "ClientInclude.h"

extern WCHAR g_wszSrcFilePath[];

// static variables
static int iCCMDataSize = sizeof(CCMDATA);
static int iSCMDataSize = sizeof(SCMDATA);
static int iFTPacketSize = sizeof(FTPACKET);

static BOOL fFTInProgress = FALSE;

LRESULT CALLBACK MsgOnlyWndProc(HWND hMOWnd, UINT message, WPARAM wParam, LPARAM lParam) {

    extern SOCKET ListenSocket;

    int iRetVal;
    WORD wEvent, wError;
    static SOCKET SCMSocket = INVALID_SOCKET;

    int iBytesSent;
    static int iTotalBytesSent;
    //	char szSendBuf[SENDBUF_SIZE + 1];
    static char szAggSendBuf[sizeof(CCMDATA) + 1];

    int iBytesReceived;
    static int iTotalBytesReceived;
    static char szRecvBuf[RECV_BUFSIZE + 1];
    static char szAggRecvBuf[sizeof(SCMDATA) + 1];

    CCMDATA *pCCMDataOut;
    SCMDATA *pSCMDataIn = NULL;

    WCHAR wszSWMsg[MAX_SW_MSG + 1];

    // File Transfer communication
    extern SOCKET FTListenSocket;
    static SOCKET FTPSocket = INVALID_SOCKET;

    int			iFTSentBytes;
    static int	iFTTotalSentBytes;
    char szFTSendBuf[sizeof(FTPACKET) + 1];

    int			iFTReceivedBytes;
    static int	iFTTotalReceivedBytes = 0;
    //char szFTRecvBuf[FT_BUFSIZE + 1];
    static char szFTAggRecvBuf[sizeof(FTPACKET) + 1];

    static FTPACKET FTPacket;

    // SourceFile management
    int				iFTBytesRead;
    static int		iFileSize;
    static int		iSrcFD = NULL;
    CHECKSUM		FileChecksum;

    static HANDLE hWriteReadyEvent = NULL;
    DWORD	dwEventRetVal;


    switch (message) {

        case WM_CLOSE:
        {
            if (fFTInProgress == TRUE)
                SendMessage(hMOWnd, WM_FT_TERM, 0, 0);

            closesocket(SCMSocket);
            SCMSocket = INVALID_SOCKET;
            closesocket(ListenSocket);
            ListenSocket = INVALID_SOCKET;
            WSACleanup();
            PrintToStatusWindow(L"WSACleanup() called");

            PostQuitMessage(0);
            return 0;
        }


        case WM_SEND_DATA:
        {
            //first, retrieve the SCMData
            if (lParam != NULL)
                pCCMDataOut = (CCMDATA*)lParam;

            memset(szAggSendBuf, 0, iCCMDataSize);

            // copy data to be sent into send buffer structure
            memcpy(szAggSendBuf, pCCMDataOut, sizeof(CCMDATA));

            // send the buffer contents
            iBytesSent = send(SCMSocket, szAggSendBuf, iCCMDataSize, 0);
            if (iBytesSent <= 0) {
                iRetVal = WSAGetLastError();
                return FALSE;
            }
            //dwTotalBytesSent += iBytesSent;
            return TRUE;
        }


        case WM_SOCKET_NOTIFY:
        {
            // wParam indicates which socket the event has been received on.
            // lParam details are as below.
            wEvent = WSAGETSELECTEVENT(lParam);	// LOWORD
            wError = WSAGETSELECTERROR(lParam);	// HIWORD

            switch (wEvent) {
                case FD_ACCEPT:
                {
                    struct sockaddr_in saFrom;
                    int iLen;
                    iLen = sizeof(saFrom);

                    // accept connection from SCM
                    SCMSocket = accept(ListenSocket, (struct sockaddr*)&saFrom, &iLen);
                    swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"Connection from %d accepted", saFrom.sin_addr.S_un.S_addr);
                    PrintToStatusWindow(wszSWMsg);
                    return 0;
                }

                case FD_READ:
                {
                    if (wError) {
                        iRetVal = WSAGetLastError();
                        PrintToStatusWindow(L"CCM: FD_READ error");
                        return 0;
                    }
                    if (iTotalBytesReceived < iSCMDataSize) {
                        iBytesReceived = recv(SCMSocket, szRecvBuf, RECV_BUFSIZE, 0);
                        if (iBytesReceived <= 0) {
                            iRetVal = WSAGetLastError();
                            return FALSE;
                        }
                        memcpy(&szAggRecvBuf[iTotalBytesReceived], szRecvBuf, iBytesReceived);
                        iTotalBytesReceived += iBytesReceived;

                        if (iTotalBytesReceived == iSCMDataSize) {
                            iTotalBytesReceived = 0;

                            __try {
                                if ((pSCMDataIn = (SCMDATA*)malloc(sizeof(SCMDATA))) == NULL) {
                                    PrintToStatusWindow(L"malloc() error: pCCMDataIn");
                                    WriteToLogFile(FP_CLIENT_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlyWndProc", L"malloc() error: pSCMDataIn",
                                        GetLastError());
                                    return 0;
                                }
                                memset(pSCMDataIn, 0, sizeof(SCMDATA));

                                // copy data sent by SCM
                                memcpy(pSCMDataIn, szAggRecvBuf, iSCMDataSize);
                                ClientComModule(CCMN_DATA_IN, NULL, pSCMDataIn);
                                return 0;
                            }
                            __finally {
                                if (pSCMDataIn)
                                    free(pSCMDataIn);
                            }
                        }
                    }
                    return 0;
                }

                case FD_WRITE:
                {
                    if (wError) {
                        iRetVal = WSAGetLastError();
                        PrintToStatusWindow(L"CCM: FD_WRITE error");
                        return 0;
                    }
                    return 0;
                }

                case FD_CLOSE:
                {
                    if (wError == 0)
                        PrintToStatusWindow(L"CCM: Graceful close");
                    else if (wError == WSAECONNRESET)
                        PrintToStatusWindow(L"CCM: Connection reset");
                    else
                        PrintToStatusWindow(L"CCM: Waiting for further communication...");

                    // clear static data
                    iTotalBytesSent = iTotalBytesReceived = 0;
                    memset(szRecvBuf, 0, sizeof(szRecvBuf));
                    memset(szAggRecvBuf, 0, sizeof(szAggRecvBuf));
                    memset(szAggSendBuf, 0, sizeof(szAggSendBuf));

                    closesocket(SCMSocket);
                    SCMSocket = INVALID_SOCKET;
                    return 0;
                }

            }// switch(wEvent)
            return 0;
        }

        case WM_FT_SOCKET_NOTIFY:
        {
            // wParam indicates which socket the event has been received on.
            // lParam details are as below.
            wEvent = WSAGETSELECTEVENT(lParam);	// LOWORD
            wError = WSAGETSELECTERROR(lParam);	// HIWORD

            switch (wEvent) {
                case FD_ACCEPT:
                {
                    struct sockaddr_in saFrom;
                    int iLen;
                    iLen = sizeof(saFrom);

                    if (wError) {
                        iRetVal = WSAGetLastError();
                        PrintToStatusWindow(L"FD_ACCEPT error");
                        return FALSE;
                    }

                    // accept connection from Server for file transfer
                    if ((FTPSocket = accept(FTListenSocket, (struct sockaddr*)&saFrom, &iLen)) == INVALID_SOCKET) {
                        swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"FT Connection from %d rejected", saFrom.sin_addr.S_un.S_addr);
                        PrintToStatusWindow(wszSWMsg);
                        return 0;
                    }

                    swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"FT Connection from %d accepted", saFrom.sin_addr.S_un.S_addr);
                    PrintToStatusWindow(wszSWMsg);

                    // create event to handle send() failing with WSAEWOULDBLOCK
                    hWriteReadyEvent = CreateEvent(NULL, TRUE, FALSE, EVENT_WRITE_READY);
                    if (hWriteReadyEvent == NULL) {
                        PrintToStatusWindow(L"FT: WriteReady Event creation error");
                    }

                    fFTInProgress = TRUE;
                    return 0;
                }

                case FD_READ:
                {
                    if (wError) {
                        iRetVal = WSAGetLastError();
                        PrintToStatusWindow(L"FD_READ error");
                        return FALSE;
                    }

                    if ((iFTReceivedBytes = recv(FTPSocket, szFTAggRecvBuf,
                        iFTPacketSize, MSG_PEEK)) != iFTPacketSize) {
                        // wait until iFTPacketSize amount of data is available in the input stream
                        return 0;
                    }
                    iFTReceivedBytes = recv(FTPSocket, szFTAggRecvBuf, iFTPacketSize, 0);
                    if (iFTReceivedBytes <= 0) {
                        iRetVal = WSAGetLastError();
                        PrintToStatusWindow(L"FT: FD_READ error");
                        return FALSE;
                    }
                    //memcpy(&szFTAggRecvBuf[iFTTotalReceivedBytes], szFTRecvBuf, iFTReceivedBytes);
                    //iFTTotalReceivedBytes += iFTReceivedBytes;

                    if (iFTReceivedBytes == iFTPacketSize) {
                        //iFTTotalReceivedBytes = 0;
                        memcpy(&FTPacket, szFTAggRecvBuf, iFTPacketSize);

                        if (FTPacket.iPacketType == COMMAND) {
                            switch (FTPacket.cmd) {
                                case FT_INIT:
                                {
                                    // send SOF/FNF
                                    SendMessage(hMOWnd, WM_FT_INIT, 0, 0);
                                    return 0;
                                }

                                case FT_START:
                                {
                                    // start actual file transfer
                                    SendMessage(hMOWnd, WM_FT_START, 0, 0);
                                    return 0;
                                }

                                case FT_SEND_CHECKSUM:
                                {
                                    // send checksum
                                    SendMessage(hMOWnd, WM_FT_SEND_CHECKSUM, 0, 0);
                                    return 0;
                                }

                                default:
                                {
                                    PrintToStatusWindow(L"FT: Invalid command from server");
                                    return 0;
                                }
                            }// switch(FTPacket.cmd)
                        } else if (FTPacket.iPacketType == NOTIFICATION) {
                            switch (FTPacket.cmd) {
                                case FT_ERROR_SUCCESS_DELETE:
                                {
                                    if (DeleteFile(g_wszSrcFilePath) == 0) {
                                        iRetVal = GetLastError();
                                        swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"FT: DeleteFile error %s", g_wszSrcFilePath);
                                        WriteToLogFile(FP_CLIENT_LOGFILE, L"MsgOnlyWndProc", L"MsgOnlyWndProc", wszSWMsg, iRetVal);
                                        break;
                                    }
                                }
                            }
                            return 0;
                        }
                    }
                    return 0;
                }

                case FD_WRITE:
                {
                    if (wError) {
                        PrintToStatusWindow(L"FT: FD_WRITE error");
                        return FALSE;
                    }

                    iRetVal = SetEvent(hWriteReadyEvent);
                    if (iRetVal == 0)
                        PrintToStatusWindow(L"FT: FD_WRITE SetEvent failed");
                    else
                        PrintToStatusWindow(L"FT: FD_WRITE SetEvent succeeded");
                    return 0;
                }


                case FD_CLOSE:
                {
                    if (wError == 0)
                        PrintToStatusWindow(L"FT: Graceful close");
                    else if (wError == WSAECONNRESET)
                        PrintToStatusWindow(L"FT: Connection reset");
                    else {
                        swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"FD_CLOSE wError: %d", wError);
                        PrintToStatusWindow(wszSWMsg);
                    }

                    SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                    return 0;
                }

            }// switch(wEvent)
            return 0;

        }// case WM_FT_SOCKET_NOTIFY

        case WM_FT_INIT:
        {
            // open source file
            if (iSrcFD == 0) {
                iRetVal = _wsopen_s(&iSrcFD, g_wszSrcFilePath, _O_RDONLY | _O_BINARY,
                    _SH_DENYWR, _S_IREAD | _S_IWRITE);
                if (iRetVal != 0) {
                    iSrcFD = 0;
                    iRetVal = GetLastError();
                    PrintToStatusWindow(L"FT: FileNotFound");
                    sprintf_s(FTPacket.szData, sizeof(FTPacket.szData), RM_FNF);
                    swprintf_s(wszSWMsg, L"FT: RM_FNF Sent");
                } else {
                    PrintToStatusWindow(L"FT: FileFound");
                    sprintf_s(FTPacket.szData, sizeof(FTPacket.szData), RM_SOF);
                    swprintf_s(wszSWMsg, L"FT: RM_SOF Sent");
                }

                // send RM_FNF/RM_SOF
                memset(&szFTSendBuf, 0, sizeof(szFTSendBuf));

                FTPacket.iPacketType = FILE_MARKER;
                FTPacket.cmd = 0;
                FTPacket.iDataSize = strlen(FTPacket.szData);
                memcpy(szFTSendBuf, &FTPacket, iFTPacketSize);

                ResetEvent(hWriteReadyEvent);

                // send the buffer contents
                while (TRUE) {
                    if (fFTInProgress == FALSE)
                        return 0;

                    iFTSentBytes = send(FTPSocket, szFTSendBuf, iFTPacketSize, 0);
                    if ((iRetVal = WSAGetLastError()) == WSAEWOULDBLOCK) {
                        PrintToStatusWindow(L"FT: send() failed with WSAEWOULDBLOCK, waiting...");
                        dwEventRetVal = WaitForSingleObject(hWriteReadyEvent, 2000);
                        if (dwEventRetVal == WAIT_OBJECT_0)
                            ResetEvent(hWriteReadyEvent);
                        continue;
                    }

                    if (iFTSentBytes <= 0) {
                        iRetVal = GetLastError();

                        // terminate transfer
                        SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                        PrintToStatusWindow(L"FT: WM_FT_INIT: send() failed");
                        return FALSE;
                    }

                    break;
                }
                PrintToStatusWindow(wszSWMsg);
            }
            return 0;
        }

        case WM_FT_START:
        {
            if (fFTInProgress == FALSE)
                return 0;

            PrintToStatusWindow(L"FT: File transfer started");

            // empty the send buffer
            memset(FTPacket.szData, 0, sizeof(FTPACKET));

            ResetEvent(hWriteReadyEvent);

            // start sending file data
            while ((iFTBytesRead = _read(iSrcFD, FTPacket.szData, FT_BUFSIZE)) != 0) {
                iFileSize += iFTBytesRead;

                // set the packet type
                FTPacket.iDataSize = iFTBytesRead;
                FTPacket.cmd = 0;
                FTPacket.iPacketType = FILE_DATA;

                // copy filecontents structure into send buffer
                memset(szFTSendBuf, 0, sizeof(szFTSendBuf));
                memcpy(szFTSendBuf, &FTPacket, iFTPacketSize);

                // send buffer contents
                while (TRUE) {
                    if (fFTInProgress == FALSE)
                        return 0;

                    iFTSentBytes = send(FTPSocket, szFTSendBuf, iFTPacketSize, 0);
                    if ((iRetVal = WSAGetLastError()) == WSAEWOULDBLOCK) {
                        PrintToStatusWindow(L"FT: send() failed with WSAEWOULDBLOCK, waiting...");
                        dwEventRetVal = WaitForSingleObject(hWriteReadyEvent, 2000);
                        if (dwEventRetVal == WAIT_OBJECT_0)
                            ResetEvent(hWriteReadyEvent);
                        continue;
                    }
                    if (iFTSentBytes <= 0) {
                        iRetVal = WSAGetLastError();

                        // terminate transfer
                        SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                        PrintToStatusWindow(L"FT: WM_FT_START: send() failed");
                        return 0;
                    }
                    iFTTotalSentBytes += iFTSentBytes;
                    if (iFTTotalSentBytes % (iFTSentBytes * 10) == 0) {
                        swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"FT: %d bytes sent", iFTTotalSentBytes);
                        PrintToStatusWindow(wszSWMsg);
                    }
                    Sleep(1);

                    break;
                }
                memset(&FTPacket, 0, sizeof(FTPACKET));

            }// while(fread())

            if (iSrcFD > 0) {
                _close(iSrcFD);
                iSrcFD = 0;
                PrintToStatusWindow(L"WM_FT_START File Desc closed");
            }

            // send RM_EOF
            memset(szFTSendBuf, 0, sizeof(szFTSendBuf));
            FTPacket.iPacketType = FILE_MARKER;
            FTPacket.cmd = 0;
            FTPacket.iDataSize = strlen(RM_EOF);
            strcpy_s(FTPacket.szData, sizeof(FTPacket.szData), RM_EOF);
            memcpy(szFTSendBuf, &FTPacket, iFTPacketSize);

            ResetEvent(hWriteReadyEvent);

            while (TRUE) {
                if (fFTInProgress == FALSE)
                    return 0;

                iFTSentBytes = send(FTPSocket, szFTSendBuf, iFTPacketSize, 0);
                if ((iRetVal = WSAGetLastError()) == WSAEWOULDBLOCK) {
                    PrintToStatusWindow(L"FT: send() failed with WSAEWOULDBLOCK, waiting...");
                    dwEventRetVal = WaitForSingleObject(hWriteReadyEvent, 2000);
                    if (dwEventRetVal == WAIT_OBJECT_0)
                        ResetEvent(hWriteReadyEvent);
                    continue;
                }
                if (iFTSentBytes <= 0) {
                    iRetVal = WSAGetLastError();

                    // terminate transfer
                    SendMessage(hMOWnd, WM_FT_TERM, 0, 0);
                    PrintToStatusWindow(L"FT: WM_FT_TERM: send() failed");
                    return 0;
                }

                PrintToStatusWindow(L"FT: RM_EOF sent");
                swprintf_s(wszSWMsg, MAX_SW_MSG + 1, L"FT: Filesize: %d", iFileSize);
                PrintToStatusWindow(wszSWMsg);
                break;
            }
            return 0;
        }


        case WM_FT_SEND_CHECKSUM:
        {
            if (fFTInProgress == FALSE)
                return 0;

            PrintToStatusWindow(L"FT: Sending CHECKSUM");

            // compute checksum
            iRetVal = ComputeChecksum(g_wszSrcFilePath, &FileChecksum);	// error checking to be done

            // fill the FTPACKET structure
            FTPacket.cmd = 0;
            FTPacket.iPacketType = FILE_CHECKSUM;
            FTPacket.iDataSize = sizeof(CHECKSUM);
            memcpy(&FTPacket.szData, &FileChecksum, sizeof(CHECKSUM));

            // copy the checksum into FTPACKET structure
            memset(szFTSendBuf, 0, sizeof(szFTSendBuf));
            memcpy(szFTSendBuf, &FTPacket, iFTPacketSize);

            ResetEvent(hWriteReadyEvent);

            while (TRUE) {
                if (fFTInProgress == FALSE)
                    return 0;

                iFTSentBytes = send(FTPSocket, szFTSendBuf, iFTPacketSize, 0);
                if ((iRetVal = WSAGetLastError()) == WSAEWOULDBLOCK) {
                    PrintToStatusWindow(L"FT: send() failed with WSAEWOULDBLOCK, waiting...");
                    dwEventRetVal = WaitForSingleObject(hWriteReadyEvent, 2000);
                    if (dwEventRetVal == WAIT_OBJECT_0)
                        ResetEvent(hWriteReadyEvent);
                    continue;
                }
                if (iFTSentBytes <= 0) {
                    iRetVal = WSAGetLastError();

                    PrintToStatusWindow(L"FT: WM_FT_SEND_CHECKSUM: send() failed");
                    return 0;
                }

                PrintToStatusWindow(L"FT: Checksum sent");
                break;
            }
            return 0;
        }

        case WM_FT_TERM:
        {
            // reset static data
            iFileSize = iFTTotalSentBytes = iFTTotalReceivedBytes = 0;
            CloseHandle(hWriteReadyEvent);
            hWriteReadyEvent = NULL;
            memset(szFTAggRecvBuf, 0, sizeof(szFTAggRecvBuf));
            if (iSrcFD > 0) {
                _close(iSrcFD);
                iSrcFD = 0;
                PrintToStatusWindow(L"WM_FT_TERM File Desc closed");
            }

            fFTInProgress = FALSE;

            closesocket(FTPSocket);
            closesocket(FTListenSocket);
            FTPSocket = FTListenSocket = INVALID_SOCKET;
            if (WSACleanup() == SOCKET_ERROR)
                PrintToStatusWindow(L"FT: WM_FT_TERM: WSACleanup() returned SOCKET_ERROR");
            PrintToStatusWindow(L"WM_FT_TERM: WSACleanup() called");

            return 0;
        }

    }// switch(message)

    return DefWindowProc(hMOWnd, message, wParam, lParam);

}