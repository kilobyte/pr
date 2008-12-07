/*
char* print(char *fn, int *mode, char *charset)

fn	-	nazwa zbioru
mode	-	tryb:
                0	-	sam wydruk
                1	-	podglπd i ew. wydruk
                2	-	pytanie o typ drukarki i wydruk
charset	-	zestaw znakÛw
                "IBM", "MAZ", "1250", "LAT2", "ISO"
*/

#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include "charsets.h"

//#define DEBUG
#define EXACTWRITE
#define SIZEMOD 
#define MARGIN
#define MARGINX 8*1440
#define MARGINY 15922

static int curcharset;

static wchar_t charset_mazovia[256];
static void build_mazovia()
{
    memcpy(&charset_mazovia, &charset_437, sizeof(charset_mazovia));
#define MAZ(maz, name, unicode) charset_mazovia[maz]=unicode;
    MAZ(134, 'π', 0x0161);
    MAZ(143, '•', 0x0104);
    MAZ(145, 'Í', 0x0119);
    MAZ(144, ' ', 0x0118);
    MAZ(141, 'Ê', 0x0107);
    MAZ(149, '∆', 0x0106);
    MAZ(162, 'Û', 0x00F3);
    MAZ(163, '”', 0x00D3);
    MAZ(164, 'Ò', 0x0144);
    MAZ(165, '—', 0x0143);
    MAZ(160, 'è', 0x017A);
    MAZ(167, 'è', 0x0179);
    MAZ(158, 'ú', 0x015B);
    MAZ(152, 'å', 0x015A);
    MAZ(166, 'ø', 0x017C);
    MAZ(161, 'Ø', 0x017B);
    MAZ(146, '≥', 0x0142);
    MAZ(156, '£', 0x0141);
#undef MAZ
}

static wchar_t* charsets[]=
{
    charset_437,
    0,
    charset_cp1250,
    charset_cp852,
    charset_8859_2,
};

#define BUFFER_SIZE 256
static wchar_t buf[BUFFER_SIZE],*bp;
static HDC dc;
static HFONT oldfont;
static int x,y,ly;

static int mode_bold;
static int mode_sscript;	// -1: superscript, 1: subscript
static int mode_underline;
static int mode_italic;
static int mode_cpi;
static int mode_condensed;
static int mode_doublewide;
static int mode_doublewide_reset;
static int mode_proportional;
static int mode_doublehigh;

static int chx,chy,cx,cy;
static int in_page;

static void flush_buf()
{
    SIZE sz;
    int i;

    if (bp==buf)
        return;
    if (!in_page)
    {
        in_page=1;
    }
    
    if (mode_proportional)
    {
        TextOutW(dc, x, -y-(mode_sscript==1?chy/2:0), buf, bp-buf);
        GetTextExtentPointW(dc, buf, bp-buf, &sz);
//      x+=sz.cx;
    }
    else
#ifdef EXACTWRITE
        for(i=0;i<bp-buf;i++)
        {
            TextOutW(dc, x, -y-(mode_sscript==1?chy/2:0), buf+i, 1);
            x+=chx;
        }
#else
    {
        TextOutW(dc, x, -y-(mode_sscript==1?chy/2:0), buf, bp-buf);
        x+=chx*(bp-buf);
    }
#endif
    bp=buf;
}

static void reset()
{
    mode_bold=0;
    mode_sscript=0;
    mode_underline=0;
    mode_italic=0;
    mode_cpi=10;
    mode_condensed=0;
    mode_doublewide=0;
    mode_doublewide_reset=0;
    mode_proportional=0;
    mode_doublehigh=0;
}

