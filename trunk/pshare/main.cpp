#include "upnp.h"
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include "soap.h"
#include "tmpl.h"

// TODO: mipsel - crash if fork(), '&' in playlist => '&amp;' but PS3 fail
// TODO: Internet Radio - MP3 - PS3 unsupported
// TODO: memory leak?

namespace dlna
{
    struct list
    {
        char* buf;
        int len;
        list* next;
    };

    // mime classes
    const char upnp_video[]     = "object.item.videoItem";
    const char upnp_audio[]     = "object.item.audioItem.musicTrack";
    const char upnp_container[] = "object.container";

    // mime types
    const char upnp_avi[]       = "http-get:*:video/avi:";
    const char upnp_asf[]       = "http-get:*:video/x-ms-asf:";
    const char upnp_wmv[]       = "http-get:*:video/x-ms-wmv:";
    const char upnp_mp4[]       = "http-get:*:video/mp4:";
    const char upnp_mpeg[]      = "http-get:*:video/mpeg:";
    const char upnp_mpeg2[]     = "http-get:*:video/mpeg2:";
    const char upnp_mp2t[]      = "http-get:*:video/mp2t:";
    const char upnp_mp2p[]      = "http-get:*:video/mp2p:";
    const char upnp_mov[]       = "http-get:*:video/quicktime:";
    const char upnp_aac[]       = "http-get:*:audio/x-aac:";
    const char upnp_ac3[]       = "http-get:*:audio/x-ac3:";
    const char upnp_mp3[]       = "http-get:*:audio/mpeg:";
    const char upnp_ogg[]       = "http-get:*:audio/x-ogg:";
    const char upnp_wma[]       = "http-get:*:audio/x-ms-wma:";

    struct mime
    {
        const char* file_ext;
        const char* upnp_class;
        const char* type;
    };

    mime mime_type_list[]=
    {
        {"mpg"  ,upnp_video,upnp_mpeg},                                 // default
        {"mpeg" ,upnp_video,upnp_mpeg},
        {"mpeg2",upnp_video,upnp_mpeg2},
        {"m2v"  ,upnp_video,upnp_mpeg2},
        {"ts"   ,upnp_video,upnp_mp2t},
        {"m2ts" ,upnp_video,upnp_mp2t},
        {"mts"  ,upnp_video,upnp_mp2t},
        {"vob"  ,upnp_video,upnp_mp2p},
        {"avi"  ,upnp_video,upnp_avi},
        {"asf"  ,upnp_video,upnp_asf},
        {"wmv"  ,upnp_video,upnp_wmv},
        {"mp4"  ,upnp_video,upnp_mp4},
        {"mov"  ,upnp_video,upnp_mov},
        {"aac"  ,upnp_audio,upnp_aac},
        {"ac3"  ,upnp_audio,upnp_ac3},
        {"mp3"  ,upnp_audio,upnp_mp3},
        {"ogg"  ,upnp_audio,upnp_ogg},
        {"wma"  ,upnp_audio,upnp_wma},
        {0,0,0}
    };

    struct playlist_item
    {
        int parent_id;
        int object_id;
        char* name;
        char* length;
        char* url;
        const char* upnp_class;
        const char* upnp_mime_type;
        int childs;

        playlist_item* next;
    };

    playlist_item* playlist_beg=0;
    playlist_item* playlist_end=0;
    int playlist_counter=0;
    playlist_item* playlist_root=0;

    playlist_item* playlist_add(playlist_item* parent,const char* name,const char* length,const char* url,const char* upnp_class,const char* upnp_mime_type)
    {
        if(!url || !*url)
            url="";

        if(!name || !*name)
            name=url;
        if(!length || !*length)
            length="-1";

        int name_len=strlen(name)+1;
        int length_len=strlen(length)+1;
        int url_len=strlen(url)+1;

        int len=name_len+length_len+url_len+sizeof(playlist_item);
        
        playlist_item* p=(playlist_item*)malloc(len);
        if(!p)
            return 0;

        p->parent_id=parent?parent->object_id:-1;
        p->object_id=playlist_counter;
        p->name=(char*)(p+1);
        p->length=p->name+name_len;
        p->url=p->length+length_len;
        p->upnp_class=upnp_class?upnp_class:"";
        p->upnp_mime_type=upnp_mime_type?upnp_mime_type:"";
        p->childs=0;

        strcpy(p->name,name);
        strcpy(p->length,length);
        strcpy(p->url,url);

        p->name[name_len-1]=0;
        p->length[length_len-1]=0;
        p->url[url_len-1]=0;

        p->next=0;

        if(!playlist_beg)
            playlist_beg=playlist_end=p;
        else
        {
            playlist_end->next=p;
            playlist_end=p;
        }

        playlist_counter++;

        if(parent)
            parent->childs++;

        return p;
    }

