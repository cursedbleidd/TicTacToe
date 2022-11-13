/*
 * Dependencies:
 *  gdi32
 *  (kernel32)
 *  user32
 *  (comctl32)
 *
 */

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include <windows.h>
#include <wingdi.h>
#include <winuser.h>
#include <conio.h>
#pragma comment(lib, "Msimg32.lib")

#define KEY_SHIFTED     0x8000
#define KEY_TOGGLED     0x0001

const TCHAR szWinClass[] = _T("Tic Tac Toe");
const TCHAR szWinName[] = _T("TicTacToe");
const TCHAR szField[] = _T("Local\\field");
HWND hwnd;               /* This is the handle for our window */
HBRUSH hBrush;           /* Current brush */
HANDLE bckGrnThread;
HANDLE PaintThread;

HDC hdc, hMdc;
HBITMAP hBmpFrame;

volatile bool bContinue = true;

COLORREF line = 0x000000FF;
COLORREF bckGrn = 0x00FF5555;
COLORREF nbckGrn;
COLORREF cross = 0x0000FFFF;
COLORREF ellipse = 0x00FFFF00;

const int CButton = 0x43;
const int QButton = 0x51;

int N = 3;
int HEIGHT = 240;
int WIDTH = 320;

LPTSTR FIELD; // shared memory
int fieldN = 0;

const int IDENT = 10;  // padding for cell
const int CLRCNG = 15; // color change for field

const int X = 1; // cross to matrix
const int O = 2; // circle to matrix

UINT FieldUpdate;
UINT CloseAll;
bool threadSuspended = false;

enum Role {
    XOPLAYER,
    XPLAYER,
    OPLAYER,
    OBSERVER
};

Role currentRole;
Role previousRole;

bool CheckRow(int y, TCHAR click)
{
    bool win = true;
    for (int x = 0; x < N && win; x++)
        win = FIELD[y * N + x] == click;
    return win;
}
bool CheckColumn(int x, TCHAR click)
{
    bool win = true;
    for (int y = 0; y < N && win; y++)
        win = FIELD[y * N + x] == click;
    return win;
}
bool CheckDiagonals(TCHAR click)
{
    bool win1 = true, win2 = true;
    for (int y = 0, x = 0; y < N && win1; y++, x++)
        win1 = FIELD[y * N + x] == click;
    for (int y = 0, x = N - 1; y < N && win2; y++, x--)
        win2 = FIELD[y * N + x] == click;
    return win1 || win2;
}

bool DrawCheck()
{
    bool draw = true;
    for (int y = 0; y < N && draw; y++)
        for (int x = 0; x < N && draw; x++)
            draw = FIELD[y * N + x] != NULL;
    return draw;
}

void HorizontalGradient(HDC hdc, const RECT& lprect,
    COLORREF rgbTop, COLORREF rgbBottom)
{
    GRADIENT_RECT gradientRect = { 0, 1 };
    TRIVERTEX triVertext[2] = {
        lprect.left,
        lprect.top,
        GetRValue(rgbTop) << 8,
        GetGValue(rgbTop) << 8,
        GetBValue(rgbTop) << 8,
        0x0000,
        lprect.right,
        lprect.bottom,
        GetRValue(rgbBottom) << 8,
        GetGValue(rgbBottom) << 8,
        GetBValue(rgbBottom) << 8,
        0x0000
    };
    GradientFill(hdc, triVertext, 2, &gradientRect, 1, GRADIENT_FILL_RECT_H);
}

/* Runs Notepad */
void RunNotepad()
{
    STARTUPINFO sInfo;
    PROCESS_INFORMATION pInfo;

    ZeroMemory(&sInfo, sizeof(STARTUPINFO));

    puts("Starting Notepad...");
    CreateProcess(_T("C:\\Windows\\Notepad.exe"),
        NULL, NULL, NULL, FALSE, 0, NULL, NULL, &sInfo, &pInfo);
}

