#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <map>

namespace mkv
{
    class track
    {
    public:
        std::string codec;
        std::string lang;
        std::string delay;
    };

    void getInfo(const std::string& filename,std::map<int,track>& tracks);
}

void mkv::getInfo(const std::string& filename,std::map<int,mkv::track>& tracks)
{
    FILE* fp=popen(filename.c_str(),"r");
    if(fp)
    {
        char tmp[512];

        int track_number=0;
        std::string track_codec;
        std::string track_lang;

        int blocks=256;

        while(fgets(tmp,sizeof(tmp),fp) && blocks>0)
        {
            int depth=0;

            char* ptr=tmp;

            for(;*ptr && *ptr!='+';ptr++)
                depth++;

            if(*ptr=='+')
                ptr++;

            while(*ptr && *ptr==' ')
                ptr++;


            if(depth==3)
            {
                if(!strncmp(ptr,"Block (",7))
                {
                    ptr+=7;
                    char* p=strpbrk(ptr,"\r\n");
                    if(p)
                    {
                        while(p>ptr && *p!=')')
                            p--;
                        *p=0;
                        blocks--;

                        int tn=0;
                        std::string ttc;

                        for(char* p1=ptr,*p2=0;p1;p1=p2)
                        {
                            while(*p1 && *p1==' ')
                                p1++;
                            p2=strchr(p1,',');
                            if(p2)
                            {
                                *p2=0;
                                p2++;
                            }

                            if(!strncmp(p1,"track number ",13))
                                tn=atoi(p1+13);
                            else if(!strncmp(p1,"timecode ",9))
                            {
                                p1+=9;
                                char* pp=strchr(p1,' ');
                                if(pp)
                                    ttc.assign(p1,pp-p1);
                            }
                        }

                        if(tn>0 && ttc.length())
                        {
                            track& t=tracks[tn];
                            if(!t.delay.length())
                                t.delay=ttc;
                        }
                    }
                }
                else if(!strncmp(ptr,"Track number: ",14))
                    track_number=atoi(ptr+14);
                else if(!strncmp(ptr,"Codec ID: ",10))
                {
                    ptr+=10;
                    char* p=strpbrk(ptr,"\r\n");
                    if(p)
                        track_codec.assign(ptr,p-ptr);
                }else if(!strncmp(ptr,"Language: ",10))
                {
                    ptr+=10;
                    char* p=strpbrk(ptr,"\r\n");
                    if(p)
                        track_lang.assign(ptr,p-ptr);
                }
            }

            if(track_number>0 && track_codec.length() && track_lang.length())
            {
                track& t=tracks[track_number];
                t.codec=track_codec;
                t.lang=track_lang;
                track_number=0;
                track_codec.clear();
                track_lang.clear();
            }

        }

        pclose(fp);
    }
}


int main(int argc,char** argv)
{
    fprintf(stderr,"mkvtracks 1.00 Simple mkvinfo wrapper\n\nCopyright (C) 2009 Anton Burdinuk\n\nclark15b@gmail.com\nhttp://code.google.com/p/tsdemuxer\n\n");

    if(argc<2)
    {
        fprintf(stderr,"USAGE: ./mkvtracks filename.mkv\n");
        return 0;
    }

    std::map<int,mkv::track> tracks;

    getInfo(std::string("mkvinfo -v ")+argv[1],tracks);

    for(std::map<int,mkv::track>::const_iterator i=tracks.begin();i!=tracks.end();++i)
    {
        const mkv::track& t=i->second;

        printf("track %i, codec=%s, lang=%s, delay=%s\n",i->first,t.codec.c_str(),t.lang.c_str(),t.delay.c_str());
    }

    return 0;
}