    void playlist_free(void)
    {
        while(playlist_beg)
        {
            playlist_item* p=playlist_beg;
            playlist_beg=playlist_beg->next;
            free(p);            
        }
        playlist_beg=0;
        playlist_end=0;
        playlist_counter=0;
    }

    playlist_item* playlist_find_by_id(int id)
    {
        for(playlist_item* p=playlist_beg;p;p=p->next)
            if(p->object_id==id)
                return p;
        return 0;
    }


    volatile int __sig_quit=0;
    volatile int __sig_alarm=0;
    volatile int __sig_child=0;
    int __sig_pipe[2]={-1,-1};
    sigset_t __sig_proc_mask;

    int sock_up=-1;
    int sock_down=-1;
    int sock_http=-1;

    FILE* verb_fp=0;

    static const char upnp_mgrp[]="239.255.255.250:1900";

    static const int upnp_notify_timeout=120;

    static const int http_timeout=15;

    static const char device_name[]="UPnP Playlist Browser";
    const char* device_friendly_name="UPnP-IPTV";

    static const char manufacturer[]="Anton Burdinuk <clark15b@gmail.com>";
    static const char manufacturer_url[]="http://code.google.com/p/tsdemuxer";
    static const char model_description[]="UPnP Playlist Browser from Anton Burdinuk <clark15b@gmail.com>";
    static const char model_number[]="0.0.1";
    static const char model_url[]="http://code.google.com/p/tsdemuxer";                                                        

    char device_uuid[64]="";

    int http_port=4044;


    const char www_root_def[]="/opt/share/pshare/www";
    const char pls_root_def[]="/opt/share/pshare/playlists";

    list* ssdp_alive_list=0;
    list* ssdp_byebye_list=0;

    upnp::mcast_grp mcast_grp;

    void __sig_handler(int n);

    int signal(int num,void (*handler)(int));

    int get_gmt_date(char* dst,int ndst);

    int parse_playlist_file(const char* name);
    int parse_playlist(const char* name);

    int init_ssdp(void);
    int done_ssdp(void);
    int send_ssdp_alive_notify(void);
    int send_ssdp_byebye_notify(void);
    int send_ssdp_msearch_response(sockaddr_in* sin,const char* st);
    int on_ssdp_message(char* buf,int len,sockaddr_in* sin);
    int on_http_connection(FILE* fp,sockaddr_in* sin);
    int upnp_print_item(FILE* fp,playlist_item* item);
    int upnp_browse(FILE* fp,int object_id,const char* flag,const char* filter,int index,int count);

    list* add_to_list(list* lst,const char* s,int len);
    void free_list(list* lst);

    template<typename T>
    T max(T a,T b) { return a>b?a:b; }
}

void dlna::__sig_handler(int n)
{
    int err=errno;

    switch(n)
    {
    case SIGINT:
    case SIGQUIT:
    case SIGTERM:
        __sig_quit=1;
        break;
    case SIGALRM:
        __sig_alarm=1;
        break;
    case SIGCHLD:
        __sig_child=1;
        break;
    }

    send(__sig_pipe[1],"$",1,MSG_DONTWAIT);

    errno=err;
}


int dlna::signal(int num,void (*handler)(int))
{
    struct sigaction action;

    sigfillset(&action.sa_mask);

    action.sa_flags=(num==SIGCHLD)?SA_NOCLDSTOP:0;

    action.sa_handler=handler;

    return sigaction(num,&action,0);
}

dlna::list* dlna::add_to_list(dlna::list* lst,const char* s,int len)
{
    list* ll=(list*)malloc(sizeof(list)+len);
    ll->next=0;
    ll->len=len;
    ll->buf=(char*)(ll+1);
    memcpy(ll->buf,s,len);

    if(lst)
        lst->next=ll;

    return ll;
}

void dlna::free_list(dlna::list* lst)
{
    while(lst)
    {
        list* p=lst;
        lst=lst->next;
        free(p);
    }
}

