#include <windows.h>
#include "printdoc.h"

struct pr_page *printdoc;

void printdoc_draw(HDC dc, int npage, float s, int layout)
{
    struct pr_page *page;
    struct pr_line *line;
    int oldmm;
    LOGFONT lf;
    HFONT oldfont;
    int i;

    if (npage<1)
        return;
    page=printdoc;
    while(--npage)
        if (page)
            page=page->next;
        else
            return;
    if (!page)
        return;

    oldmm=GetMapMode(dc);
    SetMapMode(dc, MM_TWIPS);
    SetBkMode(dc, TRANSPARENT);
    if (layout>0)
    {
        s=s*207.5/297.0;	// naprawdê 210/297
        SetTextAlign(dc, TA_LEFT|TA_BOTTOM);
    }
    if (layout>1)
        s=s*77.5/80.0;

#if 0    
    {
        int x,y;
        for(x=0;x<10;x++)
            for(y=0;y<10;y++)
                Rectangle(dc, x*1600, -y*1600-1500, x*1600+1500, -y*1600);
    }
#endif

    oldfont=0;
    line=page->lines;
    
    while (line)
    {
        if (line->flags!=-1)
        {
            memset(&lf,0,sizeof(LOGFONT));
            lf.lfHeight=line->chy*s;
            lf.lfWidth=line->chx*s;
            lf.lfPitchAndFamily=FIXED_PITCH;
            lf.lfQuality=5/*ANTIALIASED_QUALITY*/;
            strcpy(lf.lfFaceName,"Courier New");
            if (layout>0)
                lf.lfEscapement=lf.lfOrientation=900;
            if (line->flags&PF_BOLD)
                lf.lfWeight=FW_BOLD;
            if (line->flags&PF_UNDERLINE)
                lf.lfUnderline=1;
            if (line->flags&PF_ITALIC)
                lf.lfItalic=1;
            if (line->flags&PF_ERROR)
                SetTextColor(dc, 0x0000ff);
            else
                SetTextColor(dc, 0);
            if (oldfont)
                DeleteObject(SelectObject(dc,CreateFontIndirect(&lf)));
            else
                oldfont=SelectObject(dc,CreateFontIndirect(&lf));
        }
        for(i=0;i<line->len;i++)
            if (line->text[i]!=32)
                switch(layout)
                {
                default:
                case 0:
                    TextOutW(dc, line->x*s+i*(int)(line->chx*s), -line->y*s, line->text+i, 1);
                    break;
                case 1:
                    TextOutW(dc, 11905-4*241-line->y*s, -(line->x*s+i*(int)(line->chx*s)), line->text+i, 1);
                    break;
                case 2:
                    TextOutW(dc, 11905-5*241-line->y*s, -(line->x*s+i*(int)(line->chx*s)), line->text+i, 1);
                    break;
                case 3:
                    TextOutW(dc, 11905-5*241-line->y*s, -(line->x*s+i*(int)(line->chx*s))-85*144*s, line->text+i, 1);
                    break;
                }
        line=line->next;
    }

    if (oldfont)
        DeleteObject(SelectObject(dc, oldfont));
    SetMapMode(dc, oldmm);
}


void printdoc_drawmem(HDC dc, int npage, float s, int layout)
{
    HDC memdc;
    HBITMAP bmp;
    int sx, sy;
    RECT rect;
    
    sx=GetDeviceCaps(dc, HORZRES);
    sy=GetDeviceCaps(dc, VERTRES);
    memdc=CreateCompatibleDC(dc);
    bmp=CreateCompatibleBitmap(memdc, sx, sy);
    SelectObject(memdc, bmp);
    rect.left=0;
    rect.top=0;
    rect.right=sx;
    rect.bottom=sy;
    FillRect(memdc, &rect, GetStockObject(WHITE_BRUSH));
    printdoc_draw(memdc, npage, s, layout);
    BitBlt(dc, 0, 0, sx, sy, memdc, 0, 0, SRCAND);
    DeleteDC(memdc);
    DeleteObject(bmp);
}


void printdoc_draw2(HDC dc, int npage, float s)
{
    struct pr_page *page;
    struct pr_line *line;
    int oldmm;
    LOGFONT lf;
    HFONT oldfont;
    int i;
    HDC cdc;
    HBITMAP bmp;

    if (npage<1)
        return;
    page=printdoc;
    while(--npage)
        if (page)
            page=page->next;
        else
            return;
    if (!page)
        return;

    cdc=CreateCompatibleDC(dc);
    bmp=CreateCompatibleBitmap(dc, 30, 50);
    SelectObject(cdc, bmp);
    
    oldmm=GetMapMode(dc);
    SetMapMode(dc, MM_TWIPS);
    SetBkMode(dc, TRANSPARENT);
    SetStretchBltMode(dc, /*BLACKONWHITE*/HALFTONE);

    oldfont=0;
    line=page->lines;
    
    while (line)
    {
        if (line->flags!=-1)
        {
            memset(&lf,0,sizeof(LOGFONT));
            lf.lfHeight=50;
            lf.lfWidth=30;
            lf.lfPitchAndFamily=FIXED_PITCH;
            lf.lfQuality=NONANTIALIASED_QUALITY;
            strcpy(lf.lfFaceName,"Courier New");
            if (line->flags&PF_BOLD)
                lf.lfWeight=FW_BOLD;
            if (line->flags&PF_UNDERLINE)
                lf.lfUnderline=1;
            if (line->flags&PF_ITALIC)
                lf.lfItalic=1;
            if (line->flags&PF_ERROR)
                SetTextColor(dc, 0x0000ff);
            else
                SetTextColor(dc, 0);
            if (oldfont)
                DeleteObject(SelectObject(cdc,CreateFontIndirect(&lf)));
            else
                oldfont=SelectObject(cdc,CreateFontIndirect(&lf));
        }
        for(i=0;i<line->len;i++)
            if (line->text[i]!=32)
            {
                TextOutW(cdc, 0, 0, line->text+i, 1);
                StretchBlt(dc, line->x*s+i*(int)(line->chx*s), -line->y*s, line->chx*s, -line->chy*s, 
                          cdc, 0, 0, 30, 50, SRCCOPY);
            }
        line=line->next;
    }

    if (oldfont)
        DeleteObject(SelectObject(cdc, oldfont));
    SetMapMode(dc, oldmm);
    DeleteDC(cdc);
    DeleteObject(bmp);
}

#if 0
void printdoc_html(int npage, float s)
{
    struct pr_page *page;
    struct pr_line *line;
    int i;
    int fl;

    if (npage<1)
        return;
    page=printdoc;
    while(--npage)
        if (page)
            page=page->next;
        else
            return;
    if (!page)
        return;

    line=page->lines;
    
    while (line)
    {
        if (line->flags!=-1)
            fl=line->flags;
        for(i=0;i<line->len;i++)
            if (line->text[i]!=32)
                printf("<img style='position: absolute; left:%dpx; top:%dpx;' src='http://test.angband.pl/c.cgi?%%u%x' alt='%C' width=%d height=%d>\n",
                    (int)(line->x*s+i*(int)(line->chx*s))/20, (int)(line->y*s)/20, 
                    line->text[i], line->text[i], line->chx/20, line->chy/20);
        line=line->next;
    }

}
#endif
