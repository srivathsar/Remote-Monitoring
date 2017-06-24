
// ** ServerRM project **
// Includes.h : Contains all the #includes required for ServerRM.exe

#include <windows.h>	
#include <commctrl.h>	// InitCommControls()
#include <stdio.h>		// _wfopen_s(), fclose(), fprintf(), fscanf()
#include <stdlib.h>		// MAX_PATH , errno
#include <ERRNO.H>		
#include <tchar.h> 
#include <wchar.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>			// _write
#include <setjmp.h>
#include <share.h>

#include "..\..\Include\RMDefs.h"
#include "..\..\Include\DataStructures.h"
#include "..\..\Include\SHA1.h"
#include "..\..\Include\Library.h"
#include "ServerRM.h"
#include "ResServerRM.h"

//#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")