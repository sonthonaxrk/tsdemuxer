#include "ebml.h"
#include <arpa/inet.h>


namespace ebml
{
    int file::open(const char* filename) throw()
    {
        cluster_timecode=0;

        return (fp=fopen64(filename,"rb"))?0:-1;
    }

    void file::close(void) throw()
    {
        if(fp)
        {
            fclose(fp);
            fp=0;
        }
    }

    u_int64_t file::tell(void) throw(std::exception)
    {
        u_int64_t pos=ftello64(fp);
        if(pos==-1)
            throw(exception("tell(): can't get current file position"));
        return pos;
    }

    void file::seek(u_int64_t offset,int whence) throw(std::exception)
    {
        if(fseeko64(fp,offset,whence)==-1)
            throw(exception("fseek(): can't set file position"));
    }

    u_int32_t file::gettag(void) throw(std::exception)
    {
        int ch=fgetc(fp);

        if(ch==EOF)
            return 0;

        int len=0;

        u_int8_t c;

        for(c=(u_int8_t)ch;!(c&0x80) && len<sizeof(u_int32_t);c<<=1,len++);

        if(!(c&0x80))
            throw(exception("gettag(): invalid ebml element tag length"));

        u_int32_t tag=ch;

        for(int i=0;i<len;i++)
        {
            ch=fgetc(fp);

            if(ch==EOF)
                throw(exception("gettag(): broken file"));

            tag=(tag<<8)|(u_int8_t)ch;
        }

        return tag;
    }


    u_int64_t file::getlen(void) throw(std::exception)
    {
        int ch=fgetc(fp);

        if(ch==EOF)
            throw(exception("getlen(): broken file"));

        int len=0;

        u_int8_t c;

        for(c=(u_int8_t)ch;!(c&0x80) && len<sizeof(u_int64_t);c<<=1,len++);

        if(!(c&0x80))
            throw(exception("getlen(): invalid ebml element length size"));

        ch&=~(0x80>>len);

        u_int64_t length=ch;

        for(int i=0;i<len;i++)
        {
            ch=fgetc(fp);

            if(ch==EOF)
                throw(exception("getlen(): broken file"));

            length=(length<<8)|(u_int8_t)ch;
        }

        return length;
    }


    std::string file::getstring(int len) throw(std::exception)
    {
        char s[256];
        if(len>sizeof(s))
            len=sizeof(s);

        if(fread(s,1,len,fp)!=len)
            throw(exception("getstring(): broken file"));

        return std::string(s,len);
    }

    u_int32_t file::getint(int len) throw(std::exception)
    {
        u_int32_t n=0;

        if(len>sizeof(n))
            throw(exception("getint(): integer too large"));


        if(fread(((char*)&n)+(sizeof(n)-len),1,len,fp)!=len)
            throw(exception("getint(): broken file"));

        return ntohl(n);
    }


    int file::parse(int depth) throw(std::exception)
    {
        u_int32_t tag=gettag();

        if(!tag)
            return -1;

        u_int64_t len=getlen();

        u_int64_t next_pos=tell();

        next_pos+=len;

        int rc=0;
        
        switch(tag)
        {
        case 0xa1:              // block
        case 0xa3:
            {
                u_int64_t track=getlen();

                u_int16_t timecode=0;

                if(fread((char*)&timecode,1,sizeof(timecode),fp)!=sizeof(timecode))
                    throw(exception("parse(): can't read block timecode"));

                timecode=ntohs(timecode);

                fprintf(stdout,"track=%llu, timecode=%i\n",track,cluster_timecode+timecode);
            }
            break;
        case 0xa0:              // container
        case 0x1a45dfa3:
        case 0x18538067:
        case 0x1549A966:
        case 0x1654ae6b:
        case 0xae:
        case 0x1f43b675:
            while(tell()<next_pos && !(rc=parse(depth+1)));
            break;
        case 0xe7:              // cluster timecode (uint)
            cluster_timecode=getint(len);
            break;
        case 0x86:              // codec (string)
            {
                std::string s=getstring(len);
                fprintf(stdout,"codec='%s'\n",s.c_str());
            }
            break;
        case 0xd7:              // track number (uint)
            {
                u_int32_t track=getint(len);
                fprintf(stdout,"track=%i\n",track);
            }
            break;
        case 0x22b59c:          // language (string)
            {
                std::string s=getstring(len);
                fprintf(stdout,"lang='%s'\n",s.c_str());
            }
            break;
        }

        seek(next_pos,SEEK_SET);

        return rc;
    }

}

int main(void)
{
    ebml::file mkv;

    if(!mkv.open("../../samples/Watchmen.2009.BluRay.720p.x264.DTS.Rus-WiKi.mkv"))
    {
        try
        {
            while(!mkv.parse());
        }
        catch(const std::exception& e)
        {
            fprintf(stderr,"*** %s\n",e.what());
        }

        mkv.close();
    }
    return 0;
}
