#define PF_BOLD		0x01
#define PF_UNDERLINE	0x02
#define PF_ITALIC	0x04
#define PF_ERROR	0x08

struct pr_line
{
    struct pr_line *next;
    int x,y,chx,chy,flags,len;
    wchar_t text[0];
};

struct pr_page
{
    struct pr_page *next;
    struct pr_line *lines;
};