int dlna::get_gmt_date(char* dst,int ndst)
{
    time_t timestamp=time(0);

    tm* t=gmtime(&timestamp);

    static const char* wd[7]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char* m[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

    return snprintf(dst,ndst,"%s, %i %s %.4i %.2i:%.2i:%.2i GMT",wd[t->tm_wday],t->tm_mday,m[t->tm_mon],t->tm_year+1900,
        t->tm_hour,t->tm_min,t->tm_sec);
}


int main(int argc,char** argv)
{
    const char* mcast_iface="";
    int mcast_ttl=0;
    int mcast_loop=0;

    const char* playlist_filename="";
    const char* www_root="";

    const char* app_name=strrchr(argv[0],'/');
    if(app_name)
        app_name++;
    else
        app_name=argv[0];

    int opt;
    while((opt=getopt(argc,argv,"dvli:u:t:p:n:hr:?"))>=0)
        switch(opt)
        {
        case 'd':
            upnp::debug=1;
        case 'v':
            dlna::verb_fp=upnp::verb_fp=stderr;
            break;
        case 'l':
            mcast_loop=1;
            break;
        case 'i':
            mcast_iface=optarg;
            break;
        case 'u':
            snprintf(dlna::device_uuid,sizeof(dlna::device_uuid),"%s",optarg);
            break;
        case 't':
            mcast_ttl=atoi(optarg);
            break;
        case 'p':
            dlna::http_port=atoi(optarg);
            break;
        case 'n':
            dlna::device_friendly_name=optarg;
            break;
        case 'r':
            www_root=optarg;
            break;
        case 'h':
        case '?':
            fprintf(stderr,"\n%s UPnP Playlist Browser\n",app_name);
            fprintf(stderr,"\nThis program is a simple DLNA Media Server which provides ContentDirectory:1 service\n",app_name);
            fprintf(stderr,"    for sharing IPTV unicast streams over local area network (with 'udpxy' for multicast maybe).\n\n");
            fprintf(stderr,"Copyright (C) 2010 Anton Burdinuk\n\n");
            fprintf(stderr,"clark15b@gmail.com\n");
            fprintf(stderr,"http://code.google.com/p/tsdemuxer\n\n");


            fprintf(stderr,"USAGE: ./%s [-v] [-l] [-i iface] [-u device_uuid] [-t mcast_ttl] [-p http_port] [-r www_root] [playlist]\n",app_name);
            fprintf(stderr,"   -v          Turn on verbose output\n");
            fprintf(stderr,"   -d          Turn on verbose output + debug messages\n");
            fprintf(stderr,"   -l          Turn on loopback multicast transmission\n");
            fprintf(stderr,"   -i          Multicast interface address or device name\n");
            fprintf(stderr,"   -u          DLNA server UUID\n");
            fprintf(stderr,"   -n          DLNA server friendly name\n");
            fprintf(stderr,"   -t          Multicast datagrams time-to-live (TTL)\n");
            fprintf(stderr,"   -p          TCP port for incoming HTTP connections\n");
            fprintf(stderr,"   -r          WWW root directory (default: '%s')\n",dlna::www_root_def);
            fprintf(stderr,"    playlist   single file or directory with utf8-encoded *.m3u files (default: '%s')\n\n",dlna::pls_root_def);
            fprintf(stderr,"example 1: './%s -i eth0 playlist.m3u'\n",app_name);
            fprintf(stderr,"example 2: './%s -i 192.168.1.1 -u 32ccc90a-27a7-494a-a02d-71f8e02b1937 -n IPTV -t 1 -p 4044 playlists/'\n",app_name);
            fprintf(stderr,"example 3: './%s -v'\n\n",app_name);
            return 0;
        }


    if(argc>optind)
        playlist_filename=argv[optind];
    else
        playlist_filename=dlna::pls_root_def;

    if(!*www_root)
        www_root=dlna::www_root_def;

    if(mcast_ttl<1)
        mcast_ttl=1;
    else if(mcast_ttl>4)
        mcast_ttl=4;

    if(dlna::http_port<0)
        dlna::http_port=0;

    if(!*dlna::device_uuid)
    {
#ifndef NO_UUIDGEN  
        char filename[512]="";
        snprintf(filename,sizeof(filename),"/var/tmp/%s.uuid",dlna::device_friendly_name);

        FILE* fp=fopen(filename,"r");
        if(fp)
        {
            int n=fread(dlna::device_uuid,1,sizeof(dlna::device_uuid)-1,fp);
            dlna::device_uuid[n]=0;
            fclose(fp);
        }

        if(!*dlna::device_uuid)
        {
            upnp::uuid_gen(dlna::device_uuid);

            FILE* fp=fopen(filename,"w");
            if(fp)
            {
                fprintf(fp,"%s",dlna::device_uuid);
                fclose(fp);
            }
        }
#else
        strcpy(dlna::device_uuid,"32ccc90a-27a7-494a-a02d-71f8e02b1937");
#endif
    }


    if(!dlna::verb_fp)
    {
        pid_t pid=fork();
        if(pid==-1)
        {
            perror("fork");
            return 1;
        }else if(pid)
            exit(0);

        int fd=open("/dev/null",O_RDWR);
        if(fd!=-1)
        {
            for(int i=0;i<3;i++)
                dup2(fd,i);
            close(fd);
        }else
            for(int i=0;i<3;i++)
                close(i);
    }


    dlna::playlist_root=dlna::playlist_add(0,dlna::device_friendly_name,0,0,dlna::upnp_container,0);

    dlna::parse_playlist(playlist_filename);

    setsid();

    sigfillset(&dlna::__sig_proc_mask);
    socketpair(PF_UNIX,SOCK_STREAM,0,dlna::__sig_pipe);

    dlna::signal(SIGHUP ,SIG_IGN);
    dlna::signal(SIGPIPE,SIG_IGN);
    dlna::signal(SIGINT ,dlna::__sig_handler);
    dlna::signal(SIGQUIT,dlna::__sig_handler);
    dlna::signal(SIGTERM,dlna::__sig_handler);
    dlna::signal(SIGALRM,dlna::__sig_handler);
    dlna::signal(SIGCHLD,dlna::__sig_handler);

    sigprocmask(SIG_BLOCK,&dlna::__sig_proc_mask,0);

    dlna::mcast_grp.init(dlna::upnp_mgrp,mcast_iface,mcast_ttl,mcast_loop);

    setenv("DEV_FNAME",dlna::device_friendly_name,1);
    setenv("DEV_NAME",dlna::device_name,1);
    setenv("DEV_UUID",dlna::device_uuid,1);
    setenv("DEV_HOST",dlna::mcast_grp.interface,1);

    {
        char bb[32]; sprintf(bb,"%i",dlna::http_port);
        setenv("DEV_PORT",bb,1);
    }

    setenv("MANUFACTURER",dlna::manufacturer,1);
    setenv("MANUFACTURER_URL",dlna::manufacturer_url,1);
    setenv("MODEL_DESCRIPTION",dlna::model_description,1);
    setenv("MODEL_NUMBER",dlna::model_number,1);
    setenv("MODEL_URL",dlna::model_url,1);


    if(*www_root)
        chdir(www_root);

    if(dlna::verb_fp)
    {
        fprintf(dlna::verb_fp,"root device uuid: '%s'\n",dlna::device_uuid);
        fprintf(dlna::verb_fp,"device friendly name: '%s'\n",dlna::device_friendly_name);
    }
    dlna::sock_up=dlna::mcast_grp.upstream();

    if(dlna::sock_up!=-1)
    {
        dlna::sock_down=dlna::mcast_grp.join();

        if(dlna::sock_down!=-1)
        {
            dlna::sock_http=upnp::create_tcp_listener(dlna::http_port);

            if(dlna::sock_http!=-1)
            {
                fcntl(dlna::sock_http,F_SETFL,fcntl(dlna::sock_http,F_GETFL,0)|O_NONBLOCK);

                dlna::init_ssdp();

                alarm(dlna::upnp_notify_timeout);

                int maxfd=dlna::max(dlna::sock_up,dlna::sock_down);
                maxfd=dlna::max(maxfd,dlna::__sig_pipe[0]);
                maxfd=dlna::max(maxfd,dlna::sock_http);
                maxfd++;

                dlna::send_ssdp_alive_notify();

                while(!dlna::__sig_quit)
                {
                    fd_set fdset;

                    FD_ZERO(&fdset);
                    FD_SET(dlna::sock_up,&fdset);
                    FD_SET(dlna::sock_down,&fdset);
                    FD_SET(dlna::__sig_pipe[0],&fdset);
                    FD_SET(dlna::sock_http,&fdset);

                    sigprocmask(SIG_UNBLOCK,&dlna::__sig_proc_mask,0);

                    int rc=select(maxfd,&fdset,0,0,0);

                    sigprocmask(SIG_BLOCK,&dlna::__sig_proc_mask,0);

                    if(rc==-1)
                    {
                        if(errno!=EINTR)
                            break;
                    }

                    if(rc>0)
                    {
                        char tmp[1024];

                        if(FD_ISSET(dlna::__sig_pipe[0],&fdset))
                            while(recv(dlna::__sig_pipe[0],tmp,sizeof(tmp),MSG_DONTWAIT)>0);

                        if(FD_ISSET(dlna::sock_up,&fdset))
                            while(recv(dlna::sock_up,tmp,sizeof(tmp),MSG_DONTWAIT)>0);

                        if(FD_ISSET(dlna::sock_down,&fdset))
                        {
                            sockaddr_in sin;

                            int n;

                            while((n=dlna::mcast_grp.recv(dlna::sock_down,tmp,sizeof(tmp)-1,&sin,MSG_DONTWAIT))>0)
                            {
                                tmp[n]=0;

                                dlna::on_ssdp_message(tmp,n,&sin);
                            }
                        }

                        if(FD_ISSET(dlna::sock_http,&fdset))
                        {
                            sockaddr_in sin;
                            socklen_t sin_len=sizeof(sin);

                            int fd;
                            while((fd=accept(dlna::sock_http,(sockaddr*)&sin,&sin_len))!=-1)
                            {
                                pid_t pid=fork();

                                if(!pid)
                                {
                                    close(dlna::sock_up);
                                    close(dlna::sock_down);
                                    close(dlna::sock_http);
                                    for(int i=0;i<sizeof(dlna::__sig_pipe)/sizeof(*dlna::__sig_pipe);i++)
                                        close(dlna::__sig_pipe[i]);

                                    dlna::signal(SIGHUP ,SIG_DFL);
                                    dlna::signal(SIGPIPE,SIG_DFL);
                                    dlna::signal(SIGINT ,SIG_DFL);
                                    dlna::signal(SIGQUIT,SIG_DFL);
                                    dlna::signal(SIGTERM,SIG_DFL);
                                    dlna::signal(SIGALRM,SIG_DFL);
                                    dlna::signal(SIGCHLD,SIG_DFL);

                                    int on=1;
                                    setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&on,sizeof(on));

                                    sigprocmask(SIG_UNBLOCK,&dlna::__sig_proc_mask,0);

                                    FILE* fp=fdopen(fd,"a+");

                                    if(fp)
                                    {
                                        dlna::on_http_connection(fp,&sin);
                                        fclose(fp);
                                    }else
                                        close(fd);

                                    exit(0);
                                }

                                close(fd);
                            }
                        }
                    }

                    if(dlna::__sig_child)
                    {
                        dlna::__sig_child=0;

                        while(wait3(0,WNOHANG,0)>0);
                    }

                    if(dlna::__sig_alarm)
                    {
                        dlna::__sig_alarm=0;
                        alarm(dlna::upnp_notify_timeout);

                        dlna::send_ssdp_alive_notify();
                    }
                }

                dlna::send_ssdp_byebye_notify();

                dlna::done_ssdp();

                close(dlna::sock_http);
            }

            dlna::mcast_grp.leave(dlna::sock_down);
            dlna::mcast_grp.close(dlna::sock_down);
        }

        dlna::mcast_grp.close(dlna::sock_up);
    }

    dlna::playlist_free();

    dlna::signal(SIGTERM,SIG_IGN);

    sigprocmask(SIG_UNBLOCK,&dlna::__sig_proc_mask,0);

    kill(0,SIGTERM);

    for(int i=0;i<sizeof(dlna::__sig_pipe)/sizeof(*dlna::__sig_pipe);i++)
        close(dlna::__sig_pipe[i]);

    return 0;
}