void ClickCalc(RECT r, int xPos, int yPos, TCHAR result)
{
    int x = xPos / (r.right / N), y = yPos / (r.bottom / N);
    if (result != FIELD[N * N])
    {
        const int BUFF_SIZE = 50;
        wchar_t msg[BUFF_SIZE];
        WCHAR player = (FIELD[N * N] == X) ? 'X' : 'O';
        _snwprintf_s(msg, BUFF_SIZE, L"It's %c-player turn to move!", player);
        MessageBox(hwnd, (LPCWSTR)msg, _T(""), MB_OK | MB_ICONSTOP);
        return;
    }
    if (FIELD[y * N + x] == NULL)
    {
        FIELD[y * N + x] = FIELD[N * N];
        FIELD[N * N] = (result == X) ? O : X;
    }
    else return;
    /* FIELD = "(0*N+1),..,(0*N+N-1),*/
    /*(1*N+0),..,(1*N+N-1),*/
    /*......,((N-1)*N+N-1)"*/
    if (CheckRow(y, FIELD[y * N + x]) || CheckColumn(x, FIELD[y * N + x]) || CheckDiagonals(FIELD[y * N + x]))
    {
        const int BUFF_SIZE = 50;
        wchar_t msg[BUFF_SIZE];
        WCHAR player = (FIELD[y * N + x] == X) ? 'X' : 'O';
        _snwprintf_s(msg, BUFF_SIZE, L"%c-player won!", player);
        MessageBox(hwnd, (LPCWSTR)msg, _T("Congratulations!"), MB_OK);
        PostMessage(HWND_BROADCAST, CloseAll, NULL, NULL);
    }
    else if (DrawCheck())
    {
        MessageBox(hwnd, _T("Friendship won!"), _T("Draw!"), MB_OK);
        PostMessage(HWND_BROADCAST, CloseAll, NULL, NULL);
    }
}

void PaintCross(HDC hdc, int lX, int lY, int mX, int mY)
{
    HPEN xPen = CreatePen(PS_SOLID, 2, cross);
    SelectObject(hdc, xPen);
    MoveToEx(hdc, lX + IDENT, lY + IDENT, NULL);
    LineTo(hdc, mX - IDENT, mY - IDENT);
    MoveToEx(hdc, lX + IDENT, mY - IDENT, NULL);
    LineTo(hdc, mX - IDENT, lY + IDENT);
    DeleteObject(xPen);
}
void PaintEllipse(HDC hdc, int lX, int lY, int mX, int mY)
{
    HPEN ePen = CreatePen(PS_SOLID, 2, ellipse);
    SelectObject(hdc, ePen);
    Arc(hdc, lX + IDENT, lY + IDENT, mX - IDENT, mY - IDENT, (mX - lX) / 2, (mY - lY) / 2, (mX - lX) / 2, (mY - lY) / 2);
    DeleteObject(ePen);
}

void PaintFigures(HDC hdc, int xPos, int yPos)
{
    int partX = xPos / N, partY = yPos / N;
    for (int x = 0; x < N; x++) for (int y = 0; y < N; y++)
    {
        if (FIELD[y * N + x] == X) PaintCross(hdc, partX * x, partY * y, partX * (x + 1), partY * (y + 1));
        else if (FIELD[y * N + x] == O) PaintEllipse(hdc, partX * x, partY * y, partX * (x + 1), partY * (y + 1));
    }
}

void PaintLines(HDC hdc, int x, int y)
{
    HPEN hPen = CreatePen(PS_SOLID, 3, line);
    SelectObject(hdc, hPen);
    int partX = x / N;
    int partY = y / N;
    for (int i = 0; i < N - 1; i++)
    {
        MoveToEx(hdc, partX + partX * i, 0, NULL);
        LineTo(hdc, partX + partX * i, y);
        MoveToEx(hdc, 0, partY + partY * i, NULL);
        LineTo(hdc, x, partY + partY * i);
    }
    DeleteObject(hPen);
}

void Paint()
{
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    SelectObject(hMdc, hBmpFrame);
    HorizontalGradient(hMdc, clientRect, bckGrn, nbckGrn);
    PaintLines(hMdc, clientRect.right, clientRect.bottom);
    PaintFigures(hMdc, clientRect.right, clientRect.bottom);
    InvalidateRect(hwnd, NULL, TRUE);
}

