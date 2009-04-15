/*

Copyright (C) 2009 Anton Burdinuk

clark15b@gmail.com

*/

#include <sys/types.h>
#include <stdio.h>
#include <map>
#include <memory.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

// TODO: read all files from directory
// TODO: write timecodes file (format v2) for mkvtoolnix
// TODO: write chapter file for mkvtoolnix
// TODO: write mkvtoolnix options file (?)
// TODO: Windows build
// TODO: fps*2 for interlaced ES (?)
// TODO: read AVCHD playlist and date/time (for chapters)

/*
# timecode format v2
0
33
67

# timecode format v2
0
32
64
96
128
160

CHAPTER01=00:00:00.000
CHAPTER01NAME=Intro
CHAPTER02=00:02:30.000
CHAPTER02NAME=Baby prepares to rock
CHAPTER03=00:02:42.300
CHAPTER03NAME=Baby rocks the house
*/

namespace tsmux
{
    struct stream
    {
	u_int16_t programm;			// programm number (1,2 ...)
	u_int8_t  id;				// stream number in programm
	u_int8_t  type;				// 0xff			- not ES
						// 0x01,0x02		- MPEG2 video
						// 0x80			- MPEG2 video (for TS only, not M2TS)
						// 0x1b			- H.264 video
						// 0xea			- VC-1  video
						// 0x81,0x06		- AC3   audio
						// 0x03,0x04		- MPEG2 audio
						// 0x80			- LPCM  audio

	u_int8_t stream_id;			// MPEG stream id
	u_int64_t dts;				// MPEG stream DTS (presentation time for audio, decode time for video)
	u_int64_t first_pts;
	u_int64_t last_pts;
	u_int32_t frame_rate;			// time for show frame in ticks (90 ticks = 1 ms, 90000/frame_rate=fps)

	FILE* fp;				// ES output file
	
	stream(void):programm(0xffff),id(0),type(0xff),stream_id(0),dts(0),first_pts(0),last_pts(0),frame_rate(0),fp(0) {}
    };

    struct channel_data
    {
	u_int64_t beg;
        u_int64_t end;
        
        channel_data(void):beg(0),end(0) {}
    };

    inline u_int16_t ntohs(u_int16_t v) { return ((v<<8)&0xff00)|((v>>8)&0x00ff); }

    u_int64_t decode_pts(const unsigned char* p);


    const char* output_dir	= ".";
    bool hdmv_mode		= false;
    bool parse_only		= false;
    bool interlace_mode		= false;
    int channel			= -1;

    int demux_packet(const unsigned char* ptr,int len,std::map<u_int16_t,stream>& pids);


    namespace stream_type
    {
	enum
	{
	    unknown		= 0,
	    audio		= 1,
	    video		= 2
	};
	
	enum
	{
	    data		= 0,
	    mpeg2_video		= 1,
	    h264_video		= 2,
	    vc1_video		= 3,
	    ac3_audio		= 4,
	    mpeg2_audio		= 5,
	    lpcm_audio		= 6
	};
    }

    
    const char* get_stream_type_name(u_int8_t type_id,int* type);
    int get_stream_type(u_int8_t type_id);
    const char* get_file_ext_by_stream_type(u_int8_t type_id);

    int demux_file(const char* name,FILE* fp,std::map<u_int16_t,tsmux::stream>& pids,int chapter);
}

const char* tsmux::get_stream_type_name(u_int8_t type_id,int* type)
{
    int t=get_stream_type(type_id);

    static const char* list[7]=
	{ "Data","MPEG2 video","H.264 video","VC-1 video","AC3 audio","MPEG2 audio","LPCM audio" };

    static const int tlist[7]=
	{ stream_type::unknown, stream_type::video, stream_type::video, stream_type::video,
	    stream_type::audio,stream_type::audio,stream_type::audio };
    
    if(type)
	*type=tlist[t];
    
    return list[t];
}

int tsmux::get_stream_type(u_int8_t type_id)
{
    switch(type_id)
    {
    case 0x01:
    case 0x02:
	return stream_type::mpeg2_video;
    case 0x80:
	return hdmv_mode?stream_type::lpcm_audio:stream_type::mpeg2_video;
    case 0x1b:
	return stream_type::h264_video;
    case 0xea:
	return stream_type::vc1_video;
    case 0x81:
    case 0x06:
	return stream_type::ac3_audio;
    case 0x03:
    case 0x04:
	return stream_type::mpeg2_audio;
    }
    
    return stream_type::data;
}

const char* tsmux::get_file_ext_by_stream_type(u_int8_t type_id)
{
    static const char* list[7]= { "bin", "m2v", "264", "vc1", "ac3", "m2a", "pcm" };
    
    return list[type_id];
}


