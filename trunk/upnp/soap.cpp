#include "soap.h"
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

    void string::clear(void)
    {
        if(ptr)
        {
            free(ptr);
            ptr=0;
        }
        size=0;
    }

    void string::swap(string& s)
    {
        char* pp=s.ptr;
        int ss=s.size;
        s.ptr=ptr; s.size=size;
        ptr=pp; size=ss;
    }

    void string::trim_right(void)
    {
#ifndef NO_TRIM_DATA
        if(ptr && size>0)
        {
            for(;size>0 && types[ptr[size-1]]==cht_sp;size--);
            ptr[size]=0;
        }
#endif
    }

    int string_builder::add_chunk(void)
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

    void string_builder::clear(void)
    {
        while(beg)
        {
            chunk* tmp=beg;
            beg=beg->next;
            free(tmp);
        }
        beg=end=0;
    }

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

    node* node::add_node(void)
    {
        node* p=(node*)malloc(sizeof(node));
        if(!p)
            return 0;
    
        p->init();
        p->parent=this;
    
        if(!beg)
            beg=end=p;
        else
        {
            end->next=p;
            end=p;
        }

        return p;
    }

    void node::clear(void)
    {
printf("%s=%s\n",name,data);

        if(name) { free(name); name=0; }

        if(data) { free(data); data=0; len=0; }

        while(beg)
        {
printf("%s\n",beg->name);
            node* p=beg;

            beg=beg->next;

            p->clear();

            free(p);
        }
printf("------\n");
        beg=end=0;
    }
    
}


void soap::ctx::begin(void)
{
    st=0;
    err=0;
    line=0;
    tok_size=0;
    st_close_tag=0;
    st_quot=0;
    st_text=0;
}

int soap::ctx::end(void)
{
    if(tok_size>0 || st)
    {
        err=1;
        return -1;
    }

    data_push();

    return 0;
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
            {
                st=10;
                data_push();
                tok_reset();
                st_text=0;
            }else
            {
                switch(st_text)
                {
                case 0:
#ifndef NO_TRIM_DATA
                    if(cht==cht_sp) continue;
#endif
                    st_text=10;
                case 10:
                    if(ch=='&') { tok_add(ch); st_text=20; } else ch_push(ch);
                    break;
                case 20:
                    tok_add(ch);
                    if(ch==';')
                    {
                        tok[tok_size]=0;

                        if(!strcmp(tok,"&lt;")) ch_push('<');
                        else if(!strcmp(tok,"&gt;")) ch_push('>');
                        else if(!strcmp(tok,"&apos;")) ch_push('\'');
                        else if(!strcmp(tok,"&quot;")) ch_push('\"');
                        else if(!strcmp(tok,"&amp;")) ch_push('&');
                        tok_reset();
                        st_text=10;
                    }                    
                    break;
                }
            }
            break;
        case 10:
            if(ch=='/')
            {
                st=20;
                st_close_tag=1;
                continue;
            }else if(ch=='?')
            {
                st=50;
                continue;
            }else if(ch=='!')
            {
                st=60;
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
            if(cht==cht_sp) continue; st=21;
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
            if(ch!='>') err=1; else tok_push();
            break;
        case 50:
            if(ch=='?') st=55;
            break;
        case 55:
            if(ch=='>') st=0; else if(ch!='?') st=50;                
            break;
        case 60:
            if(ch=='-') st=61; else err=1;
            break;
        case 61:
            if(ch=='-') st=62; else err=1;
            break;
        case 62:
            if(ch=='-') st=63;
            break;
        case 63:
            if(ch=='-') st=64; else st=62;
            break;
        case 64:
            if(ch=='>')
                st=0;
            else if(ch!='-')
                st=62;
            break;
        }
    }

    return err;
}




int main(void)
{
    static const char s[]=
        "<!-- comment -->\n"
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
        "                       <staff3>Hello &lt;&gt;&aaa; World</staff3>\n"
        "               </u:Browse>\n"
        "       </s:Body>\n"
        "</s:Envelope>\n";

    soap::node root;

    soap::ctx ctx(&root);

    ctx.begin();

    if(ctx.parse(s,sizeof(s)-1) || ctx.end())
        printf("\n\nerror in line %i\n",ctx.line+1);

    return 0;
}