int dlna::init_ssdp(void)
{
    char tmp[1024]="";
    int n;

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/dev.xml\r\n"
        "NT: uuid:%s\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s\r\n\r\n",
        mcast_grp.interface,http_port,device_uuid,device_name,device_uuid);

    list* ll=0; ssdp_alive_list=ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/dev.xml\r\n"
        "NT: upnp:rootdevice\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::upnp:rootdevice\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/dev.xml\r\n"
        "NT: urn:schemas-upnp-org:device:MediaServer:1\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:device:MediaServer:1\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/dev.xml\r\n"
        "NT: urn:schemas-upnp-org:service:ContentDirectory:1\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:service:ContentDirectory:1\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=add_to_list(ll,tmp,n);
/*
    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/dev.xml\r\n"
        "NT: urn:schemas-upnp-org:service:ConnectionManager:1\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:service:ConnectionManager:1\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=add_to_list(ll,tmp,n);
*/

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/dev.xml\r\n"
        "NT: uuid:%s\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s\r\n\r\n",
        mcast_grp.interface,http_port,device_uuid,device_name,device_uuid);

    ll=0; ssdp_byebye_list=ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/dev.xml\r\n"
        "NT: upnp:rootdevice\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::upnp:rootdevice\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/dev.xml\r\n"
        "NT: urn:schemas-upnp-org:device:MediaServer:1\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:device:MediaServer:1\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/dev.xml\r\n"
        "NT: urn:schemas-upnp-org:service:ContentDirectory:1\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:service:ContentDirectory:1\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

