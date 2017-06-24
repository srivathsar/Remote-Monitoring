
// ** Remote Monitoring **

// Library.h : Header file for the Library.cpp source file

// Functions available in Library.cpp
BOOL	PrintToStatusWindow(const WCHAR *msg);
BOOL	PrintToStatusBar(const WCHAR *wszMsg, int iPart);
int		WriteToLogFile(const WCHAR *pwszFilePath, const WCHAR *pwszFromModule, const WCHAR *pwszFunction, 
						const WCHAR *pwszData, int iError);
int		ListDir(const WCHAR *pwszDirPath, const WCHAR *pwszExt, FILELIST flFileList[], int nMaxFiles);