static void set_font(int resized)
{
    LOGFONT lf;
    SIZE sf;
    int width;

    memset(&lf,0,sizeof(LOGFONT));
//    if (printing)
//        lf.lfHeight=GetDeviceCaps(dc, VERTRES)/66;
//    else
    lf.lfHeight=241 SIZEMOD;
    lf.lfWidth=1440
        *(mode_doublewide?2:1)
        /(mode_condensed?17:mode_cpi) SIZEMOD;
    width=lf.lfWidth;
    lf.lfPitchAndFamily=mode_proportional?VARIABLE_PITCH:FIXED_PITCH;
//    strcpy(lf.lfFaceName,"Bitstream Vera Sans Mono");
//    if (!printing)
    lf.lfQuality=5/*ANTIALIASED_QUALITY*/;
    if (mode_bold)
        lf.lfWeight=FW_BOLD;
    if (mode_underline)
        lf.lfUnderline=1;
    if (mode_italic)
        lf.lfItalic=1;
    if (mode_doublehigh)
        lf.lfHeight*=2;
    if (mode_sscript)
        lf.lfHeight/=2;
    if (!oldfont)
        resized=1;
#ifdef DEBUG
    printf("%d%s CPI.  Requesting %ldx%ld",
        mode_condensed?17:mode_cpi,
        mode_doublewide?"/2":"",
        lf.lfWidth, lf.lfHeight);
#endif
    chx=lf.lfWidth;
    chy=lf.lfHeight;
    if (cx==0)
        cx=chx, cy=chy;
    //TODO:wywalic resized, zrobic cos z cx i sf
    
    if (oldfont)
        DeleteObject(SelectObject(dc,CreateFontIndirect(&lf)));
    else
        oldfont=SelectObject(dc,CreateFontIndirect(&lf));
    GetTextExtentPoint(dc,"W",1,&sf);
#ifdef DEBUG
    printf(", got %ldx%ld\n", sf.cx, sf.cy);
#endif
}

