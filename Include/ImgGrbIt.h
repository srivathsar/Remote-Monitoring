
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <io.h>
#include <wchar.h>
#include <share.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <errno.h>
#include "..\..\..\Include\RMDefs.h"
#include "..\..\..\Include\DataStructures.h"

#define SGCONFIGUPDATE		L"SGConfigUpdate"

extern "C" __declspec(dllexport) BOOL ProcessCmd(RM_COMMAND Cmd, MNTRDATA *MntrData, PDATA __out **pPDataToCPM);

typedef BOOL(__cdecl *INSTALLPROC)(WCHAR *pszSGDir);
typedef BOOL(__cdecl *REMOVEPROC)();
