#include <stdio.h>
#include <stdarg.h>
#include <strings.h>
#include <stdlib.h>
#include "charsets.h"
#include "printdoc.h"

#define DPI 1440
#define CH_HEIGHT 241

static int curcharset = 1;

static wchar_t* charsets[]=
{
    charset_437,
    charset_mazovia,
    charset_cp1250,
    charset_cp852,
    charset_8859_2,
};

#define BUFFER_SIZE 256
static wchar_t buf[BUFFER_SIZE],*bp;
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
static int mode_error;

static int chx,chy;
static int chflags;
static int page;
static int in_page;
static int samefont;
static struct pr_page *lastpage;
static struct pr_line *lastline;

struct pr_page *printdoc=0;

void printdoc_free()
{
    void *p;

    if (!printdoc)
        return;
    while(printdoc)
    {
        while(printdoc->lines)
        {
            p=printdoc->lines->next;
            free(printdoc->lines);
            printdoc->lines=p;
        }
        p=printdoc->next;
        free(printdoc);
        printdoc=p;
    }
}

static void flush_buf()
{
    struct pr_line **lptr;

    if (bp==buf)
        return;
    if (!in_page)
    {
        page++;
        in_page=1;
        if (lastpage)
            lastpage=lastpage->next=malloc(sizeof(struct pr_page));
        else
            lastpage=printdoc=malloc(sizeof(struct pr_page));
        lastpage->lines=0;
        lastpage->next=0;
        lastline=0;
        samefont=0;
    }
    if (lastline)
        lptr=&lastline->next;
    else
        lptr=&lastpage->lines;
    lastline=*lptr=malloc(sizeof(struct pr_line)+(bp-buf)*sizeof(wchar_t));
    lastline->x=x;
    lastline->y=y+(mode_sscript==1?chy/2:0);
    lastline->chx=chx;
    lastline->chy=chy;
    lastline->len=(bp-buf);
    if (samefont)
        lastline->flags=-1;
    else
        lastline->flags=chflags, samefont=1;
    memcpy(lastline->text, buf, (bp-buf)*sizeof(wchar_t));
    lastline->next=0;

    x+=chx*(bp-buf);
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
    mode_error=0;
}

static void set_font()
{
    chy=CH_HEIGHT;
    chx=DPI
        *(mode_doublewide?2:1)
        /(mode_condensed?(mode_cpi==12?20:17):mode_cpi);
    chflags=0;
    if (mode_bold)
        chflags|=PF_BOLD;
    if (mode_underline)
        chflags|=PF_UNDERLINE;
    if (mode_italic)
        chflags|=PF_ITALIC;
    if (mode_error||mode_proportional)
        chflags|=PF_ERROR;
    if (mode_doublehigh)
        chy*=2;
    if (mode_sscript)
        chy/=2;
    samefont=0;
}

static int state;

static void printdoc_start()
{
    page=0;
    in_page=0;
    printdoc_free();
    lastpage=0;
    lastline=0;
    state=0;
    bp=buf;
    x=y=ly=0;
    reset();
    set_font();
}