static void paint(char *text)
{
    int state;
    char ch;
    wchar_t last;
    int oldmm;

    oldmm=GetMapMode(dc);
    SetMapMode(dc, MM_TWIPS);    
#ifdef DEBUG
    printf(
"Device caps:\n"
"	res: %4dx%4d\n"
"	DPI: %4dx%4d\n",
        GetDeviceCaps(dc, HORZRES),
        GetDeviceCaps(dc, VERTRES),
        GetDeviceCaps(dc, LOGPIXELSX),
        GetDeviceCaps(dc, LOGPIXELSY));
#endif

    in_page=0;
    state=0;
    bp=buf;
    x=y=ly=0;
    reset();
    cx=cy=0;
    set_font(1);
    SetBkMode(dc, TRANSPARENT);
    
    while((ch=*text++))
    {
        switch(state)
        {
//----------------------------------------------------------------------------
        normal:
        case 0:
            switch(ch)
            {
            case '\a':	// Bell [ignore]
                break;
                
            case '\b':	// Backspace
                state=1;
                break;
                
            case '\t':	// Tab
                flush_buf();
                x=(x/(chx*8)+1)*(chx*8);
                break;
                
            case '\n':	// Newline
            newline:
                flush_buf();
                y+=chy;
                x=0;
                if (mode_doublewide_reset)
                {
                    mode_doublewide=0;
                    mode_doublewide_reset=0;
                    set_font(1);
                }
                if ((ly+=1+!!mode_doublehigh)>=66)
                    goto formfeed;
#if 0
                *bp++='0'+ly/10;
                *bp++='0'+ly%10;
                *bp++=':';
                *bp++=' ';
#endif
                break;
            
            case '\v':	// Vertical tab
                flush_buf();
                ly=((ly/8)+1)*8-1;
                y=ly*(chy/(1+!!mode_doublehigh));
                goto newline;
            
            case '\f':	// Form Feed
            formfeed:
                flush_buf();
                x=y=ly=0;
                break;
                
            case '\r':	// CR
                flush_buf();
                x=0;
                break;
            
            case 14:	// Double-wide on (1 line)
                flush_buf();
                mode_doublewide=1;
                mode_doublewide_reset=1;
                set_font(1);
                break;
                
            case 15:	// Condensed on
                flush_buf();
                mode_condensed=1;
                set_font(1);
                break;
            
            case 18:	// Condensed off
                flush_buf();
                mode_condensed=0;
                set_font(1);
                break;
                
            case 20:	// Double-wide off;
                flush_buf();
                mode_doublewide=0;
                mode_doublewide_reset=0;
                set_font(1);
                break;
            
            case 24:
                bp=buf;	// Cancel line -- BROKEN
                break;
            
            case '\033':// ESC
                state=2;
                break;
            default:
                if ((unsigned char)ch<32)
                {
                    flush_buf();
                    *bp++='^';
                    *bp++=ch+64;
                    goto perror;
                }
                *bp++=charsets[curcharset][(unsigned char)ch];
                if (bp>=buf+BUFFER_SIZE)
                    flush_buf();
            }
            break;
//----------------------------------------------------------------------------
        case 1:	// Backspace
            state=0;
            if (bp==buf)
            {
#define prev (*(bp-1))
                backspace:
                flush_buf();
                x-=chx;
                if (x<0)
                    x=0;
                goto normal;
            }
            if (charsets[curcharset][(unsigned char)ch]==prev && !mode_bold)
            {
                last=prev;
                bp--;
                flush_buf();
                mode_bold=1;
                set_font(0);
                *bp++=last;
                flush_buf();
                mode_bold=0;
                set_font(0);
                break;
            }
            switch(ch)
            {
            case ',':
                switch(prev)
                {
                case 'a': prev=0x0161; break;
                case 'A': prev=0x0104; break;
                case 'e': prev=0x0119; break;
                case 'E': prev=0x0118; break;
                default: goto backspace;
                }
                break;
            case '\'':
                switch(prev)
                {
                case 'c': prev=0x0107; break;
                case 'C': prev=0x0106; break;
                case 'o': prev=0x00F3; break;
                case 'O': prev=0x00D3; break;
                case 'n': prev=0x0144; break;
                case 'N': prev=0x0143; break;
                case 'z': prev=0x017A; break;
                case 'Z': prev=0x0179; break;
                case 's': prev=0x015B; break;
                case 'S': prev=0x015A; break;
                default: goto backspace;
                }
                break;
            case '-':
                switch(prev)
                {
                case 'z': prev=0x017C; break;
                case 'Z': prev=0x017B; break;
                default: goto backspace;
                }
                break;
            case '/':
                switch(prev)
                {
                case 'l': prev=0x0142; break;
                case 'L': prev=0x0141; break;
                default: goto backspace;
                }
                break;
            default:
                goto backspace;
            }
            break;
//----------------------------------------------------------------------------
        case 2:  // ESC
            state=0;
            switch(ch)
            {
            case 14:	// ESC #14: double-wide on (1 line)
                flush_buf();
                mode_doublewide=1;
                mode_doublewide_reset=1;
                set_font(1);
                break;
            case 15:	// ESC #15: condensed on
                flush_buf();
                mode_condensed=1;
                set_font(1);
                break;
            case '-':	// ESC -: underline on/off (arg)
                state=4;
                break;
            case '4':	// ESC 4: italic on
                flush_buf();
                mode_italic=1;
                set_font(0);
                break;
            case '5':	// ESC 5: italic off
                flush_buf();
                mode_italic=0;
                set_font(0);
                break;
            case ':':	// ESC :: ROM->RAM [ignore]
                break;
            case '@':	// ESC @: reset
                reset();
                set_font(1);
                break;
            case 'E':	// ESC E: emphasize [bold] on
            case 'G':	// ESC G: double-strike [bold] on
                flush_buf();
                mode_bold=1;
                set_font(0);
                break;
            case 'F':	// ESC F: emphasize [bold] off
            case 'H':	// ESC H: double-strike [bold] off
                flush_buf();
                mode_bold=0;
                set_font(0);
                break;
            case 'M':	// ESC M: 12 CPI
                flush_buf();
                mode_cpi=12;
                set_font(1);
                break;
            case 'P':	// ESC P: 10 CPI
                flush_buf();
                mode_cpi=10;
                set_font(1);
                break;
            case 'S':	// ESC S: super/subscript (arg)
                state=4;
                break;
            case 'T':	// ESC T: sscript off
                flush_buf();
                mode_sscript=0;
                set_font(0);
                break;
            case 'W':	// ESC W: double-wide on/off (arg)
                state=5;
                break;
            case 'g':	// ESC g: 15 CPI
                flush_buf();
                mode_cpi=15;
                set_font(1);
                break;
            case 'p':	// ESC p: proportional on/off (arg)
                state=6;
                break;
            case 'w':	// ESC w: double-high on/off (arg)
                state=7;
                break;
            case 'x':	// ESC x: NLQ/draft on/off (arg) [ignore]
                state=8;
                break;
            default:
                flush_buf();
                *bp++='E';
                *bp++='S';
                *bp++='C';
                *bp++=' ';
                if ((unsigned char)ch<32)
                {
                    *bp++='^';
                    *bp++=ch+64;
                }
                else
                    *bp++=(unsigned char)ch;
                goto perror;
            }
            break;
//----------------------------------------------------------------------------
        case 3:		// ESC - (underline)
            state=0;
            flush_buf();
            switch(ch)
            {
            case 0:
            case '0':
                mode_underline=0;
                set_font(0);
                break;
            case 1:
            case '1':
                mode_underline=1;
                set_font(0);
                break;
            default:
                *bp++='E';
                *bp++='S';
                *bp++='C';
                *bp++=' ';
                *bp++='-';
                *bp++=ch;
                goto perror;
            }
            break;
//----------------------------------------------------------------------------
        case 4:		// ESC S (super/subscript)
            state=0;
            flush_buf();
            switch(ch)
            {
            case 0:
            case '0':
                mode_sscript=-1;
                set_font(0);
                break;
            case 1:
            case '1':
                mode_sscript=1;
                set_font(0);
                break;
            default:
                *bp++='E';
                *bp++='S';
                *bp++='C';
                *bp++=' ';
                *bp++='S';
                *bp++=ch;
                goto perror;
            }
            break;
//----------------------------------------------------------------------------
        case 5:		// ESC W (double-wide)
            state=0;
            flush_buf();
            switch(ch)
            {
            case 0:
            case '0':
                mode_doublewide=0;
                mode_doublewide_reset=0;
                set_font(1);
                break;
            case 1:
            case '1':
                mode_doublewide=1;
                mode_doublewide_reset=0;
                set_font(1);
                break;
            default:
                *bp++='E';
                *bp++='S';
                *bp++='C';
                *bp++=' ';
                *bp++='W';
                *bp++=ch;
                goto perror;
            }
            break;
//----------------------------------------------------------------------------
        case 6:		// ESC p (proportional)
            state=0;
            flush_buf();
            switch(ch)
            {
            case 0:
            case '0':
                mode_proportional=0;
                set_font(1);
                break;
            case 1:
            case '1':
                mode_proportional=1;
                set_font(1);
                break;
            default:
                *bp++='E';
                *bp++='S';
                *bp++='C';
                *bp++=' ';
                *bp++='p';
                *bp++=ch;
                goto perror;
            }
            break;
//----------------------------------------------------------------------------
        case 7:		// ESC w (double-high)
            state=0;
            flush_buf();
            switch(ch)
            {
            case 0:
            case '0':
                mode_doublehigh=0;
                set_font(1);
                break;
            case 1:
            case '1':
                mode_doublehigh=1;
                set_font(1);
                break;
            default:
                *bp++='E';
                *bp++='S';
                *bp++='C';
                *bp++=' ';
                *bp++='w';
                *bp++=ch;
                goto perror;
            }
            break;
//----------------------------------------------------------------------------
        case 8:		// ESC x (NLQ)
            state=0;
            flush_buf();
            switch(ch)
            {
            case 0:
            case '0':
                break;
            case 1:
            case '1':
                break;
            default:
                *bp++='E';
                *bp++='S';
                *bp++='C';
                *bp++=' ';
                *bp++='x';
                *bp++=ch;
                goto perror;
            }
            break;
//----------------------------------------------------------------------------
        perror:
            SetTextColor(dc, 0x0000ff);
            flush_buf();
            SetTextColor(dc, 0x000000);
            state=0;
        }
    }
    flush_buf();
    
    DeleteObject(SelectObject(dc,oldfont));
    oldfont=0;
    reset();

#ifdef MARGIN
    {
        HPEN oldpen=SelectObject(dc, CreatePen(PS_DOT, 1, 0xf0f0f0));
        MoveToEx(dc, MARGINX,0, 0);
        LineTo(dc, MARGINX,-MARGINY);
        LineTo(dc, 0,-MARGINY);
        DeleteObject(SelectObject(dc, oldpen));
    }
#endif
    SetMapMode(dc, oldmm);
}