/*    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/t/dev.xml\r\n"
        "NT: urn:schemas-upnp-org:service:ConnectionManager:1\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:service:ConnectionManager:1\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=add_to_list(ll,tmp,n);
*/
    return 0;
}

int dlna::done_ssdp(void)
{
    free_list(ssdp_alive_list);
    free_list(ssdp_byebye_list);
    return 0;
}

int dlna::send_ssdp_alive_notify(void)
{
    for(list* l=ssdp_alive_list;l;l=l->next)
        mcast_grp.send(sock_up,l->buf,l->len);

    return 0;
}

int dlna::send_ssdp_byebye_notify(void)
{
    for(list* l=ssdp_byebye_list;l;l=l->next)
        mcast_grp.send(sock_up,l->buf,l->len);

    return 0;
}

int dlna::on_ssdp_message(char* buf,int len,sockaddr_in* sin)
{
    char tmp[1024];

    int line=0;

    int ignore=0;

    char what[256]="";

    for(int i=0,j=0;i<len && !ignore;i++)
    {
        if(buf[i]!='\r')
        {
            if(buf[i]!='\n')
            {
                if(j<sizeof(tmp)-1)
                {
                    ((unsigned char*)tmp)[j]=((unsigned char*)buf)[i];
                    j++;
                }
            }else
            {
                tmp[j]=0;

                if(j>0)
                {
                    if(!line)
                    {
                        char* p=strchr(tmp,' ');
                        if(p)
                        {
                            *p=0;
                            if(strcasecmp(tmp,"M-SEARCH"))
                                ignore=1;
                        }
                    }else
                    {
                        char* p=strchr(tmp,':');
                        if(p)
                        {
                            *p=0; p++;
                            while(*p && *p==' ')
                                p++;

                            if(!strcasecmp(tmp,"MAN"))
                            {
                                if(strcasecmp(p,"\"ssdp:discover\""))
                                    ignore=1;
                            }else if(!strcasecmp(tmp,"ST"))
                            {
                                int n=snprintf(what,sizeof(what),"%s",p);
                                if(n==-1 || n>=sizeof(what))
                                    what[sizeof(what)-1]=0;
                            }
                        }
                    }
                }
                j=0;
                line++;
            }
        }
    }

    if(!ignore)
        send_ssdp_msearch_response(sin,what);

    return 0;
}

