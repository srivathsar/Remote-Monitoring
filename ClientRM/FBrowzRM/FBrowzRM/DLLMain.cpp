
// ** FBrowzRM project **
// DLLMain.cpp : 

#include "FBIncludes.h"


// Global Variables
HINSTANCE	g_hInstance;


// DllMain()
// --
BOOL WINAPI DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason) {

        case DLL_PROCESS_ATTACH:
        {
            g_hInstance = hInst;
            return TRUE;			// indicate success
        }

        case DLL_THREAD_ATTACH:
        {

            return TRUE;
        }

        case DLL_PROCESS_DETACH:
        {
            return TRUE;
        }

        case DLL_THREAD_DETACH:
        {
            return TRUE;
        }
    }
}
