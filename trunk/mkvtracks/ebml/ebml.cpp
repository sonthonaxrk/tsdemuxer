#include "ebml.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdexcept>
#include <map>
#include <string>

namespace ebml
{
    enum
    {
        t_struct        = 1,
        t_string        = 2,
        t_uint          = 3,
        t_block         = 4
    };

    class element_info
    {
    public:
        u_int8_t type;
        std::string name;

        element_info(void):type(0) {}
        element_info(u_int8_t _type,const std::string& _name):type(_type),name(_name) {}
    };

    std::map<u_int32_t,element_info> semantic;

    void init(void)
    {
        semantic[0x1a45dfa3]    = element_info(t_struct,"EBML");
        semantic[0x4282]        = element_info(t_string,"DocType");
        semantic[0x4282]        = element_info(t_string,"DocType");
        semantic[0x4287]        = element_info(t_string,"DocTypeVersion");
        semantic[0x4285]        = element_info(t_string,"DocTypeReadVersion");

        semantic[0x18538067]    = element_info(t_struct,"Segment");
        semantic[0x1549A966]    = element_info(t_struct,"Info");
        semantic[0x2AD7B1]      = element_info(t_uint,"TimecodeScale");
        semantic[0x1654ae6b]    = element_info(t_struct,"Tracks");
        semantic[0xae]          = element_info(t_struct,"TrackEntry");
        semantic[0x86]          = element_info(t_uint  ,"Codec");
        semantic[0xd7]          = element_info(t_string,"TrackNumber");
        semantic[0x22B59C]      = element_info(t_string,"Language");

        semantic[0x1f43b675]    = element_info(t_struct,"Cluster");
        semantic[0xa0]          = element_info(t_struct,"BlockGroup");
        semantic[0xa1]          = element_info(t_block ,"Block");
    }


}

int ebml::stream_base::getTag(u_int32_t& tag)
{
    tag=0;

    u_int8_t buf[4];

    if(read((char*)buf,1)!=1)
        return 0;

    int l=1;

    u_int8_t p=buf[0];

    while(!(p&0x80) && l<=sizeof(buf))
        { l++; p<<=1; }

    if(!(p&0x80))
        return -1;

    if(read((char*)buf+1,l-1)!=l-1)
        return -1;

    for(int i=0;i<l;i++)
        tag=(tag<<8)+buf[i];

    return l;
}

int ebml::stream_base::getLen(u_int64_t& len)
{
    len=0;

    u_int8_t buf[8];

    if(read((char*)buf,1)!=1)
        return -1;

    int l=1;

    u_int8_t p=buf[0];

    while(!(p&0x80))
        { l++; p<<=1; }

    if(!(p&0x80))
        return -1;

    buf[0]&=(~(0x80>>l-1));

    if(read((char*)buf+1,l-1)!=l-1)
        return -1;

    for(int i=0;i<l;i++)
        len=(len<<8)+buf[i];

    return l;
}


bool ebml::stream::open(const char* name)
{
    close();

    fd=::open(name,O_LARGEFILE|O_BINARY|O_RDONLY,0644);

    if(fd!=-1)
    {
        len=offset=0;
        return true;
    }

    return false;
}


void ebml::stream::close(void)
{
    if(fd!=-1)
    {
        ::close(fd);
        fd=-1;
    }
}

int ebml::stream::read(char* p,int l)
{
    if(fd==-1)
        return 0;

    const char* tmp=p;

    while(l>0)
    {
        int n=len-offset;

        if(n>0)
        {
            if(n>l)
                n=l;

            memcpy(p,buf+offset,n);
            p+=n;
            offset+=n;
            l-=n;
        }else
        {
            int m=::read(fd,buf,max_buf_len);
            if(m==-1 || !m)
                break;
            len=m;
            offset=0;
        }
    }

    return p-tmp;
}

int ebml::stream::skip(u_int64_t l)
{
    int n=len-offset;

    if(n<l)
    {
        l-=n;
        len=offset=0;
        if(lseek(fd,l,SEEK_CUR)==(off_t)-1)
            return -1;
    }else
        offset+=l;

    return 0;
}


int parse(ebml::stream_base* s,int depth)
{
    for(;;)
    {
        u_int32_t tag;
        u_int64_t len;

        int rc=s->getTag(tag);

        if(!rc)
            break;

        if(rc<0)
            { fprintf(stderr,"invalid tag\n"); return -1; }

        if(s->getLen(len)<1)
            { fprintf(stderr,"invalid length\n"); return -1; }

        ebml::element_info& info=ebml::semantic[tag];

        std::string value;
        int sub=0;

        switch(info.type)
        {
        case ebml::t_block:
            {
                u_int8_t buf[3];
                if(s->read((char*)buf,sizeof(buf))!=sizeof(buf))
                    { fprintf(stderr,"invalid block\n"); return -1; }
                else
                {
                    char tmp[256];
                    int n=sprintf(tmp,"track=%i, timecode=%i",buf[0]&0x7f,(buf[1]<<8)+buf[2]);
                    value.assign(tmp,n);
                }

                if(s->skip(len-3))
                    { fprintf(stderr,"invalid segment\n"); return -1; }
            }
            break;
        case ebml::t_struct:
            value="sub-items";
            sub=1;
            break;
        default:
            value="unknown";
            if(s->skip(len))
                { fprintf(stderr,"invalid segment\n"); return -1; }
            break;
        }

        if(info.name.length())
        {
            for(int i=0;i<depth;i++)
                printf(" ");

            printf("%s: %s\n",info.name.c_str(),value.c_str());
        }

        if(sub)
        {
            ebml::segment ss(s,len);
            if(parse(&ss,depth+1))
                return -1;
        }
    }

    return 0;
}

int main(void)
{
    ebml::init();

    ebml::stream stream("../output.mkv");

    parse(&stream,0);

    return 0;
}