int dlna::send_ssdp_msearch_response(sockaddr_in* sin,const char* st)
{
    const char* what=st;

    static const char dev_type[]="urn:schemas-upnp-org:device:MediaServer:1";

    if(strcmp(st,"ssdp:all") && strcmp(st,"upnp:rootdevice"))
    {
        static const char tag[]="urn:";
        static const char tag2[]="uuid:";

        if(!strncmp(st,tag,sizeof(tag)-1))
        {
            const char* p=st+sizeof(tag)-1;

            if(strcmp(p,"schemas-upnp-org:device:MediaServer:1") && strcmp(p,"schemas-upnp-org:service:ContentDirectory:1") &&
                strcmp(p,"schemas-upnp-org:service:ConnectionManager:1"))
                    return -1;
        }else if(!strncmp(st,tag2,sizeof(tag2)-1))
        {
            const char* p=st+sizeof(tag2)-1;

            if(strcmp(p,device_uuid))
                    return -1;

            what=dev_type;
        }
    }else
        what=dev_type;


    char date[64], tmp[1024];

    get_gmt_date(date,sizeof(date));

    int n=snprintf(tmp,sizeof(tmp),
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "DATE: %s\r\n"
        "EXT:\r\n"
        "LOCATION: http://%s:%i/t/dev.xml\r\n"
        "Server: %s\r\n"
        "ST: %s\r\n"
        "USN: uuid:%s::%s\r\n\r\n",
        date,mcast_grp.interface,http_port,device_name,st,device_uuid,what);

    mcast_grp.send(sock_up,tmp,n,sin);

    return 0;
}

int dlna::on_http_connection(FILE* fp,sockaddr_in* sin)
{
    alarm(http_timeout);

    do
    {
        // read HTTP request

        enum {m_get=1, m_post=2};

        int method=0;

        char req[128]="";

        char soap_action[128]="";

        int content_length=-1;

        char* content=0;

        char tmp[1024];

        int line=0;

        while(fgets(tmp,sizeof(tmp),fp))
        {
            char* p=strpbrk(tmp,"\r\n");
            if(p)
                *p=0;

            if(!*tmp)
                break;

            if(!line)
            {
                char* uri=strchr(tmp,' ');
                if(uri)
                {
                    *uri=0; uri++;
                    while(*uri && *uri==' ')
                        uri++;

                    char* ver=strchr(uri,' ');
                    if(ver)
                    {
                        *ver=0; ver++;
                        while(*ver && *ver==' ')
                            ver++;

                        if(!strcasecmp(tmp,"GET"))
                            method=m_get;
                        else if(!strcasecmp(tmp,"POST"))
                            method=m_post;

                        snprintf(req,sizeof(req),"%s",uri);
                    }
                }
            }else
            {
                p=strchr(tmp,':');
                if(p)
                {
                    *p=0; p++;
                    while(*p && *p==' ')
                        p++;

                    if(!strcasecmp(tmp,"Content-Length"))
                    {
                        char* endptr=0;
                        int ll=strtol(p,&endptr,10);

                        if((!*endptr || *endptr==' ') && ll>=0)
                            content_length=ll;
                    }else if(!strcasecmp(tmp,"SOAPAction"))
                    {
                        if(*p=='\"')
                            p++;

                        int n=snprintf(soap_action,sizeof(soap_action),"%s",p);
                        if(n==-1 || n>=sizeof(soap_action))
                        {
                            n=sizeof(soap_action)-1;
                            soap_action[n]=0;
                        }
                        if(n>0 && soap_action[n-1]=='\"')
                            soap_action[n-1]=0;
                    }
                    
                }
            }

            line++;
        }

        if(!method || !*req || content_length>4096 || (method==m_get && content_length>0))
            break;

        if(content_length>0)
        {
            content=(char*)malloc(content_length+1);
            if(!content)
                break;

            int l=0;
            while(l<content_length)
            {
                int n=fread(content+l,1,content_length-l,fp);
                if(!n)
                    break;
                l+=n;
            }

            if(l!=content_length)
            {
                free(content);
                break;
            }

            content[content_length]=0;
        }


        if(verb_fp)
        {
            fprintf(verb_fp,"%s '%s' from '%s:%i'\n",method==m_post?"POST":"GET",req,inet_ntoa(sin->sin_addr),ntohs(sin->sin_port));

            if(upnp::debug && content)
            {
                fwrite(content,content_length,1,verb_fp);
                fprintf(verb_fp,"\n");
            }
        }

        char date[64];

        get_gmt_date(date,sizeof(date));



        static const char ttag[]="/t/";

        if(!strcmp(req,"/"))
            tmpl::get_file("index.html",fp,1,date,device_name);
        else if(!strncmp(req,ttag,sizeof(ttag)-1))
            tmpl::get_file(req+sizeof(ttag)-1,fp,1,date,device_name);
        else if(!strcmp(req,"/cds_control"))
        {
            fprintf(fp,"HTTP/1.1 200 OK\r\nPragma: no-cache\r\nDate: %s\r\nServer: %s\r\nContent-Type: text/xml\r\nConnection: close\r\n\r\n",
                date,device_name);

            if(*soap_action)
            {
                soap::node req;

                if(content && content_length>0)
                    soap::parse(content,content_length,&req);

                if(verb_fp)
                    fprintf(verb_fp,"SOAPAction: %s\n",soap_action);

                char* req_type=strchr(soap_action,'#');
                
                if(req_type)
                {
                    req_type++;

                    if(!strcmp(req_type,"Browse"))
                    {
                        soap::node* req_data=req.find("Envelope/Body/Browse");

                        if(req_data)
                        {
                            int object_id=atoi(req_data->find_data("ObjectID"));
                            const char* flag=req_data->find_data("BrowseFlag");
                            const char* filter=req_data->find_data("Filter");
                            int index=atoi(req_data->find_data("StartingIndex"));
                            int count=atoi(req_data->find_data("RequestedCount"));

                            if(verb_fp)
                                fprintf(verb_fp,"Browse: ObjectID=%i, BrowseFlag='%s', StartingIndex=%i, RequestedCount=%i\n",
                                    object_id,flag,index,count);

                            upnp_browse(fp,object_id,flag,filter,index,count);
                        }
                    }else if(!strcmp(req_type,"GetSystemUpdateID"))
                    {
                        fprintf(fp,
                            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
                            "   <s:Body>\n"
                            "      <u:GetSystemUpdateIDResponse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">\n"
                            "         <Id>1</Id>\n"
                            "      </u:GetSystemUpdateIDResponse>\n"
                            "   </s:Body>\n"
                            "</s:Envelope>\n");                
                    }
                }
            }
        }
        else
            tmpl::get_file(req,fp,0,date,device_name);

        fflush(fp);

        if(content)
            free(content);

        alarm(http_timeout);


    }while(0);

    alarm(0);

    return 0;
}


