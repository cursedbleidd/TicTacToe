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

#define KEY_SHIFTED     0x8000
#define KEY_TOGGLED     0x0001

const TCHAR szWinClass[] = _T("Tic Tac Toe");
const TCHAR szWinName[] = _T("TicTacToe");
const TCHAR szField[] = _T("Local\\field");
HWND hwnd;               /* This is the handle for our window */
HBRUSH hBrush;           /* Current brush */

COLORREF line = 0x000000FF;
COLORREF bckGrn = 0x00FF0000;
COLORREF cross = 0x0000FFFF;
COLORREF ellipse = 0x00FFFF00;

const int CButton = 0x43;
const int QButton = 0x51;

int N = 3;
int HEIGHT = 240;
int WIDTH = 320;

LPTSTR FIELD; // shared memory

const int IDENT = 10;  // padding for cell
const int CLRCNG = 15; // color change for field

const int X = 1; // cross to matrix
const int O = 2; // circle to matrix

UINT FieldUpdate;

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

void ClickCalc(RECT r, int xPos, int yPos, int click)
{
    int x = xPos / (r.right / N), y = yPos / (r.bottom / N);
    if (FIELD[y * N + x] == NULL) FIELD[y * N + x] = click;
    /* FIELD = "(0*N+1),..,(0*N+N-1),*/
    /*(1*N+0),..,(1*N+N-1),*/
    /*......,((N-1)*N+N-1)"*/
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
/*  This function is called by the Windows function DispatchMessage()  */
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == FieldUpdate)
        InvalidateRect(hwnd, NULL, TRUE);
    switch (message)                  /* handle the messages */
    {
    case WM_MOUSEWHEEL: /* yeah, i am insane, how could you tell? */
    {
        byte r = GetRValue(line), g = GetGValue(line), b = GetBValue(line);
        boolean r255 = r == 255, b255 = b == 255, g255 = g == 255;
        boolean r0 = r == 0, b0 = b == 0, g0 = g == 0;
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
        else if (wParam == VK_RETURN)
        {
            int r = rand() % 256, g = rand() % 256, b = rand() % 256;
            bckGrn = RGB(r, g, b);
            HANDLE oldBrush = SetClassLongPtr(hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(bckGrn));
            InvalidateRect(hwnd, NULL, TRUE);
            DeleteObject(oldBrush);
        }
    }
    return 0;
    case WM_LBUTTONDOWN:
    {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        ClickCalc(clientRect, LOWORD(lParam), HIWORD(lParam), X);
        PostMessage(HWND_BROADCAST, FieldUpdate, 0, 0);
    }
    return 0;
    case WM_RBUTTONDOWN:
    {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        ClickCalc(clientRect, LOWORD(lParam), HIWORD(lParam), O);
        PostMessage(HWND_BROADCAST, FieldUpdate, 0, 0);
    }
    return 0;
    case WM_SIZE:
    {
        RECT w;
        GetWindowRect(hwnd, &w);
        WIDTH = w.right - w.left;
        HEIGHT = w.bottom - w.top;
        InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
    case WM_PAINT:
    {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        PaintLines(hdc, clientRect.right, clientRect.bottom);
        PaintFigures(hdc, clientRect.right, clientRect.bottom);
        EndPaint(hwnd, &ps);
        DeleteObject(hdc);
    }
    return 0;
    case WM_DESTROY:
    {
        FILE* cnf;
        fopen_s(&cnf, "config.txt", "w");
        fprintf_s(cnf, "%d\n%d\n%d\n%d\n%d", N, WIDTH, HEIGHT, bckGrn, line);
        fclose(cnf);
    }
    PostQuitMessage(0); /* send a WM_QUIT to the message queue */
    return 0;
    }

    /* for messages that we don't deal with */
    return DefWindowProc(hwnd, message, wParam, lParam);
}

int main(int argc, char** argv)
{
    FieldUpdate = RegisterWindowMessage(_T("MSG"));

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
            N * N,
            szField
        );
        if (hFileMap == NULL)
        {
            _tprintf(_T("Could not create file mapping object (%d).\n"),
                GetLastError());
            _getch();
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
        fprintf_s(cnf, "%d\n%d\n%d\n%d\n%d", N, WIDTH, HEIGHT, bckGrn, line);
    }
    fclose(cnf);

    /* Shared Memory*/
    FIELD = (LPTSTR)MapViewOfFile(
        hFileMap,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        N * N
    );
    if (FIELD == NULL)
    {
        _tprintf(_T("Could not map view of file (%d).\n"),
            GetLastError());

        _getch();
        CloseHandle(hFileMap);

        return 1;
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

    /* Use custom brush to paint the background of the window */
    hBrush = CreateSolidBrush(bckGrn);
    wincl.hbrBackground = hBrush;

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
            _getch();
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
    return 0;
}