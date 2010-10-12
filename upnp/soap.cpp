#include <stdio.h>

namespace soap
{
    enum
    {
        cht_oth = 0,
        cht_al  = 1,
        cht_num = 2,
        cht_sp  = 4,
        cht_s   = 8
    };

    const char types[256]=
    {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x04,0x00,0x00,0x04,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x04,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
        0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x08,0x08,0x08,0x08,0x08,0x08,
        0x08,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
        0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x08,0x08,0x08,0x08,0x08,
        0x08,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
        0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x08,0x08,0x08,0x08,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
    };

    class ctx
    {
    protected:
        int st;
        int st_close_tag;
        int st_quot;
        int err;

        enum { max_tok_size=64 };

        char tok[max_tok_size];
        int tok_size;

        void tok_add(unsigned char ch)
        {
            if(tok_size<max_tok_size-1)
                ((unsigned char*)tok)[tok_size++]=ch;
        }
        void tok_reset(void)
        {
            tok_size=0;
        }
        void tok_push(void)
        {
            tok[tok_size]=0;

            if(st==40)
            {
                if(tok_size)
                    printf("[%i,'%s']",0,tok);
                printf("[%i,'%s']",1,"");
            }else
                printf("[%i,'%s']",st_close_tag,tok);

            tok_size=0;
            st_close_tag=0;
            st_quot=0;
            st=0;
        }
        void ch_push(unsigned char ch)
        {
            printf("%c",ch);
        }

    public:
        int line;
    public:
        ctx(void):st(0),err(0),line(0),tok_size(0),st_close_tag(0),st_quot(0) {}

        int parse(const char* buf,int len);
    };
}


int soap::ctx::parse(const char* buf,int len)
{
    for(int i=0;i<len && !err;i++)
    {
        unsigned char ch=((unsigned char*)buf)[i];

        int cht=types[ch];

        if(ch=='\n')
            line++;

        switch(st)
        {
        case 0:
            if(ch=='<')
                st=10;
            else
                ch_push(ch);
            break;
        case 10:
            if(ch=='/')
            {
                st=20;
                st_close_tag=1;
                continue;
            }
            st=11;
        case 11:
            if(cht!=cht_sp)
            {
                if(cht==cht_al)
                {
                    tok_add(ch);
                    st=20;
                }else
                    err=1;
            }
            break;
        case 20:
            if(cht==cht_sp)
                continue;
            st=21;
        case 21:
            if(ch==':')
                tok_reset();
            else if(cht==cht_sp)
            {
                tok_push();
                st=30;
            }else if(cht&(cht_al|cht_num) || ch=='_' || ch=='-')
                tok_add(ch);
            else if(ch=='/')
                st=40;
            else if(ch=='>')
                tok_push();
            else
                err=1;
            break;
        case 30:
            if(st_quot)
            {
                if(ch=='\"')
                    st_quot=0;
            }else
            {
                if(ch=='\"')
                    st_quot=1;
                else if(ch=='/')
                    st=40;
                else if(ch=='>')
                    st=0;
            }
            break;
        case 40:
            if(ch!='>')
                err=1;
            else
                tok_push();
            break;
        }
    }

    return err;
}


int main(void)
{
    static const char s[]=
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "       <s:Body>\n"
        "               <u:Browse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">\n"
        "                       <ObjectID>0</ObjectID>\n"
        "                       <BrowseFlag>BrowseDirectChildren</BrowseFlag>\n"
        "                       <Filter>*</Filter>\n"
        "                       <StartingIndex>0</StartingIndex>\n"
        "                       <RequestedCount>0</RequestedCount>\n"
        "                       <SortCriteria></SortCriteria>\n"
        "                       <staff/>\n"
        "                       <  staff2  />\n"
        "               </u:Browse>\n"
        "       </s:Body>\n"
        "</s:Envelope>\n";

    soap::ctx ctx;

    int n=ctx.parse(s,sizeof(s)-1);

    if(n)
        printf("\n\nerror in line %i\n",ctx.line+1);

    return 0;
}