DWORD WINAPI PaintFieldThread(LPVOID t)
{
    int i = 0;
    while (bContinue)
    {
        if (i++ % 5 == 0)
        {
            byte r = GetRValue(bckGrn), g = GetGValue(bckGrn), b = GetBValue(bckGrn); // calculates color in [85; 255]
            bool r255 = r == 255, b255 = b == 255, g255 = g == 255;
            bool r0 = r == 85, b0 = b == 85, g0 = g == 85;
            if (r255 && g0 && !b255) nbckGrn = RGB(r, g, b += 85);
            else if (!r0 && b255 && g0) nbckGrn = RGB(r -= 85, g, b);
            else if (b255 && r0 && !g255) nbckGrn = RGB(r, g += 85, b);
            else if (!b0 && g255 && r0) nbckGrn = RGB(r, g, b -= 85);
            else if (g255 && b0 && !r255) nbckGrn = RGB(r += 85, g, b);
            else if (r255 && b0 && !g0) nbckGrn = RGB(r, g -= 85, b);
        }
        Paint();
        if (i % 5 == 0)
            bckGrn = nbckGrn;
        Sleep(15);
    }
    ExitThread(0);
}

/*  This function is called by the Windows function DispatchMessage()  */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (currentRole == OBSERVER)
    {
        if (FIELD[N * N + 1] == NULL)
        {
            FIELD[N * N + 1] = 1;
            currentRole = previousRole = XPLAYER;
        }
        if (FIELD[N * N + 2] == NULL)
        {
            FIELD[N * N + 2] = 1;
            currentRole = previousRole = OPLAYER;
        }
    }
    if ((FIELD[N * N + 1] == NULL && currentRole == OPLAYER) || (FIELD[N * N + 2] == NULL && currentRole == XPLAYER))
        currentRole = XOPLAYER;
    else if (currentRole == XOPLAYER && (FIELD[N * N + 1] == 1 && FIELD[N * N + 2] == 1))
        currentRole = previousRole;

    if (message == FieldUpdate)
    {
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    if (message == CloseAll)
    {
        PostMessage(hwnd, WM_DESTROY, NULL, NULL); //TODO: Fix config writing
        return 0;
    }
    switch (message)                  /* handle the messages */
    {
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEWHEEL: // yeah, i am insane, how could you tell?
    {
        byte r = GetRValue(line), g = GetGValue(line), b = GetBValue(line);
        bool r255 = r == 255, b255 = b == 255, g255 = g == 255;
        bool r0 = r == 0, b0 = b == 0, g0 = g == 0;
        if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
        {
            if (r255 && g0 && !b255) line = RGB(r, g, b += CLRCNG);
            else if (!r0 && b255 && g0) line = RGB(r -= CLRCNG, g, b);
            else if (b255 && r0 && !g255) line = RGB(r, g += CLRCNG, b);
            else if (!b0 && g255 && r0) line = RGB(r, g, b -= CLRCNG);
            else if (g255 && b0 && !r255) line = RGB(r += CLRCNG, g, b);
            else if (r255 && b0 && !g0) line = RGB(r, g -= CLRCNG, b);
        }
        else
        {
            if (r255 && b0 && !g255) line = RGB(r, g += CLRCNG, b);
            else if (!r0 && g255 && b0) line = RGB(r -= CLRCNG, g, b);
            else if (g255 && r0 && !b255) line = RGB(r, g, b += CLRCNG);
            else if (!g0 && b255 && r0) line = RGB(r, g -= CLRCNG, b);
            else if (b255 && g0 && !r255) line = RGB(r += CLRCNG, g, b);
            else if (r255 && !b0 && g0) line = RGB(r, g, b -= CLRCNG);
        }
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
    case WM_KEYDOWN:
    {
        if ((wParam == QButton && (GetKeyState(VK_CONTROL) < 0)) || (wParam == VK_ESCAPE))
            PostMessage(hwnd, WM_DESTROY, NULL, NULL); // Exit
        else if (wParam == CButton && (GetKeyState(VK_SHIFT) < 0))
            RunNotepad(); // SHIFT + C
        else if (GetKeyState(VK_SPACE) < 0)
        {
            if (!threadSuspended)
                SuspendThread(PaintThread);
            else
                ResumeThread(PaintThread);

            threadSuspended = !threadSuspended;
        }
        else if (GetKeyState('1') < 0)
            SetThreadPriority(PaintThread, THREAD_PRIORITY_IDLE);
        else if (GetKeyState('2') < 0)
            SetThreadPriority(PaintThread, THREAD_PRIORITY_LOWEST);
        else if (GetKeyState('3') < 0)
            SetThreadPriority(PaintThread, THREAD_PRIORITY_BELOW_NORMAL);
        else if (GetKeyState('4') < 0)
            SetThreadPriority(PaintThread, THREAD_PRIORITY_NORMAL);
        else if (GetKeyState('5') < 0)
            SetThreadPriority(PaintThread, THREAD_PRIORITY_ABOVE_NORMAL);
        else if (GetKeyState('6') < 0)
            SetThreadPriority(PaintThread, THREAD_PRIORITY_HIGHEST);
        else if (GetKeyState('7') < 0)
            SetThreadPriority(PaintThread, THREAD_PRIORITY_TIME_CRITICAL);
    }
    return 0;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    {
        if (currentRole == OBSERVER)
            return 0;
        TCHAR result = (message == WM_LBUTTONDOWN) ? X : O;
        if (currentRole == XPLAYER && result == O) return 0;
        if (currentRole == OPLAYER && result == X) return 0;
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        ClickCalc(clientRect, LOWORD(lParam), HIWORD(lParam), result);
        PostMessage(HWND_BROADCAST, FieldUpdate, 0, 0);
    }
    return 0;
    case WM_SIZE:
    {
        RECT w;
        GetWindowRect(hwnd, &w);
        WIDTH = w.right - w.left;
        HEIGHT = w.bottom - w.top;
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        hBmpFrame = CreateCompatibleBitmap(GetDC(hwnd), clientRect.right, clientRect.bottom);
        Paint();
    }
    return 0;
    case WM_CREATE:
    {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        hdc = GetDC(hwnd);
        hMdc = CreateCompatibleDC(hdc);
        hBmpFrame = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
        SelectObject(hMdc, hBmpFrame);
        PaintThread = CreateThread( // thread for painting
            NULL,
            0,
            PaintFieldThread,
            NULL,
            0,
            NULL
        );
        if (PaintThread == NULL)
        {
            _tprintf(_T("Could not create new thread. (%d).\n"),
                GetLastError());
            
            return 1;
        }
    }
    return 0;
    case WM_PAINT:
        PAINTSTRUCT ps;
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        hdc = BeginPaint(hwnd, &ps);
        BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, hMdc, 0, 0, SRCCOPY);
        EndPaint(hwnd, &ps);
    return 0;
    case WM_DESTROY:
    bContinue = false;
    if (currentRole != OBSERVER)
        FIELD[N * N + ((currentRole == XPLAYER) ? 1 : 2)] = NULL;
    DeleteObject(hdc);
    DeleteObject(hMdc);
    DeleteObject(hBmpFrame);
    PostQuitMessage(0); /* send a WM_QUIT to the message queue */
    return 0;
    }

    /* for messages that we don't deal with */
    return DefWindowProc(hwnd, message, wParam, lParam);
}

