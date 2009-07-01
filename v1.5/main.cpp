/*

 Copyright (C) 2009 Anton Burdinuk

 clark15b@gmail.com

*/


#include "ts.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <list>
#include <getopt.h>
#include <stdlib.h>

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
}

int main(int argc,char** argv)
{
    fprintf(stderr,"tsdemux 1.50 AVCHD/Blu-Ray HDMV Transport Stream demultiplexer\n\nCopyright (C) 2009 Anton Burdinuk\n\nclark15b@gmail.com\nhttp://code.google.com/p/tsdemuxer\n\n");

    if(argc<2)
    {
        fprintf(stderr,"USAGE: ./tsdemux [-u] [-p] [-o dst] [-d mode] [-j] [-z] [-c channel] *.ts|*.m2ts ...\n");
        fprintf(stderr,"-u demux unknown streams\n");
        fprintf(stderr,"-p parse only\n");
        fprintf(stderr,"-o redirect output to another directory or file\n");
        fprintf(stderr,"-d dump TS structure to STDOUT (mode=1: dump M2TS timecodes, mode=2: dump PTS/DTS, mode=3: human readable output)\n");
        fprintf(stderr,"-j join elementary streams\n");
        fprintf(stderr,"-z demux to PES streams (instead of elementary streams)\n");
        fprintf(stderr,"-c channel number for demux\n");
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
    std::string output;

    int opt;
    while((opt=getopt(argc,argv,"pd:ujc:zo:"))>=0)
        switch(opt)
        {
        case 'p':
            parse_only=true;
            break;
        case 'd':
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
            output=optarg;
            break;
        }

    std::list<std::string> playlist;             // playlist

    while(optind<argc)
    {
        playlist.push_back(argv[optind]);
        optind++;
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
