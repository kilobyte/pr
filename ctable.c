#include <stdio.h>
#include <iconv.h>

void ctable(char *charset)
{
    iconv_t ic,icc;
    int a;
    short b;
    char c;
    char *ap, *bp;
    int ac, bc;
    
    if (!(ic=iconv_open("ucs2", charset)))
    {
        fprintf(stderr, "Can't iconv_open.\n");
        return;
    }
    if (!(icc=iconv_open("cp1250", charset)))
    {
        fprintf(stderr, "Can't iconv_open.\n");
        return;
    }
    printf("wchar_t charset_%s[]={\n", charset);
    for(a=0;a<256;a++)
    {
        b=0;
        ap=(char*)&a;
        bp=(char*)&b;
        ac=1;
        bc=2;
        iconv(ic, &ap, &ac, &bp, &bc);
        ap=(char*)&a;
        bp=(char*)&c;
        ac=1;
        bc=1;
        iconv(icc, &ap, &ac, &bp, &bc);
        if (bc && a>=32)
            printf("/* %02X */ 0x%04X,\n", a, b);
        else
            printf("/* %02X */ 0x%04X, /* %c */\n", a, b, c);
    }
    printf("};\n");
    iconv_close(ic);
    iconv_close(icc);
}

int main()
{
    ctable("437");
    ctable("cp1250");
    ctable("8859_2");
    ctable("cp852");
    return 0;
}