static void printdoc_char(char ch)
{
    wchar_t last;

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
                set_font();
            }
            if ((ly+=1+!!mode_doublehigh)>=66)
                goto formfeed;
            break;
        
        case '\v':	// Vertical tab
            flush_buf();
            ly=((ly/8)+1)*8-1;
            y=ly*(chy/(1+!!mode_doublehigh));
            goto newline;
        
        case '\f':	// Form Feed
        formfeed:
            flush_buf();
            in_page=0;
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
            set_font();
            break;
            
        case 15:	// Condensed on
            flush_buf();
            mode_condensed=1;
            set_font();
            break;
        
        case 18:	// Condensed off
            flush_buf();
            mode_condensed=0;
            set_font();
            break;
            
        case 20:	// Double-wide off;
            flush_buf();
            mode_doublewide=0;
            mode_doublewide_reset=0;
            set_font();
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
            set_font();
            *bp++=last;
            flush_buf();
            mode_bold=0;
            set_font();
            break;
        }
        switch(ch)
        {
        case ',':
            switch(prev)
            {
            case 'a': prev=0x0105; break;
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
#undef prev
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
            set_font();
            break;
        case 15:	// ESC #15: condensed on
            flush_buf();
            mode_condensed=1;
            set_font();
            break;
        case '-':	// ESC -: underline on/off (arg)
            state=4;
            break;
        case '4':	// ESC 4: italic on
            flush_buf();
            mode_italic=1;
            set_font();
            break;
        case '5':	// ESC 5: italic off
            flush_buf();
            mode_italic=0;
            set_font();
            break;
        case ':':	// ESC :: ROM->RAM [ignore]
            break;
        case '@':	// ESC @: reset
            reset();
            set_font();
            break;
        case 'E':	// ESC E: emphasize [bold] on
        case 'G':	// ESC G: double-strike [bold] on
            flush_buf();
            mode_bold=1;
            set_font();
            break;
        case 'F':	// ESC F: emphasize [bold] off
        case 'H':	// ESC H: double-strike [bold] off
            flush_buf();
            mode_bold=0;
            set_font();
            break;
        case 'M':	// ESC M: 12 CPI
            flush_buf();
            mode_cpi=12;
            set_font();
            break;
        case 'P':	// ESC P: 10 CPI
            flush_buf();
            mode_cpi=10;
            set_font();
            break;
        case 'S':	// ESC S: super/subscript (arg)
            state=4;
            break;
        case 'T':	// ESC T: sscript off
            flush_buf();
            mode_sscript=0;
            set_font();
            break;
        case 'W':	// ESC W: double-wide on/off (arg)
            state=5;
            break;
        case 'g':	// ESC g: 15 CPI
            flush_buf();
            mode_cpi=15;
            set_font();
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
            set_font();
            break;
        case 1:
        case '1':
            mode_underline=1;
            set_font();
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
            set_font();
            break;
        case 1:
        case '1':
            mode_sscript=1;
            set_font();
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
            set_font();
            break;
        case 1:
        case '1':
            mode_doublewide=1;
            mode_doublewide_reset=0;
            set_font();
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
            set_font();
            break;
        case 1:
        case '1':
            mode_proportional=1;
            set_font();
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
            set_font();
            break;
        case 1:
        case '1':
            mode_doublehigh=1;
            set_font();
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
        mode_error=1;
        set_font();
        flush_buf();
        mode_error=0;
        set_font();
        state=0;
    }
}

int printdoc_charset(char *chset)
{
    if (strcasecmp(chset, "IBM")==0)
        curcharset=0;
    else if (strcasecmp(chset, "437")==0)
        curcharset=0;
    else if (strcasecmp(chset, "MAZ")==0)
        curcharset=1;
    else if (strcasecmp(chset, "MAZOVIA")==0)
        curcharset=1;
    else if (strcasecmp(chset, "WIN")==0)
        curcharset=2;
    else if (strcasecmp(chset, "1250")==0)
        curcharset=2;
    else if (strcasecmp(chset, "CP1250")==0)
        curcharset=2;
    else if (strcasecmp(chset, "LAT")==0)
        curcharset=3;
    else if (strcasecmp(chset, "LATIN2")==0)
        curcharset=3;
    else if (strcasecmp(chset, "LAT2")==0)
        curcharset=3;
    else if (strcasecmp(chset, "852")==0)
        curcharset=3;
    else if (strcasecmp(chset, "CP852")==0)
        curcharset=3;
    else if (strcasecmp(chset, "ISO")==0)
        curcharset=4;
    else if (strcasecmp(chset, "8859-2")==0)
        curcharset=4;
    else
        return 0;
    return 1;
}

int printdoc_string(char *text, char *charset)
{
    if (charset && *charset)
        if (!printdoc_charset(charset))
            return -1;
    
    printdoc_start();
    while(*text)
        printdoc_char(*text++);
    flush_buf();
    return page;
}

int printdoc_f(FILE *f, char *charset)
{
    char ch;
    
    if (charset && *charset)
        if (!printdoc_charset(charset))
            return -1;
    
    if (!f)
        return -1;
    
    printdoc_start();
    while((ch=getc(f))!=EOF)
        printdoc_char(ch);
    flush_buf();
    return page;
}

int printdoc_file(char *filename, char *charset)
{
    FILE *f;
    int res;
    
    f=fopen(filename, "r");
    if (!f)
        return -1;
    
    res=printdoc_f(f, charset);
    fclose(f);
    return res;
}
