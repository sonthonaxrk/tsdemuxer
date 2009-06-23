/*

 Copyright (C) 2009 Anton Burdinuk

 clark15b@gmail.com

*/

#ifndef __TS_H
#define __TS_H

#include <sys/types.h>
#include <map>
#include <string>
#include <stdio.h>

#ifndef _WIN32
#define O_BINARY                0
#else
typedef unsigned char           u_int8_t;
typedef unsigned short          u_int16_t;
typedef unsigned long           u_int32_t;
typedef unsigned long long      u_int64_t;
#endif

namespace ts
{
    inline u_int8_t to_byte(const char* p)
        { return *((unsigned char*)p); }

    inline u_int16_t to_int(const char* p)
        { u_int16_t n=((unsigned char*)p)[0]; n<<=8; n+=((unsigned char*)p)[1]; return n; }

    inline u_int32_t to_int32(const char* p)
    {
        u_int32_t n=((unsigned char*)p)[0];
        n<<=8; n+=((unsigned char*)p)[1];
        n<<=8; n+=((unsigned char*)p)[2];
        n<<=8; n+=((unsigned char*)p)[3];
        return n;
    }

    class table
    {
    public:
        enum { max_buf_len=512 };

        char buf[max_buf_len];

        u_int16_t len;

        u_int16_t offset;

        table(void):offset(0),len(0) {}

        void reset(void) { offset=0; len=0; }
    };

    class file
    {
    protected:
        int fd;

        enum { max_buf_len=2048 };

        char buf[max_buf_len];

        int len,offset;
    public:
        std::string filename;
    public:
        file(void):fd(-1),len(0),offset(0) {}
        ~file(void);

        enum { in=0, out=1 };

        bool open(int mode,const char* fmt,...);
        void close(void);
        int write(const char* p,int l);
        int flush(void);
        int read(char* p,int l);

        bool is_opened(void) { return fd==-1?false:true; }
    };

    namespace stream_type
    {
        enum
        {
            data                = 0,
            mpeg2_video         = 1,
            h264_video          = 2,
            vc1_video           = 3,
            ac3_audio           = 4,
            mpeg2_audio         = 5,
            lpcm_audio          = 6
        };
    }

    class stream
    {
    public:
        u_int16_t channel;                      // channel number (1,2 ...)
        u_int8_t  id;                           // stream number in channel
        u_int8_t  type;                         // 0xff                 - not ES
                                                // 0x01,0x02            - MPEG2 video
                                                // 0x80                 - MPEG2 video (for TS only, not M2TS)
                                                // 0x1b                 - H.264 video
                                                // 0xea                 - VC-1  video
                                                // 0x81,0x06,0x83       - AC3   audio
                                                // 0x03,0x04            - MPEG2 audio
                                                // 0x80                 - LPCM  audio

        table psi;                              // PAT,PMT cache (only for PSI streams)

        u_int8_t stream_id;                     // MPEG stream id

        ts::file file;                          // output ES file
        FILE* timecodes;

        u_int64_t dts;                          // current MPEG stream DTS (presentation time for audio, decode time for video)
        u_int64_t first_pts;
        u_int64_t last_pts;
        u_int32_t frame_length;                 // frame length in ticks (90 ticks = 1 ms, 90000/frame_length=fps)
        u_int64_t frame_num;                    // frame counter
        u_int32_t nal_ctx;
        u_int64_t nal_frame_num;                // JVT NAL (h.264) frame counter

        stream(void):channel(0xffff),id(0),type(0xff),stream_id(0),
            dts(0),first_pts(0),last_pts(0),frame_length(0),frame_num(0),nal_ctx(0),nal_frame_num(0),timecodes(0) {}

        ~stream(void);

        void reset(void)
        {
            psi.reset();
            dts=first_pts=last_pts=0;
            frame_length=0;
            frame_num=0;
            nal_ctx=0;
            nal_frame_num=0;
        }
    };


    class demuxer
    {
    public:
        std::map<u_int16_t,stream> streams;
        bool hdmv;                                      // HDMV mode, using 192 bytes packets
        bool av_only;                                   // Audio/Video streams only
        bool parse_only;                                // no demux
        int dump;                                       // 0 - no dump, 1 - dump M2TS timecodes, 2 - dump PTS/DTS, 3 - dump tracks
        int channel;                                    // channel for demux
        std::string prefix;                             // output file name prefix
    protected:

        u_int64_t base_pts;

        bool validate_type(u_int8_t type);
        u_int64_t decode_pts(const char* ptr);
        int get_stream_type(u_int8_t type);
        const char* get_stream_ext(u_int8_t type_id);

        // take 188/192 bytes TS/M2TS packet
        int demux_ts_packet(const char* ptr);

        void write_timecodes(FILE* fp,u_int64_t first_pts,u_int64_t last_pts,u_int32_t frame_num);
    public:
        demuxer(void):hdmv(false),av_only(true),parse_only(false),dump(0),channel(0),base_pts(0) {}

        void show(void);

        int demux_file(const char* name);

        int gen_timecodes(void);

        void reset(void)
        {
            for(std::map<u_int16_t,stream>::iterator i=streams.begin();i!=streams.end();++i)
                i->second.reset();
        }
    };
}


#endif