static int parse_charset(char *chset)
{
    if (strcmpi(chset, "IBM")==0)
        curcharset=0;
    else if (strcmpi(chset, "437")==0)
        curcharset=0;
    else if (strcmpi(chset, "MAZ")==0)
        curcharset=1;
    else if (strcmpi(chset, "MAZOVIA")==0)
        curcharset=1;
    else if (strcmpi(chset, "WIN")==0)
        curcharset=2;
    else if (strcmpi(chset, "1250")==0)
        curcharset=2;
    else if (strcmpi(chset, "CP1250")==0)
        curcharset=2;
    else if (strcmpi(chset, "LAT")==0)
        curcharset=3;
    else if (strcmpi(chset, "LATIN2")==0)
        curcharset=3;
    else if (strcmpi(chset, "LAT2")==0)
        curcharset=3;
    else if (strcmpi(chset, "852")==0)
        curcharset=3;
    else if (strcmpi(chset, "CP852")==0)
        curcharset=3;
    else if (strcmpi(chset, "ISO")==0)
        curcharset=4;
    else if (strcmpi(chset, "8859-2")==0)
        curcharset=4;
    else
        return 0;
    return 1;
}

void print(HDC _dc, char *text, char *charset)
{
    dc=_dc;

    if (!charsets[1])
    {
        build_mazovia();
        charsets[1]=charset_mazovia;
    }
    
    curcharset=3;


    if (charset && *charset)
        if (!parse_charset(charset))
            text="Z≥y parametr: zestaw znakÛw.", curcharset=4;
    
    paint(text);
}