int main(int argc, char** argv)
{
    FieldUpdate = RegisterWindowMessage(_T("FieldUpdate"));
    CloseAll = RegisterWindowMessage(_T("CloseAll"));

    BOOL isInput = (argc > 1 && atoi(argv[1]) > 0);
    BOOL isOpened = TRUE; // opened filemapping or not

    /* Shared Memory */
    HANDLE hFileMap = OpenFileMapping(
        FILE_MAP_ALL_ACCESS,
        FALSE,
        szField
    );
    if (hFileMap == NULL)
    {
        isOpened = FALSE;
        hFileMap = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            N * N + sizeof(TCHAR) * 3,
            szField
        );
        if (hFileMap == NULL)
        {
            _tprintf(_T("Could not create file mapping object (%d).\n"),
                GetLastError());
            
            return 1;
        }
    }

    /* config file */
    FILE* cnf;
    fopen_s(&cnf, "config.txt", "r");
    if (cnf != NULL)
        while ((fscanf_s(cnf, "%d\n%d\n%d\n%d\n%d", &N, &WIDTH, &HEIGHT, &bckGrn, &line)) != EOF);
    if (isInput && !isOpened)
        N = atoi(argv[1]);
    if (!isOpened && cnf == NULL)
    {
        fopen_s(&cnf, "config.txt", "w");
        if (cnf == NULL)
        {
            _tprintf(_T("Could not create config file (%d).\n"),
                GetLastError());
            
            return 1;
        }
        fprintf_s(cnf, "%d\n%d\n%d\n%d\n%d", N, WIDTH, HEIGHT, bckGrn, line);
    }
    fclose(cnf);

    /* Shared Memory*/
    FIELD = (LPTSTR)MapViewOfFile(
        hFileMap,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        N * N + sizeof(TCHAR) * 3
    );
    if (FIELD == NULL)
    {
        _tprintf(_T("Could not map view of file (%d).\n"),
            GetLastError());

        
        CloseHandle(hFileMap);

        return 1;
    }
    FIELD[N * N] = X;

    currentRole = OBSERVER;
   
    if (FIELD[N * N + 1] == NULL && FIELD[N * N + 2] == NULL)
    {
        FIELD[N * N + 1] = 1;
        currentRole = previousRole = XPLAYER;
    }

    srand(time(NULL));
    BOOL bMessageOk;
    MSG message;            /* Here message to the application are saved */
    WNDCLASS wincl = { 0 };         /* Data structure for the windowclass */

    /* Harcode show command num when use non-winapi entrypoint */
    int nCmdShow = SW_SHOW;
    /* Get handle */
    HINSTANCE hThisInstance = GetModuleHandle(NULL);

    /* The Window structure */
    wincl.hInstance = hThisInstance;
    wincl.lpszClassName = szWinClass;
    wincl.lpfnWndProc = WindowProcedure;      /* This function is called by Windows */
    wincl.hbrBackground = NULL;
    /* Register the window class, and if it fails quit the program */
    if (!RegisterClass(&wincl))
        return 0;

    /* The class is registered, let's create the program*/
    hwnd = CreateWindow(
        szWinClass,          /* Classname */
        szWinName,       /* Title Text */
        WS_OVERLAPPEDWINDOW, /* default window */
        CW_USEDEFAULT,       /* Windows decides the position */
        CW_USEDEFAULT,       /* where the window ends up on the screen */
        WIDTH,                 /* The programs width */
        HEIGHT,                 /* and height in pixels */
        HWND_DESKTOP,        /* The window is a child-window to desktop */
        NULL,                /* No menu */
        hThisInstance,       /* Program Instance handler */
        NULL                 /* No Window Creation data */
    );
    /* Make the window visible on the screen */
    ShowWindow(hwnd, nCmdShow);
    /* Run the message loop. It will run until GetMessage() returns 0 */
    while ((bMessageOk = GetMessage(&message, NULL, 0, 0)) != 0)
    {
        if (bMessageOk == -1)
        {
            _tprintf(_T("Suddenly, GetMessage failed (%d).\n"),
                GetLastError());
            
            break;
        }
        /* Translate virtual-key message into character message */
        TranslateMessage(&message);
        /* Send message to WindowProcedure */
        DispatchMessage(&message);
    }
    /* Cleanup stuff */
    DestroyWindow(hwnd);
    UnregisterClass(szWinClass, hThisInstance);
    DeleteObject(hBrush);
    UnmapViewOfFile(FIELD);
    CloseHandle(hFileMap);
    CloseHandle(bckGrnThread);
    CloseHandle(PaintThread);
    fopen_s(&cnf, "config.txt", "w");
    if (cnf == NULL)
    {
        _tprintf(_T("Could not create config file (%d).\n"),
            GetLastError());

        return 1;
    }
    fprintf_s(cnf, "%d\n%d\n%d\n%d\n%d", N, WIDTH, HEIGHT, bckGrn, line);
    fclose(cnf);
    return 0;
}