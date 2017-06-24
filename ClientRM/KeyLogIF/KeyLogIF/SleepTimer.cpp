//** Running in the SleepTimer Thread.

#include "..\..\..\Include\Keylog.h"

extern	CRITICAL_SECTION	csSleepTimer;
extern	BOOL				fExitKLLogFileUpdate;
extern	DWORD				dwSleepTimerVal;
extern	HANDLE				hUpdateLogFileEvent;

DWORD WINAPI SleepTimer() {
    BOOL dwSleepTimerValCopy = FALSE;

    while (fExitKLLogFileUpdate != TRUE) {
        EnterCriticalSection(&csSleepTimer);

        dwSleepTimerValCopy = dwSleepTimerVal;

        LeaveCriticalSection(&csSleepTimer);

        Sleep(dwSleepTimerValCopy);
        SetEvent(hUpdateLogFileEvent);
    }

    return TRUE;
}
