
// * ClientRM project *
// ClientRM.h: Main header file for the ClientRM exe

// Function Pointer to DLL interface entry point
typedef BOOL(__cdecl *PROCESSCMD)(RM_COMMAND, MNTRDATA*, PDATA __out **pPData);

// ** Private Data Structures **

typedef struct tagDDLInfo {
    HMODULE		hPluginDLL;
    PROCESSCMD	hProcessCmd;
}DLLINFO;

// Data structures to store arguments to ClientComModule()
typedef struct tagCCMArgs {
    RM_COMMAND	cmd;
    CCMDATATYPE	CCMDataType;
}CCMARGS;


// ** Function Prototypes **

int WINAPI			WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow);
LRESULT CALLBACK	WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK	MsgOnlyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
DWORD WINAPI		InitThread(LPVOID lpParam);
DWORD WINAPI		DataReadyEventThread(LPVOID lpArgs);

BOOL ClientPluginManager(RM_COMMAND cmd, const CPMARGS *CPMArgs, const SPMDATA *pSPMDataIn);
BOOL ClientComModule(RM_COMMAND cmd, const CCMARGS *CCMArgs, const SCMDATA *SCMDataIn);


// Helpers
BOOL	TransferFile(const WCHAR *pwszSourcepath);
BOOL	GetNextDate(WORD *wDate, WORD *wMonth, WORD *wYear);
int		ComputeStructSizes();