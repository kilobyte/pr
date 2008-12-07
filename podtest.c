#include <windows.h>
#include <stdio.h>

HINSTANCE inst;
HWND wnd;
char buf[65536];

extern void print(HDC dc, char *text, char *charset);

LRESULT APIENTRY MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void win32_init()
{
    WNDCLASS wc;
    FILE *f;
    
    f=fopen("file1.prn", "r");
    fread(buf, 1, 65536, f);
    fclose(f);

    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC) MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = inst;
    wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(0));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground=0;
//    wc.hbrBackground = CreateSolidBrush(0xf0fbff);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = "DeathToUselessButNeededParams";

    if (!RegisterClass(&wc))
        exit(0);

}

void create_window(int nCmdShow)
{
    wnd = CreateWindow(
        "DeathToUselessButNeededParams", "PodTest",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, inst,
        NULL);

    ShowWindow(wnd, nCmdShow);
    UpdateWindow(wnd);
}

int APIENTRY WinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;

    inst = instance;
    
    win32_init();
    
    create_window(nCmdShow);

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return msg.wParam;
        UNREFERENCED_PARAMETER(hPrevInstance);
}

void paint(HWND hwnd)
{
    HDC dc;
    PAINTSTRUCT paints;
    
    if (!(dc=BeginPaint(hwnd,&paints)))
        return;
    print(dc, buf, 0);
    EndPaint(hwnd,&paints);
}

LRESULT APIENTRY MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_PAINT:
            paint(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}