int dlna::parse_playlist(const char* name)
{
    struct stat st;

    int rc=stat(name,&st);

    if(rc==-1)
        return -1;

    if(S_IFDIR&st.st_mode)
    {
        char buf[512];

        int n=snprintf(buf,sizeof(buf),"%s/",name);
        if(n==-1 || n>=sizeof(buf))
            n=sizeof(buf)-1;

        if(n>1 && buf[n-2]=='/')
            n--;

        int m=sizeof(buf)-n;

        DIR* dfp=opendir(name);

        if(!dfp)
            return -1;

        dirent* d=0;

        while((d=readdir(dfp)))
        {
            if(*d->d_name!='.')
            {
                int nn=snprintf(buf+n,m,"%s",d->d_name);
                if(nn==-1 || nn>=m)
                    buf[sizeof(buf)-1]=0;

                parse_playlist_file(buf);
            }
        }

        closedir(dfp);
    }else
        parse_playlist_file(name);
    
    return 0;
}

int dlna::parse_playlist_file(const char* name)
{
    const char* ext=strrchr(name,'.');

    if(!ext || strcasecmp(ext+1,"m3u"))
        return -1;

    FILE* fp=fopen(name,"r");
    
    if(fp)
    {
        char playlist_name[64]="";

        {
            const char* fname=strrchr(name,'/');
            if(!fname)
                fname=name;
            else
                fname++;

            int n=ext-fname;

            if(n>=sizeof(playlist_name))
                n=sizeof(playlist_name)-1;
            memcpy(playlist_name,fname,n);
            playlist_name[n]=0;
        }
    
        if(verb_fp)
            fprintf(verb_fp,"playlist: '%s' -> %s\n",playlist_name,name);

        playlist_item* parent_item=playlist_add(playlist_root,playlist_name,0,0,upnp_container,0);

        char track_length[32]="";
        char track_name[64]="";
        char track_url[256]="";
        const char* track_type=0;
        const char* track_class=0;

        char buf[256];
        while(fgets(buf,sizeof(buf),fp))
        {
            char* p=strpbrk(buf,"\r\n");
            if(p)
                *p=0;
            p=buf;
            if(!strncmp(p,"\xEF\xBB\xBF",3))    // skip BOM
                p+=3;

            if(!*p)
                continue;

            if(*p=='#')
            {
                p++;
                static const char tag[]="EXTINF:";
                if(!strncmp(p,tag,sizeof(tag)-1))
                {
                    p+=sizeof(tag)-1;

                    char* p2=strchr(p,',');
                    if(p2)
                    {
                        *p2=0;
                        p2++;

                        int n=snprintf(track_length,sizeof(track_length),"%s",p);
                        if(n==-1 || n>=sizeof(track_length))
                            track_length[sizeof(track_length)-1]=0;

                        n=snprintf(track_name,sizeof(track_name),"%s",p2);
                        if(n==-1 || n>=sizeof(track_name))
                            track_name[sizeof(track_name)-1]=0;
                    }
                }
            }else
            {
                int n=snprintf(track_url,sizeof(track_url),"%s",p);
                if(n==-1 || n>=sizeof(track_url))
                    track_url[sizeof(track_url)-1]=0;

                const char* p=track_url+strlen(track_url)-1;
                while(p>track_url && p[-1]!='/' && p[-1]!='.') p--;
                if(p[-1]=='.')
                {
                    for(int i=0;mime_type_list[i].file_ext;i++)
                    {
                        if(!strcmp(p,mime_type_list[i].file_ext))
                        {
                            track_type=mime_type_list[i].type;
                            track_class=mime_type_list[i].upnp_class;
                            break;
                        }
                    }
                }

                if(!track_type)
                {
                    track_type=upnp_mpeg;
                    track_class=upnp_video;
                }

                if(verb_fp && upnp::debug)
                    fprintf(verb_fp,"   len=%s, name='%s', url='%s', mime=%s (%s)\n",track_length,track_name,track_url,track_type,track_class);

                playlist_item* item=playlist_add(parent_item,track_name,track_length,track_url,track_class,track_type);

                *track_length=0;
                *track_name=0;
                *track_url=0;
                track_type=0;
                track_class=0;
            }
        }


        fclose(fp);
    }

    return 0;
}

