#ifndef __SOAP_H
#define __SOAP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace soap
{
    class string
    {
    protected:
        char* ptr;
        int size;
    public:
        string(void):ptr(0),size(0) {}
        ~string(void) { clear(); }

        void clear(void);

        int length(void) const { return size; }
        const char* c_str(void) const { return ptr?ptr:""; }

        void swap(string& s);

        void trim_right(void);

        friend class string_builder;
        friend class ctx;
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

        int add_chunk(void);

    public:
        string_builder(void):beg(0),end(0) {}
        ~string_builder(void) { clear(); }

        void clear(void);

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


    struct node
    {
        node* parent;
        node* next;
        node* beg;
        node* end;

        char* name;
        char* data;
        int   len;

        node(void):parent(0),next(0),beg(0),end(0),name(0),data(0),len(0) {}
        ~node(void) { clear(); }

        void init(void) { parent=next=beg=end; name=data=0; len=0; }

        node* add_node(void);

        void clear(void);
    
    };


    class ctx
    {
    protected:
        node* cur;

        short st;
        short st_close_tag;
        short st_quot;
        short st_text;
        short err;

        string_builder data;

        enum { max_tok_size=64 };

        char tok[max_tok_size];
        int tok_size;

        void tok_add(unsigned char ch)
        {
            if(tok_size<max_tok_size-1)
                ((unsigned char*)tok)[tok_size++]=ch;
        }
        void tok_reset(void) { tok_size=0; }

        void ch_push(unsigned char ch) { data.add(ch); }

        void tok_push(void)
        {
            tok[tok_size]=0;

            if(st==40)
            {
                if(tok_size)
                    tag_open(tok,tok_size);
                tag_close("",0);
            }else
            {
                if(st_close_tag)
                    tag_close(tok,tok_size);
                else
                    tag_open(tok,tok_size);
            }

            tok_size=0;
            st_close_tag=0;
            st_quot=0;
            st=0;
        }

        void tag_open(const char* s,int len)
        {
            if(!len) { err=1; return; }

            node* p=cur->add_node();

            if(!p) { err=1; return; }

            p->name=(char*)malloc(len+1);

            if(p->name)
            {
                memcpy(p->name,s,len);
                p->name[len]=0;
            }

            cur=p;
        }
        void tag_close(const char* s,int len)
        {
            if(!cur->parent || !cur->name) { err=1; return; }

            if(len && strcmp(cur->name,s)) { err=1; return; }

            cur=cur->parent;
        }

        void data_push(void)
        {
            string s;
            data.swap(s);
            s.trim_right();

            if(s.length())
            {
                if(cur->data) free(cur->data);
                cur->data=s.ptr;
                cur->len=s.size;
                s.ptr=0;
                s.size=0;
            }
        }

    public:
        int line;
    public:
        ctx(node* root):cur(root),st(0),err(0),line(0),tok_size(0),st_close_tag(0),st_quot(0),st_text(0) {}

        void begin(void);

        int parse(const char* buf,int len);

        int end(void);
    };
}

#endif /* __SOAP_H */
