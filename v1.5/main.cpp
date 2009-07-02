/*

 Copyright (C) 2009 Anton Burdinuk

 clark15b@gmail.com

*/


#include "ts.h"
#include "mpls.h"

namespace ts
{
    class ts_file_info
    {
    public:
        std::string filename;
        u_int64_t first_dts;
        u_int64_t first_pts;
        u_int64_t last_pts;

        ts_file_info(void):first_dts(0),first_pts(0),last_pts(0) {}
    };

#ifdef _WIN32
    inline int strcasecmp(const char* s1,const char* s2) { return lstrcmpi(s1,s2); }
#endif

    int scan_dir(const char* path,std::list<std::string>& l);
    int get_clip_number_by_filename(const std::string& s);

    bool is_ts_filename(const std::string& s)
    {
        if(!s.length())
            return false;

        if(s[s.length()-1]=='/' || s[s.length()-1]=='\\')
            return false;

        std::string::size_type n=s.find_last_of('.');

        if(n==std::string::npos)
            return false;

        std::string ext=s.substr(n+1);

        if(!strcasecmp(ext.c_str(),"ts") || !strcasecmp(ext.c_str(),"m2ts"))
            return true;

        return false;
    }

    std::string trim_slash(const std::string& s)
    {
        const char* p=s.c_str()+s.length();

        while(p>s.c_str() && (p[-1]=='/' || p[-1]=='\\'))
            p--;

        return s.substr(0,p-s.c_str());
    }
}

#ifdef _WIN32
int ts::scan_dir(const char* path,std::list<std::string>& l)
{
    _finddata_t fileinfo;

    intptr_t dir=_findfirst((std::string(path)+"\\*.*").c_str(),&fileinfo);

    if(dir==-1)
        perror(path);
    else
    {
        while(!_findnext(dir,&fileinfo))
            if(!(fileinfo.attrib&_A_SUBDIR) && *fileinfo.name!='.')
            {
                char p[512];

                int n=sprintf(p,"%s\\%s",path,fileinfo.name);

                l.push_back(std::string(p,n));
            }
    }

    _findclose(dir);

    return l.size();
}
#else
int ts::scan_dir(const char* path,std::list<std::string>& l)
{
    DIR* dir=opendir(path);

    if(!dir)
        perror(path);
    else
    {
        dirent* d;

        while((d=readdir(dir)))
        {
            if(*d->d_name!='.')
            {
                char p[512];

                int n=sprintf(p,"%s/%s",path,d->d_name);

                l.push_back(std::string(p,n));
            }
        }

        closedir(dir);
    }

    return l.size();
}
#endif

int ts::get_clip_number_by_filename(const std::string& s)
{
    int ll=s.length();
    const char* p=s.c_str();

    while(ll>0)
    {
        if(p[ll-1]=='/' || p[ll-1]=='\\')
            break;
        ll--;
    }

    p+=ll;
    int cn=0;

    const char* pp=strchr(p,'.');
    if(pp)
    {
        for(int i=0;i<pp-p;i++)
            cn=cn*10+(p[i]-48);
    }

    return cn;
}


