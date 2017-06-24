
// ** ClientRM project **
// Includes.h : Contains all the #includes required for ClientRM.exe

#include <windows.h>	
#include <psapi.h>
#include <commctrl.h>	// InitCommControls()
#include <stdio.h>		// _wfopen_s(), fclose(), fprintf(), fscanf()
#include <stdlib.h>		// MAX_PATH
#include <tchar.h> 
#include <wchar.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>			// _write
#include <share.h>

#include "..\..\..\Include\RMDefs.h"
#include "..\..\..\Include\DataStructures.h"
#include "..\..\..\Include\SHA1.h"
#include "..\..\..\Include\Library.h"
#include "ClientRM.h"
#include "resource.h"