u_int64_t tsmux::decode_pts(const unsigned char* p)
{
    u_int64_t pts=((p[0]&0xe)<<29);
    pts|=((p[1]&0xff)<<22);
    pts|=((p[2]&0xfe)<<14);
    pts|=((p[3]&0xff)<<7);
    pts|=((p[4]&0xfe)>>1);
    
    return pts;
}

int tsmux::demux_packet(const unsigned char* ptr,int len,std::map<u_int16_t,tsmux::stream>& pids)
{
    if(len==192)				// skip timecode from M2TS stream
    {
	ptr+=4;
	len-=4;
    }else if(len!=188)				// invalid packet length
	return -1;
    
    if(ptr[0]!=0x47)				// invalid packet sync byte
	return -1;

    u_int16_t pid=ntohs(*((u_int16_t*)(ptr+1)));
    
    if(pid&0x8000)				// transport error
	return -1;

    bool payload_unit_start_indicator=pid&0x4000;

    u_int8_t flags=ptr[3];
    
    if(flags&0xc0)				// scrambled
	return -1;

    bool adaptation_field_exist=flags&0x20;

    bool payload_data_exist=flags&0x10;

    pid&=0x1fff;
    
    if(pid!=0x1fff && payload_data_exist)
    {
	ptr+=4;
	len-=4;
    
	if(adaptation_field_exist)
	{
	    int l=(*ptr)+1;
	    
	    if(l>len)
		return -1;
	
	    ptr+=l;				// skip adaptation field
	    len-=l;
	}
	
	
	if(!pid)
	{
	    // PAT table

	    if(payload_unit_start_indicator)
	    {
		if(len<1)
		    return -1;

		ptr+=1;				// skip pointer field
		len-=1;
	    }

	    
	    if(*ptr!=0x00)			// is not PAT
		return 0;

	    if(len<8)
		return -1;
	
	
	    u_int16_t l=ntohs(*((u_int16_t*)(ptr+1)));
	    
	    if(l&0xb000!=0xb000)
		return -1;
	
	    l&=0x0fff;
	    
	    len-=3;
	    
	    if(l>len)
		return -1;
	
	    len-=5;
	    ptr+=8;
	    l-=5+4;
	    
	    if(l%4)
		return -1;
	    
	    int n=l/4;

	    for(int i=0;i<n;i++)
	    {
		u_int16_t programm=ntohs(*((u_int16_t*)ptr));

		u_int16_t pid=ntohs(*((u_int16_t*)(ptr+2)));
		
		if(pid&0xe000!=0xe000)
		    return -1;
		
		pid&=0x1fff;
	    
		ptr+=4;

		stream& s=pids[pid];
		
		s.programm=programm;
		s.type=0xff;
	    }	    
	    
	}else
	{
	    stream& s=pids[pid];
	    
	    if(s.programm!=0xffff)
	    {
		if(s.type==0xff)
		{
		    // PMT table

		    if(payload_unit_start_indicator)
		    {
			if(len<1)
			    return -1;

			ptr+=1;			// skip pointer field
			len-=1;
		    }

		    if(*ptr!=0x02)		// is not PMT
			return 0;

		    if(len<12)
			return -1;
		    
		    u_int16_t l=ntohs(*((u_int16_t*)(ptr+1)));
		    
		    if(l&0x3000!=0x3000)
			return -1;

		    l=(l&0x0fff)+3;
		    
		    u_int16_t n=(ntohs(*((u_int16_t*)(ptr+10)))&0x0fff)+12;

		    if(len<l || n>l)
			return -1;
		
		    ptr+=n;
		    len-=n;
		    
		    l-=n+4;
		    
		    while(l)
		    {
			if(l<5)
			    return -1;
		    
			u_int8_t type=*ptr;
			u_int16_t pid=ntohs(*((u_int16_t*)(ptr+1)));
			
			if(pid&0xe000!=0xe000)
			    return -1;
			
			pid&=0x1fff;
			
			u_int16_t ll=(ntohs(*((u_int16_t*)(ptr+3)))&0x0fff)+5;
			
			if(ll>l)
			    return -1;
			
			ptr+=ll;
			l-=ll;			
			
			stream& ss=pids[pid];

			if(ss.programm!=s.programm || ss.type!=type)
			{
			    ss.programm=s.programm;
			    ss.type=type;
			    ss.id=++s.id;
			}
		    }
		    
		    if(l>0)
			return -1;
		    

		}else
		{
		    // PES (Packetized Elementary Stream)

		    if(payload_unit_start_indicator)
		    {
			// PES header
			
			if(len<6)
			    return -1;

			static const unsigned char start_code_prefix[]={0x00,0x00,0x01};
		    
			if(memcmp(ptr,start_code_prefix,sizeof(start_code_prefix)))
			    return -1;
			
			u_int8_t stream_id=ptr[3];
			u_int16_t l=ntohs(*((u_int16_t*)(ptr+4)));
			
			ptr+=6;
			len-=6;
			
			if((stream_id>=0xbd && stream_id<=0xbf) || (stream_id>=0xc0 && stream_id<=0xdf) || (stream_id>=0xe0 && stream_id<=0xef)  || (stream_id>=0xfa && stream_id<=0xfe))
			{
			    // PES header extension
			
			    if(len<3)
				return -1;
			
			    u_int8_t bitmap=ptr[1];
			    u_int8_t hlen=ptr[2]+3;
			
			    if(len<hlen)
				return -1;
			
			    if(l>0)
				l-=hlen;

			    switch(bitmap&0xc0)
			    {
			    case 0x80:		// PTS only
				if(hlen>=8)
				{
				    u_int64_t pts=decode_pts(ptr+3);
				    
				    if(s.dts>0 && pts>s.dts)
					s.frame_rate=pts-s.dts;
				    
				    s.dts=pts;
				    
				    if(pts>s.last_pts)
					s.last_pts=pts;

				    if(!s.first_pts)
					s.first_pts=pts;
				    
				    /* fprintf(stderr,"%.2x: PTS=%i\n",stream_id,(unsigned int)s.dts); */
				}
				break;
			    case 0xc0:		// PTS,DTS
				if(hlen>=13)
				{
				    u_int64_t pts=decode_pts(ptr+3);
				    u_int64_t dts=decode_pts(ptr+8);
				    
				    if(s.dts>0 && dts>s.dts)
					s.frame_rate=dts-s.dts;

				    s.dts=dts;

				    if(pts>s.last_pts)
					s.last_pts=pts;

				    /* fprintf(stderr,"%.2x: PTS=%i\n",stream_id,(unsigned int)s.dts); */
				}
				break;
			    }
			    
			    ptr+=hlen;
			    len-=hlen;			    
			
			    s.stream_id=stream_id;
			}else
			    s.stream_id=0;
			    
		    }
		    
		    if(!parse_only && s.stream_id)
		    {
			if(channel==-1 || s.programm==channel)
			{
			    if(!s.fp)
			    {
				char tmp[512];
			    
				sprintf(tmp,"%s/channel_%.2x_stream_%.2x.%s",output_dir,s.programm,s.stream_id,get_file_ext_by_stream_type(get_stream_type(s.type)));
				
				s.fp=fopen(tmp,"wb");
			    
				if(!s.fp)
				    perror(tmp);
			    }
			
			    if(s.fp)
			    {
			    	/* fprintf(s.fp,"pid=%i, programm=%i, id=%i, type=0x%x, len=%i, stream=0x%x, dts=%i (%i)\n",pid,s.programm,s.id,s.type,len,s.stream_id,(unsigned int)s.dts,s.frame_rate); */

				if(!fwrite(ptr,len,1,s.fp))
				    fprintf(stderr,"elementary stream write error\n");
			    }
			}
		    }		    
		}
	    }
	}
    }

    return 0;
}


