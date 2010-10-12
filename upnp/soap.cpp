#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    class string
    {
    protected:
        char* ptr;
        int size;
    public:
        string(void):ptr(0),size(0) {}
        ~string(void) { clear(); }

        void clear(void)
        {
            if(ptr)
            {
                free(ptr);
                ptr=0;
            }
            size=0;
        }

        int length(void) const { return size; }
        const char* c_str(void) const { return ptr?ptr:""; }

        void swap(string& s)
        {
            s.clear();
            s.ptr=ptr; s.size=size;
            ptr=0; size=0;
        }

        friend class string_builder;
    };

    struct chunk
    {
        enum { max_size=64 };

        char* ptr;
        int size;

        chunk* next;
    };

    class string_builder
    {
    protected:
        chunk *beg,*end;

        int add_chunk(void)
        {
            chunk* p=(chunk*)malloc(sizeof(chunk)+chunk::max_size);
            if(!p)
                return -1;

            p->ptr=(char*)(p+1);
            p->size=0;
            p->next=0;

            if(!beg)
                beg=end=p;
            else
            {
                end->next=p;
                end=p;
            }
            return 0;
        }
    public:
        string_builder(void):beg(0),end(0) {}
        ~string_builder(void) { clear(); }

        void clear(void)
        {
            while(beg)
            {
                chunk* tmp=beg;
                beg=beg->next;
                free(tmp);
            }
            beg=end=0;
        }

        void add(int ch)
        {
            if(!end || end->size>=chunk::max_size)
                if(add_chunk())
                    return;
            ((unsigned char*)end->ptr)[end->size++]=ch;
        }

        void add(const char* s,int len);

        void swap(string& s);
    };

    void string_builder::add(const char* s,int len)
    {
        if(!s)
            return;
        if(len==-1)
            len=strlen(s);
        if(!len)
            return;

        while(len>0)
        {
            int n=end?(chunk::max_size-end->size):0;

            if(n<=0)
            {
                if(add_chunk())
                    return;
                n=chunk::max_size;
            }
    
            int m=len>n?n:len;
            memcpy(end->ptr+end->size,s,m);
            end->size+=m;
            len-=m;
            s+=m;
        }
    }

    void string_builder::swap(string& s)
    {
        int len=0;

        for(chunk* p=beg;p;p=p->next)
            len+=p->size;

        s.clear();

        if(len>0)
        {
            char* ptr=(char*)malloc(len+1);
            if(ptr)
            {
                char* pp=ptr;

                for(chunk* p=beg;p;p=p->next)
                {
                    memcpy(pp,p->ptr,p->size);
                    pp+=p->size;
                }

                *pp=0;

                s.ptr=ptr;
                s.size=len;
            }
        }

        clear();
    }


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