int main(int argc,char** argv)
{
    fprintf(stderr,"tsdemux 1.50 AVCHD/Blu-Ray HDMV Transport Stream demultiplexer\n\nCopyright (C) 2009 Anton Burdinuk\n\nclark15b@gmail.com\nhttp://code.google.com/p/tsdemuxer\n\n");

    if(argc<2)
    {
        fprintf(stderr,"USAGE: ./tsdemux [-d src] [-l mpls] [-o dst] [-c channel] [-u] [-j] [-z] [-p] [-e mode] [-v] *.ts|*.m2ts ...\n");
        fprintf(stderr,"-d demux all mts/m2ts/ts files from directory\n");
        fprintf(stderr,"-l use AVCHD/Blu-Ray playlist file (*.mpl,*.mpls)\n");
        fprintf(stderr,"-o redirect output to another directory or transport stream file\n");
        fprintf(stderr,"-c channel number for demux\n");
        fprintf(stderr,"-u demux unknown streams\n");
        fprintf(stderr,"-j join elementary streams\n");
        fprintf(stderr,"-z demux to PES streams (instead of elementary streams)\n");
        fprintf(stderr,"-p parse only\n");
        fprintf(stderr,"-e dump TS structure to STDOUT (mode=1: dump M2TS timecodes, mode=2: dump PTS/DTS, mode=3: human readable PTS/DTS dump)\n");
        fprintf(stderr,"-v turn on verbose output\n");
        fprintf(stderr,"\ninput files can be *.m2ts, *.mts or *.ts\n");
        fprintf(stderr,"output elementary streams to *.sup, *.m2v, *.264, *.vc1, *.ac3, *.m2a and *.pcm files\n");
        fprintf(stderr,"\n");
        return 0;
    }


    bool parse_only=false;
    int dump=0;
    bool av_only=true;
    bool join=false;
    int channel=0;
    int pes=0;
    int verb=0;
    std::string output;

    std::string mpls_file;                      // MPLS file

    std::list<std::string> playlist;            // playlist
    std::map<int,std::string> mpls_datetime;    // AVCHD clip date/time

    int opt;
    while((opt=getopt(argc,argv,"pe:ujc:zo:d:l:v"))>=0)
        switch(opt)
        {
        case 'p':
            parse_only=true;
            break;
        case 'e':
            dump=atoi(optarg);
            break;
        case 'u':
            av_only=false;
            break;
        case 'j':
            join=true;
            break;
        case 'c':
            channel=atoi(optarg);
            break;
        case 'z':
            pes=1;
            break;
        case 'o':
            output=ts::trim_slash(optarg);
            break;
        case 'd':
            {
                std::list<std::string> l;
                ts::scan_dir(ts::trim_slash(optarg).c_str(),l);
                l.sort();
                playlist.merge(l);
            }
            break;
        case 'l':
            mpls_file=optarg;
            break;
        case 'v':
            verb=1;
            break;
        }

    while(optind<argc)
    {
        playlist.push_back(argv[optind]);
        optind++;
    }

    if(mpls_file.length())
    {
        std::list<int> mpls;                        // list of clip id from mpls files
        std::list<std::string> new_playlist;

        if(mpls::parse(mpls_file.c_str(),mpls,mpls_datetime,playlist.size()?verb:1))
            fprintf(stderr,"%s: invalid playlist file format\n",mpls_file.c_str());

        if(mpls.size())
        {
            std::map<int,std::string> clips;
            for(std::list<std::string>::iterator i=playlist.begin();i!=playlist.end();++i)
            {
                std::string& s=*i;
                clips[ts::get_clip_number_by_filename(s)]=s;
            }

            for(std::list<int>::iterator i=mpls.begin();i!=mpls.end();++i)
            {
                std::string& s=clips[*i];

                if(s.length())
                    new_playlist.push_back(s);
            }

            playlist.swap(new_playlist);
        }
    }

    time_t beg_time=time(0);

    if(!ts::is_ts_filename(output))
    {
        if(join)
        {
            ts::demuxer demuxer;

            for(std::list<std::string>::iterator i=playlist.begin();i!=playlist.end();++i)
            {
                const std::string& s=*i;

                demuxer.av_only=av_only;
                demuxer.channel=channel;
                demuxer.pes_output=pes;
                demuxer.dst=output;
                demuxer.verb=verb;

                demuxer.demux_file(s.c_str());

                demuxer.gen_timecodes();

                demuxer.reset();
            }
            demuxer.show();
        }else
        {
            for(std::list<std::string>::iterator i=playlist.begin();i!=playlist.end();++i)
            {
                const std::string& s=*i;

                ts::demuxer demuxer;

                demuxer.parse_only=dump>0?true:parse_only;
                demuxer.dump=dump;
                demuxer.av_only=av_only;
                demuxer.channel=channel;
                demuxer.pes_output=pes;
                demuxer.dst=output;
                demuxer.verb=verb;

                demuxer.demux_file(s.c_str());

                demuxer.show();
            }
        }
    }else
    {
        // join to TS/M2TS file

        std::list<ts::ts_file_info> info;

        if(!channel)
        {
            fprintf(stderr,"the channel is not chosen, set to 1\n");
            channel=1;
        }


        fprintf(stderr,"\nstep 1 - analyze a stream\n\n");

        for(std::list<std::string>::iterator i=playlist.begin();i!=playlist.end();++i)
        {
            const std::string& name=*i;

            ts::demuxer demuxer;

            demuxer.parse_only=true;
            demuxer.av_only=av_only;
            demuxer.channel=channel;

            demuxer.demux_file(name.c_str());
            demuxer.show();

            info.push_back(ts::ts_file_info());
            ts::ts_file_info& nfo=info.back();
            nfo.filename=name;

            for(std::map<u_int16_t,ts::stream>::const_iterator i=demuxer.streams.begin();i!=demuxer.streams.end();++i)
            {
                const ts::stream& s=i->second;

                if(s.type!=0xff)
                {
                    if(s.first_dts)
                    {
                        if(s.first_dts<nfo.first_dts || !nfo.first_dts)
                            nfo.first_dts=s.first_dts;
                    }

                    if(s.first_pts<nfo.first_pts || !nfo.first_pts)
                        nfo.first_pts=s.first_pts;

                    u_int64_t n=s.last_pts+s.frame_length;

                    if(n>nfo.last_pts || !nfo.last_pts)
                        nfo.last_pts=n;
                }
            }
        }

        fprintf(stderr,"\nstep 2 - remux\n\n");

        u_int64_t cur_pts=0;

        int fn=0;

        for(std::list<ts::ts_file_info>::iterator i=info.begin();i!=info.end();++i,fn++)
        {
            ts::ts_file_info& nfo=*i;

            u_int64_t first_pts=nfo.first_dts>0?(nfo.first_pts-nfo.first_dts):0;

            if(!fn)
                cur_pts=first_pts;

            fprintf(stderr,"%s: %llu (len=%llu)\n",nfo.filename.c_str(),cur_pts,nfo.last_pts-first_pts);

            // new_pes_pts=cur_pts+(pts_from_pes-first_pts)


            cur_pts+=nfo.last_pts-first_pts;
        }
    }

    fprintf(stderr,"\ntime: %li sec\n",time(0)-beg_time);

    return 0;
}
