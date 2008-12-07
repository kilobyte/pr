#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>

//#define DEBUG
#define CHX_M 1
#define CHX_D 10
#define CHY_M 3317
#define CHY_D (300*66)
#define VER "0.75"
#ifdef PRH
# define NAME "prh"
#else
# define NAME "pr"
#endif

static HINSTANCE hinst;
static HWND wnd;
static char filename[256];
static int ask,show,warte,duplex,booklet,xerox;
static int curpage,maxpage,minpage;
static HDC printdc;
static int sx,sy,rx,ry;
static FILE *pf;
static int layout;
static DEVMODE *devmode;

static LRESULT APIENTRY MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static HMENU create_menu();
static void do_print();

extern int printdoc_f(FILE *f, char *charset);
#ifdef PRH
# define printdoc_draw(a,b,c,d) printdoc_drawmem(a,b,c,d)
  extern void printdoc_drawmem(HDC dc, int npage, float s, int layout);
#else
  extern void printdoc_draw(HDC dc, int npage, float s, int layout);
#endif
extern int printdoc_charset(char *chset);

static int parse_params(char *cmd);
static int get_printdc(int ask);
void msg(char *format, ...);

int make_wclass()
{
    WNDCLASS wc;
    
    wc.style = 0;
    wc.lpfnWndProc = (WNDPROC) MainWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hinst;
    wc.hIcon = LoadIcon(hinst, MAKEINTRESOURCE(0));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = "JKMPrint";

    return RegisterClass(&wc);
}


void show_error()
{
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError();

    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR) &lpMsgBuf,
        0, NULL );
    
    MessageBox(0, lpMsgBuf, "pr: b³¹d", MB_ICONERROR);

    LocalFree(lpMsgBuf);
}