namespace muxer
{

    int demux_file(const char* name,FILE* fp);
}

int main(int argc,char** argv)
{
    fprintf(stderr,"tsdemux AVCHD/Blu-Ray HDMV Transport Stream demultiplexer\n\nCopyright (C) 2009 Anton Burdinuk\n\nclark15b@gmail.com\nhttp://code.google.com/p/tsdemuxer\n\n");

    if(argc<2)
    {
	fprintf(stderr,"USAGE: ./tsdemux [-o directory] [-d directory] [-c channel] [-h] [-i] [-p] file1.(ts|m2ts) ... fileN.(ts|m2ts)\n");
	fprintf(stderr,"-o redirect output to another directory (default: '.')\n");
	fprintf(stderr,"-d demux all files from directory\n");
	fprintf(stderr,"-c demux one channel streams only (default: all streams from all channels)\n");
	fprintf(stderr,"-h force HDMV mode for STDIN input (192 byte packets)\n");
	fprintf(stderr,"-i force interlace mode for video stream (fps*2)\n");
	fprintf(stderr,"-p parse only, do not demux to video and audio files\n");
	fprintf(stderr,"\ninput files can be *.m2ts, *.mts, *.ts or '-' for STDIN input\n");
	fprintf(stderr,"output to *.bin, *.m2v, *.264, *.vc1, *.ac3, *.m2a, *.pcm and *.tmc files\n");
	fprintf(stderr,"\n");
	return 0;
    }


    using namespace tsmux;
    
    const char* input_dir="";

    int opt;

    while((opt=getopt(argc,argv,"o:hipc:d:"))>=0)
	switch(opt)
	{
	case 'o':
	    output_dir=optarg;
	    break;
	case 'h':
	    hdmv_mode=true;
	    break;
	case 'i':
	    interlace_mode=true;
	    break;
	case 'p':
	    parse_only=true;
	    break;
	case 'c':
	    channel=atoi(optarg);
	    break;
	case 'd':
	    input_dir=optarg;
	    break;
	}


    int stdin_number=0;

    std::map<u_int16_t,tsmux::stream> pids;
    
    int chapter=0;
    
    while(optind<argc)
    {
	if(!strcmp(argv[optind],"-"))
	{
	    if(!stdin_number)
	    {
		demux_file("stdin",stdin,pids,++chapter);
		
		stdin_number++;
	    }
	}else
	{
	    bool hdmv_mode_tmp=hdmv_mode;
	
	    const char* p=strrchr(argv[optind],'.');

	    if(p)
	    {
		p++;
		
		if(!strcasecmp(p,"mts") || !strcasecmp(p,"m2ts"))
		    hdmv_mode=true;
		else if(!strcasecmp(p,"ts"))
		    hdmv_mode=false;
	    }
	
	    FILE* fp=fopen(argv[optind],"rb");
	    
	    if(!fp)
		perror(argv[optind]);
	    else
	    {	    
		demux_file(argv[optind],fp,pids,++chapter);

		fclose(fp);
	    }
	    
	    hdmv_mode=hdmv_mode_tmp;
	}
	
	optind++;
    }


    fprintf(stderr,"\n---------------------------------------------------------------\n");

    for(std::map<u_int16_t,tsmux::stream>::iterator i=pids.begin();i!=pids.end();++i)
    {
	tsmux::stream& s=i->second;

	if(s.stream_id)
	{
	    int stream_type=0;

	    const char* type_name=tsmux::get_stream_type_name(s.type,&stream_type);

	    switch(stream_type)
	    {
	    case tsmux::stream_type::video:
		fprintf(stderr,"channel %i, stream 0x%x, type 0x%x (%s, fps=%.2f)\n",s.programm,s.stream_id,s.type,type_name,90000./s.frame_rate);
		break;
	    case tsmux::stream_type::audio:
		fprintf(stderr,"channel %i, stream 0x%x, type 0x%x (%s)\n",s.programm,s.stream_id,s.type,type_name);
		break;
	    }
	    
	    if(s.fp)
		fclose(s.fp);
	}
    }


    
    return 0;
}