int dlna::upnp_print_item(FILE* fp,playlist_item* item)
{
    if(item->upnp_class==upnp_container)
        fprintf(fp,"&lt;container id=&quot;%i&quot; childCount=&quot;%i&quot; parentID=&quot;%i&quot; restricted=&quot;false&quot;&gt;",
            item->object_id,item->childs,item->parent_id);
    else
        fprintf(fp,"&lt;item id=&quot;%i&quot; parentID=&quot;%i&quot; restricted=&quot;true&quot;&gt;",item->object_id,item->parent_id);

    fprintf(fp,"&lt;dc:title&gt;");
    
    tmpl::print_to_xml(item->name,fp);

    fprintf(fp,"&lt;/dc:title&gt;&lt;upnp:class&gt;%s&lt;/upnp:class&gt;",item->upnp_class);

    if(item->upnp_class!=upnp_container)
        fprintf(fp,"&lt;res protocolInfo=\"%s\" duration=\"%s\"&gt;%s&lt;/res&gt;&lt;/item&gt;",item->upnp_mime_type,item->length,item->url);
    else
        fprintf(fp,"&lt;/container&gt;");


    return 0;
}

int dlna::upnp_browse(FILE* fp,int object_id,const char* flag,const char* filter,int index,int count)
{
    playlist_item def_item= { -1, object_id, (char*)"???", (char*)"-1", (char*)"", upnp_container, "", 0 };

    int num=0;
    int total_num=0;

    fprintf(fp,
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\n"
        "   <s:Body>\n"
        "      <u:BrowseResponse xmlns:u=\"urn:schemas-upnp-org:service:ContentDirectory:1\">\n"
        "         <Result>&lt;DIDL-Lite xmlns=&quot;urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/&quot; xmlns:dc=&quot;http://purl.org/dc/elements/1.1/&quot; xmlns:upnp=&quot;urn:schemas-upnp-org:metadata-1-0/upnp/&quot;&gt;");


    if(!strcmp(flag,"BrowseMetadata"))
    {
        playlist_item* item=playlist_find_by_id(object_id);

        upnp_print_item(fp,item?item:&def_item);

        num=total_num=1;
    }else
    {
        {
            playlist_item* ii=playlist_find_by_id(object_id);
            if(ii)
                total_num=ii->childs;
        }

        int n=0;
        for(playlist_item* item=playlist_beg;item;item=item->next)
        {
            if(item->parent_id==object_id)
            {
                if(n>=index)
                {
                    upnp_print_item(fp,item);
//upnp_print_item(stderr,item);
                    num++;

                    if(count>0 && num>=count)
                        break;
                }
                n++;
            }
        }
    }


    fprintf(fp,"&lt;/DIDL-Lite&gt;</Result>\n"
        "         <NumberReturned>%i</NumberReturned>\n"
        "         <TotalMatches>%i</TotalMatches>\n"
        "         <UpdateID>1</UpdateID>\n"
        "      </u:BrowseResponse>\n"
        "   </s:Body>\n"
        "</s:Envelope>\n",num,total_num);

    return 0;
}