static void save_devmode(DEVMODE *dm)
{
    HKEY reg;
    
    if (RegCreateKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\JKM\\"NAME, 0, 0, 0, KEY_WRITE, 0, &reg, 0))
        return;
    RegSetValueEx(reg, "Printer", 0, REG_BINARY, (BYTE*)dm, sizeof(DEVMODE));
    RegCloseKey(reg);
}


static void load_devmode()
{
    HKEY reg;
    DEVMODE dm, *pdm;
    DWORD len;
    
    if (RegOpenKeyEx(HKEY_CURRENT_USER, "SOFTWARE\\JKM\\"NAME, 0, KEY_READ, &reg))
        return;
    len=sizeof(DEVMODE);
    if (RegQueryValueEx(reg, "Printer", 0, 0, (BYTE*)&dm, &len))
        return;
    RegCloseKey(reg);
    if (len!=sizeof(DEVMODE))
        return;
    
    if (!(pdm=malloc(sizeof(DEVMODE))))
        return;
    memcpy(pdm, &dm, sizeof(DEVMODE));
    devmode=pdm;
}


int APIENTRY WinMain(HINSTANCE hinstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG mess;
    HANDLE fh,dh;
    WIN32_FIND_DATA filedata;
    char filepath[MAX_PATH*2], *fileend;

    hinst = hinstance;
    
    filename[0]=0;
    ask=show=warte=duplex=booklet=xerox=0;
    curpage=1;
    printdc=0;
    devmode=0;
    if (!parse_params(lpCmdLine))
        return 0;
    load_devmode();
    if (warte && !get_printdc(ask))
    {
        if (!ask)
            msg("Brak drukarki!\n");
        return 0;
    }
    if (show && !make_wclass())
        return 0;
    do
    {
        if (warte)
        {
            while(1)
            {
                dh=FindFirstFile(filename, &filedata);
                if (dh!=INVALID_HANDLE_VALUE)
                {
                    FindClose(dh);
                    GetFullPathName(filename, MAX_PATH, filepath, &fileend);
                    if (fileend)
                        strncpy(fileend, filedata.cFileName, MAX_PATH-1);
                    else
                        strncat(filepath, filedata.cFileName, MAX_PATH*2-1);
                    fh=CreateFile(filepath, GENERIC_READ, 
                        FILE_SHARE_READ, 0, OPEN_EXISTING,
                        FILE_FLAG_DELETE_ON_CLOSE|FILE_FLAG_SEQUENTIAL_SCAN, 0);
                    if (fh!=INVALID_HANDLE_VALUE)
                        break;
//                    MessageBox(0,filepath,"Coœ jest ale nie daje siê otworzyæ",0);
                }
                Sleep(5000);
            };
            pf=fdopen(_open_osfhandle((intptr_t)fh, _O_RDONLY), "r");
        }
        else
        {
            pf=fopen(filename, "r");
            if (!pf)
            {
                msg("Nie mogê otworzyæ: %s\n", filename);
                exit(1);
            }
        }
        maxpage=printdoc_f(pf, 0);
        fclose(pf);
        if (maxpage<=0)
        {
            if (warte)
                continue;
            msg(maxpage?"Problem z zestawem znaków.\n":"Pusty wydruk.\n");
            exit(1);
        }
        if (!warte && !get_printdc(ask))
        {
            if (!ask)
                msg("Brak drukarki!\n");
            return 0;
        }

        if (!show)
        {
            wnd=0;
            do_print();
            continue;
        }
        

        wnd = CreateWindow("JKMPrint", filename,
            WS_OVERLAPPEDWINDOW|WS_HSCROLL|WS_VSCROLL, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, NULL, create_menu(), hinstance,
            NULL);

        if (wnd == NULL)
            return FALSE;

        ShowWindow(wnd, nCmdShow);
        UpdateWindow(wnd);

        while (GetMessage(&mess, NULL, 0, 0)) {
            TranslateMessage(&mess);
            DispatchMessage(&mess);
        }
    } while (warte);
    return 0;
    UNREFERENCED_PARAMETER(hPrevInstance);
}

#define MID_PRINT 1
#define MID_SETUP 2
#define MID_PREV 3
#define MID_NEXT 4
#define MID_ABOUT 5
#define MID_CHARSETS 10
#define MID_PAGES 1024

static char* charnames[]=
{
    "IBM",
    "Mazovia",
    "Windows 1250",
    "Latin 2",
    "ISO 8859-2",
    0,
};

static HMENU create_charsets_menu()
{
    HMENU s=CreatePopupMenu();
    int i;

    for(i=0;charnames[i];i++)
        InsertMenu(s, -1,
            MF_BYPOSITION|MF_STRING /* FIXME |((i==curcharset)?MF_CHECKED:0) */,
            MID_CHARSETS+i,
            charnames[i]);
    return s;
}

static HMENU create_pages_menu()
{
    HMENU s=CreatePopupMenu();
    int i;
    char buf[16];
    
    for(i=1;i<=maxpage;i++)
    {
        sprintf(buf, "%d", i);
        InsertMenu(s, -1, MF_BYPOSITION|MF_STRING|((i>1 && i%10==1)?MFT_MENUBARBREAK:0), MID_PAGES+i, buf);
    }
    return s;
}

static HMENU create_menu()
{
    HMENU m=CreateMenu();
    HBITMAP sep,oldbmp;
    HDC memdc;
    RECT rect;
    int h;
    char buf[16];
    
    memdc=CreateCompatibleDC(0);
    sep=CreateCompatibleBitmap(memdc, 1, h=GetSystemMetrics(SM_CYMENU));
    oldbmp=SelectObject(memdc, sep);
    rect.top=rect.left=0;
    rect.right=1;
    rect.bottom=h;
    FillRect(memdc, &rect, GetStockObject(BLACK_BRUSH));
    SelectObject(memdc, oldbmp);
    DeleteDC(memdc);
    
    InsertMenu(m, -1, MF_BYPOSITION|MF_STRING, MID_PRINT, "&Drukuj");
    InsertMenu(m, -1, MF_BYPOSITION|MF_STRING, MID_SETUP, "&Ustawienia...");
    InsertMenu(m, -1, MF_BYPOSITION|MF_BITMAP|MF_GRAYED, 0, (char*)sep);
    InsertMenu(m, -1, MF_BYPOSITION|MF_STRING|MF_GRAYED, MID_PREV, "&<<");
    sprintf(buf, "Strona 1/%d", maxpage);
    InsertMenu(m, -1, MF_BYPOSITION|MF_STRING|MF_POPUP|((maxpage<=1)?MF_GRAYED:0),
          (UINT)create_pages_menu(), buf);
    InsertMenu(m, -1, MF_BYPOSITION|MF_STRING|((maxpage<=1)?MF_GRAYED:0), MID_NEXT, "&>>");
    InsertMenu(m, -1, MF_BYPOSITION|MF_BITMAP|MF_GRAYED, 0, (char*)sep);
    /*
    InsertMenu(m, -1, MF_BYPOSITION|MF_STRING|MF_POPUP, (UINT)create_charsets_menu(), charnames[curcharset]);
    */
    InsertMenu(m, -1, MF_BYPOSITION|MF_BITMAP|MF_GRAYED, 0, (char*)sep);
    InsertMenu(m, -1, MF_BYPOSITION|MF_STRING, MID_ABOUT, "&O programie...");
    return m;
}

static void update_menu(int np)
{
    HMENU m=GetMenu(wnd);
    char buf[16];
    
    EnableMenuItem(m, 3, MF_BYPOSITION|((curpage>1)?MF_ENABLED:MF_GRAYED));
    EnableMenuItem(m, 5, MF_BYPOSITION|((curpage<maxpage)?MF_ENABLED:MF_GRAYED));
    sprintf(buf, "Strona %d/%d", curpage, maxpage);
    ModifyMenu(m, 4, MF_BYPOSITION|MF_STRING|((maxpage<=1)?MF_GRAYED:0), 0, buf);
    /*
    sprintf(buf, "Zestaw znaków: %s", charnames[curcharset]);
    ModifyMenu(m, 7, MF_BYPOSITION|MF_STRING|MF_POPUP, (UINT)create_charsets_menu(), buf);
    */
    DrawMenuBar(wnd);
}

#define BUFFER_SIZE 1024

void msg(char *format, ...)
{
    va_list ap;
    char buf[BUFFER_SIZE];
    
    va_start(ap, format);
    if (0/*GetStdHandle(STD_INPUT_HANDLE)*/)
        vfprintf(stderr, format, ap);
    else
    {
        vsnprintf(buf, BUFFER_SIZE, format, ap);
        MessageBox(wnd, buf, "JKM Print", MB_ICONERROR);
    }
    va_end(ap);
}

static void do_paint(HWND hwnd)
{
    HDC dc;
    PAINTSTRUCT paints;
    int zx,zy;
    
    
    if (!(dc=BeginPaint(hwnd,&paints)))
        return;
    zx=GetScrollPos(wnd, SB_HORZ);
    zy=GetScrollPos(wnd, SB_VERT);
    SetWindowOrgEx(dc, zx*1440/rx, -zy*1440/ry, 0);
//    printf("org=%d,%d r=%d,%d\n", zx, zy, rx, ry);
    printdoc_draw(dc, curpage, 1.0, 0);
    EndPaint(hwnd,&paints);
}

static int get_printdc(int ask)
{
    PRINTDLG pd;
    DEVMODE *dm;
    HDC dc;
    
getdefault:
    minpage=1;
    if (ask || !devmode)
    {
        memset(&pd, 0, sizeof(pd));
        pd.lStructSize=sizeof(pd);
        pd.hwndOwner=wnd;
        pd.Flags=PD_NOSELECTION|PD_USEDEVMODECOPIESANDCOLLATE;
        if (warte)
            pd.Flags|=PD_NOPAGENUMS;
        pd.nMinPage=pd.nFromPage=minpage;
        pd.nMaxPage=pd.nToPage=maxpage;
        if (!ask)
            pd.Flags|=PD_RETURNDEFAULT;
        if (!PrintDlg(&pd))
            return 0;
        dm=GlobalLock(pd.hDevMode);
        if (pd.Flags&PD_PAGENUMS)
        {
            minpage=pd.nFromPage;
            if (minpage<1)
                minpage=1;
            if (pd.nToPage<maxpage)
                maxpage=pd.nToPage;
        }
    }
    else
        dm=devmode;
    dm->dmDuplex=duplex? DMDUP_HORIZONTAL : DMDUP_SIMPLEX;
#ifdef PRH
    dm->dmFields|=DM_PAPERSIZE;
    dm->dmPaperSize=DMPAPER_A4;
#endif
    dc=CreateDC(0, dm->dmDeviceName, 0, dm);
    if (!dc)
    {
        if (!ask && devmode)
        {
            devmode=0;
            goto getdefault;
        }
        show_error();
        exit(1);
    }
    if (printdc)
        DeleteDC(printdc);
    printdc=dc;
    if (ask)
        save_devmode(dm);
    devmode=dm;
    return 1;
}

static void do_print()
{
    DOCINFO di;
    int i;
    
    if (!printdc && !get_printdc(0))
        return;

    float size = xerox ? 78.0/80 : 1.0;

    memset(&di, 0, sizeof(di));
    di.cbSize=sizeof(di);
    di.lpszDocName=filename;
    if (StartDoc(printdc, &di)>0)
    {
        if (booklet)
            maxpage=(maxpage+3)&~3;
        for(i=minpage;i<=maxpage;i++)
        {
            if (layout!=3)
                StartPage(printdc);
            if (booklet)
                printdoc_draw(printdc, (i&2)? (i+1)/2 : maxpage-(i-1)/2, size, layout);
            else
                printdoc_draw(printdc, i, size, layout);
            if (layout!=2 || i==maxpage)
                EndPage(printdc);
            if (layout==2 || layout==3)
                layout=5-layout;	// flip page
        }
        EndDoc(printdc);
    }
}

static void set_scrollbars(int wx, int wy)
{
    SCROLLINFO si;
    int inv=0;
    HDC dc;
    
    dc=GetDC(0);
    sx=(rx=GetDeviceCaps(dc, LOGPIXELSX))*8;
    sy=MulDiv(ry=GetDeviceCaps(dc, LOGPIXELSY),3317,300);
    ReleaseDC(0, dc);
    if (layout==1)
        sx*=2;
    
#ifdef DEBUG
    printf("s=%dx%d\n", sx, sy);
#endif
    
    si.cbSize=sizeof(si);
    si.fMask=SIF_PAGE|SIF_RANGE|SIF_POS;
    GetScrollInfo(wnd, SB_HORZ, &si);
    si.nMin=0;
    si.nMax=sx;
    si.nPage=wx;
    if (si.nPos>sx-wx && (si.nPos || sx>wx))
        si.nPos=sx-wx, inv=1;
    SetScrollInfo(wnd, SB_HORZ, &si, 1);
    GetScrollInfo(wnd, SB_VERT, &si);
    si.nMin=0;
    si.nMax=sy;
    si.nPage=wy;
    if (si.nPos>sy-wy && (si.nPos || sy>wy))
        si.nPos=sy-wy, inv=1;
    SetScrollInfo(wnd, SB_VERT, &si, 1);
    if (inv)
        InvalidateRect(wnd, 0, 1);
}

#define SB_MOUSE -1

static void do_scroll(int bar, int cmd, int pos)
{
    SCROLLINFO si;
    
    si.cbSize=sizeof(si);
    si.fMask=SIF_PAGE|SIF_RANGE|SIF_POS;
    GetScrollInfo(wnd, bar, &si);
    if (si.nPage>=si.nMax)
        return;
    switch(cmd)
    {
    case SB_BOTTOM:
        pos=si.nMax-si.nPage;
        break;
    case SB_LINELEFT:
        pos=si.nPos-((bar==SB_VERT)?ry*CHY_M/CHY_D:rx*CHX_M/CHX_D);
        break;
    case SB_LINERIGHT:
        pos=si.nPos+((bar==SB_VERT)?ry*CHY_M/CHY_D:rx*CHX_M/CHX_D);
        break;
    case SB_PAGELEFT:
        pos=si.nPos-si.nPage;
        break;
    case SB_PAGERIGHT:
        pos=si.nPos+si.nPage;
        break;
    case SB_THUMBPOSITION:
        break;
    case SB_TOP:
        pos=0;
        break;
    case SB_MOUSE:
        pos=si.nPos+pos/4;
        break;
    default:
        return;
    }
    if (pos<si.nMin)
        pos=si.nMin;
    if (pos>si.nMax-si.nPage)
        pos=si.nMax-si.nPage;
    SetScrollPos(wnd, bar, pos, 1);

/*
    ScrollWindowEx(wnd,
        (bar==SB_VERT)?0:(si.nPos-pos),
        (bar==SB_VERT)?(si.nPos-pos):0,
        0, 0, 0, 0, SW_ERASE|SW_INVALIDATE);
*/
    InvalidateRect(wnd, 0, 1);
}

LRESULT APIENTRY MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {

        case WM_PAINT:
            do_paint(hwnd);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        
        case WM_SIZE:
            set_scrollbars(LOWORD(lParam), HIWORD(lParam));
            return 0;
        
        case WM_HSCROLL:
            do_scroll(SB_HORZ, LOWORD(wParam), HIWORD(wParam));
            break;
        case WM_VSCROLL:
            do_scroll(SB_VERT, LOWORD(wParam), HIWORD(wParam));
            break;
        
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
            case MID_PRINT:
                do_print();
                return 0;
            case MID_SETUP:
                get_printdc(1);
                return 0;
            case MID_PREV:
                curpage--;
                InvalidateRect(wnd, 0, 1);
                update_menu(curpage);
                return 0;
            case MID_NEXT:
                curpage++;
                InvalidateRect(wnd, 0, 1);
                update_menu(curpage);
                return 0;
            case MID_ABOUT:
                MessageBox(wnd,
NAME" "VER"\n"
"JKM s.c.\n"
"tel. (0-58) 56-006-15\n"
"ul. Hallera 31/2\n"
"83-200 Starogard Gdañski"
                , NAME, MB_OK);
                return 0;
            }
            /*
            if (LOWORD(wParam)>=MID_CHARSETS && LOWORD(wParam)<20)
            {
                curcharset=LOWORD(wParam)-MID_CHARSETS;
                menu_done=0;
                InvalidateRect(wnd, 0, 1);
                return 0;
            }
            */
            if (LOWORD(wParam)>MID_PAGES && LOWORD(wParam)<32768)
            {
                curpage=LOWORD(wParam)-MID_PAGES;
                InvalidateRect(wnd, 0, 1);
                update_menu(curpage);
                return 0;
            }
            return 0;
        
        case WM_KEYDOWN:
            switch(wParam)
            {
            case VK_PRIOR:
                if (curpage>1)
                {
                    curpage--;
                    InvalidateRect(wnd, 0, 1);
                    do_scroll(SB_VERT, SB_BOTTOM, 0);
                    update_menu(curpage);
                }
                break;
            case VK_NEXT:
                if (curpage<maxpage)
                {
                    curpage++;
                    InvalidateRect(wnd, 0, 1);
                    do_scroll(SB_VERT, SB_TOP, 0);
                    update_menu(curpage);
                }
                break;
            case VK_LEFT:
                do_scroll(SB_HORZ, SB_LINELEFT, 0);
                break;
            case VK_RIGHT:
                do_scroll(SB_HORZ, SB_LINERIGHT, 0);
                break;
            case VK_UP:
                do_scroll(SB_VERT, SB_LINELEFT, 0);
                break;
            case VK_DOWN:
                do_scroll(SB_VERT, SB_LINERIGHT, 0);
                break;
            }
            return 0;
        
        case WM_MOUSEWHEEL:
            do_scroll(SB_VERT, SB_MOUSE, -(short)HIWORD(wParam));
            return 0;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

static int parse_params(char *cmd)
{
    int state=0;
    char *fn=filename;
    char chset[256],*ch;

//    MessageBox(0, cmd, "Parametry", 0);    
    ch=chset;
    layout=0;
    
    cmd--;
    while(*++cmd)
        switch(state)
        {
        case 0:
            switch(*cmd)
            {
            case '-':
                state=-1;
                goto again;
            case '"':
                state=2;
                goto again;
            }
            if (fn!=filename)
                goto doubled;
            state=1;
        case 1:
            if (*cmd==' ')
            {
                state=0;
                break;
            }
            if (fn-filename>253)
            {
                msg("Za d³uga nazwa!\n");
                exit(0);  // 0 nie 1 -- Windows jest przeciwny naturze
            }
            *fn++=*cmd;
            break;
        case 2:
            if (fn!=filename)
                goto doubled;
            if (*cmd=='"')
            {
                state=3;
                break;
            }
            if (fn-filename>253)
            {
                msg("Za d³uga nazwa!\n");
                exit(0);  // 0 nie 1 -- Windows jest przeciwny naturze
            }
            *fn++=*cmd;
            break;
        case 3:
            if (*cmd==' ')
            {
                state=0;
                break;
            }
            msg("Œmieci po cudzys³owie: %s\n", cmd);
            goto usage;
        case -1:
            switch(*cmd)
            {
            case ' ':
                state=0;
                break;
            case 'p':
                ask=1;
                break;
            case 's':
                show=1;
                break;
            case 'w':
                warte=1;
                break;
            case 'c':
                ch=chset;
                state=-2;
                break;
            case 'h':
                if (layout==2)
                    goto landscape2;
                layout=1;
                break;
            case 'b':
                booklet=1;
            case '2':
                if (layout==1)
                {
                landscape2:
                    msg("Wydruk landscape dwie strony na kartce nie jest obs³ugiwany");
                    exit(1);
                }
                layout=2;
                break;
            case 'd':
                duplex=1;
                break;
            case 'X':
                xerox=1;
                break;
            default:
                msg("Z³y parametr!\n");
                goto usage;
            }
            break;
        case -2:
            switch(*cmd)
            {
            case ':':
            case ' ':
                state=-3;
                break;
            default:
                msg("B³êdny parametr.  Powinno byæ -c: XXX\n");
                goto usage;
            }
            break;
        case -3:
            if (*cmd==' ')
            {
                state=0;
                break;
            }
            if (ch-chset>250)
            {
                msg("Blêdny zestaw znaków.\n");
                goto usage;
            }
            *ch++=*cmd;
            break;
        again:;
        }
    if (ch!=chset)
    {
        *ch=0;
        if (!printdoc_charset(chset))
        {
            msg("Nieznany zestaw znaków: %s.\n"
                "Poprawne to:\n"
                "\tIBM - standardowy ROM bez polskich znaków\n"
                "\tMAZ - Mazovia\n"
                "\tWIN - Windows 1250\n"
                "\tLAT - Latin 2\n"
                "\tISO - ISO 8859-2\n",
                chset);
            exit(0);
        }
    }
    if (fn!=filename)
        return 1;
    goto show_usage;
doubled:
    msg("Tylko jeden wydruk na raz, proszê.\n");
usage:
    if (!GetStdHandle(STD_INPUT_HANDLE))
        exit(0);
show_usage:
    msg("U¿ycie:"NAME" [-p|-s] [-c:IBM|437|MAZ|WIN|1250|LAT|852|ISO] [-h|-2] [-w] nazwa.prn\n"
        "\t-p pyta siê o typ drukarki\n"
        "\t-s pokazuje podgl¹d wydruku\n"
        "\t-c wybiera zestaw znaków\n"
        "\t-h poziomy uk³ad strony\n"
        "\t-2 dwie strony na kartkê\n"
        "\t-b tryb ksi¹¿eczkowy (kolejnoœæ strony 4123), w³¹cza -2\n"
        "\t-d duplex (wydruk dwustronny)\n"
        "\t-w czeka w pêtli na dostêpnoœæ danych\n"
        "\t-X nieco wê¿szy wydruk (b³¹d jednej drukarki Xeroxa)\n"
        "Wersja "VER);
    exit(0);
}