int tsmux::demux_file(const char* name,FILE* fp,std::map<u_int16_t,tsmux::stream>& pids,int chapter)
{
    size_t l=hdmv_mode?192:188;

    char tmp[192];
    
    for(;;)
    {
	size_t length=fread(tmp,1,l,fp);

	if(!length)
	    break;
	
	if(length!=l)
	{
	    fprintf(stderr,"%s: incompleted TS packet\n",name);
	    break;
	}

	if(tsmux::demux_packet((unsigned char*)tmp,l,pids))
	{
	    fprintf(stderr,"%s: invalid packet\n",name);
	    break;
	}
    }


    // find low first and last pts
    
    std::map<u_int16_t,channel_data> chan;
    
    for(std::map<u_int16_t,tsmux::stream>::iterator i=pids.begin();i!=pids.end();++i)
    {
	tsmux::stream& s=i->second;

	channel_data& ch=chan[s.programm];

	if(s.stream_id && s.first_pts && s.last_pts)
	{
	    if(!ch.beg || ch.beg>s.first_pts)
		ch.beg=s.first_pts;
	
	    u_int64_t end=s.last_pts+s.frame_rate;
	
	    if(!ch.end || ch.end>end)
		ch.end=end;
	}
    }

    // show info    

    for(std::map<u_int16_t,tsmux::stream>::iterator i=pids.begin();i!=pids.end();++i)
    {
	tsmux::stream& s=i->second;

	channel_data& ch=chan[s.programm];

	if(s.stream_id && s.first_pts && s.last_pts && s.frame_rate)
	{
	    double delay=s.first_pts-ch.beg;
	    delay/=90.;

	    u_int64_t end=s.last_pts+s.frame_rate;

	    double len=end-s.first_pts-ch.beg;
	    len/=90.;   

	    fprintf(stderr,"%s: channel %i, stream 0x%.2x, length %.2fms",name,s.programm,s.stream_id,len);
	    
	    
	    if(end>ch.end)
		fprintf(stderr," (+%.2fms)",(double)(end-ch.end)/90.);
	
	    if(delay)
		fprintf(stderr,", delay %.2fms",delay);
	    
	    
	    fprintf(stderr,"\n");

	    fflush(stderr);
	}

	s.first_pts=0;
	s.last_pts=0;
    }
    

    return 0;
}
