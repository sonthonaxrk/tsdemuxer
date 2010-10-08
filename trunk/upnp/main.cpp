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
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace dlna
{
    struct list
    {
        char* buf;
        int len;
        list* next;
    };

    enum { upnp_dev_location_size=64 };

    struct upnp_dev
    {
        sockaddr_in sin;
        time_t timestamp;
        char location[upnp_dev_location_size];
    };

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

    static const char device_name[]="Lolipop IPTV Directory";

    char device_uuid[64]="";

    int http_port=4044;

    list* ssdp_alive_list=0;
    list* ssdp_byebye_list=0;

    upnp::mcast_grp mcast_grp;

    void __sig_handler(int n);

    int signal(int num,void (*handler)(int));

    int get_gmt_date(char* dst,int ndst);

    int init_ssdp(void);
    int done_ssdp(void);
    int send_ssdp_alive_notify(void);
    int send_ssdp_byebye_notify(void);
    int send_ssdp_msearch_response(sockaddr_in* sin);
    int on_ssdp_message(char* buf,int len,sockaddr_in* sin);
    int on_http_connection(FILE* fp,sockaddr_in* sin);

    list* add_to_list(list* lst,const char* s,int len);
    void free_list(list* lst);

    enum { max_upnp_dev_num=8 };
    upnp_dev devices[max_upnp_dev_num];

    void init_upnp_devices(void);
    void add_upnp_device(sockaddr_in* sin,const char* location);

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

void dlna::init_upnp_devices(void)
{
    for(int i=0;i<max_upnp_dev_num;i++)
    {
        devices[i].sin.sin_addr.s_addr=0;
        devices[i].sin.sin_port=0;
        *devices[i].location=0;
        devices[i].timestamp=0;
    }
}
void dlna::add_upnp_device(sockaddr_in* sin,const char* location)
{
    int n=-1, m=-1;

    for(int i=0;i<max_upnp_dev_num;i++)
    {
        if(!devices[i].sin.sin_addr.s_addr ||
            (devices[i].sin.sin_addr.s_addr==sin->sin_addr.s_addr /*&& devices[i].sin.sin_port==sin->sin_port*/))
                { n=i; break; }
        else
        {
            if(m==-1 || devices[i].timestamp<=devices[m].timestamp)
                m=i;
        }
    }

    if(n==-1)
        n=m;

    if(n!=-1)
    {
        devices[n].sin.sin_addr.s_addr=sin->sin_addr.s_addr;
        devices[n].sin.sin_port=sin->sin_port;
        devices[n].timestamp=time(0);
        if(!location)
            *devices[n].location=0;
        else
        {
            int nn=snprintf(devices[n].location,upnp_dev_location_size,"%s",location);
            if(nn<0 || nn>=upnp_dev_location_size)
                devices[n].location[upnp_dev_location_size-1]=0;
        }
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


    const char* app_name=strrchr(argv[0],'/');
    if(app_name)
        app_name++;
    else
        app_name=argv[0];

    int opt;
    while((opt=getopt(argc,argv,"dvli:u:t:p:h?"))>=0)
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
        case 'h':
        case '?':
            fprintf(stderr,"USAGE: ./%s [-v] [-l] [-i iface] [-u device_uuid] [-t mcast_ttl] [-p http_port]\n",app_name);
            fprintf(stderr,"   -v   Turn on verbose output\n");
            fprintf(stderr,"   -d   Turn on verbose output + debug messages\n");
            fprintf(stderr,"   -l   Turn on loopback multicast transmission\n");
            fprintf(stderr,"   -i   Multicast interface address or device name\n");
            fprintf(stderr,"   -u   DLNA server UUID\n");
            fprintf(stderr,"   -t   Multicast datagrams time-to-live (TTL)\n");
            fprintf(stderr,"   -p   TCP port for incoming HTTP connections\n\n");
            fprintf(stderr,"example 1: './%s -i eth0'\n",app_name);
            fprintf(stderr,"example 2: './%s -i 192.168.1.1 -u 32ccc90a-27a7-494a-a02d-71f8e02b1937 -t 1 -p 4044'\n",app_name);
            fprintf(stderr,"example 3: './%s -v'\n\n",app_name);
            return 0;
        }

    if(mcast_ttl<1)
        mcast_ttl=1;
    else if(mcast_ttl>4)
        mcast_ttl=4;

    if(dlna::http_port<0)
        dlna::http_port=0;

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

    dlna::init_upnp_devices();

    if(!*dlna::device_uuid)
        upnp::uuid_gen(dlna::device_uuid);

    if(dlna::verb_fp)
        fprintf(dlna::verb_fp,"root device uuid: '%s'\n",dlna::device_uuid);

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
        "LOCATION: http://%s:%i/root.xml\r\n"
        "NT: upnp:rootdevice\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::upnp:rootdevice\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    list* ll=0; ssdp_alive_list=ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/root.xml\r\n"
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
        "LOCATION: http://%s:%i/root.xml\r\n"
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
        "LOCATION: http://%s:%i/root.xml\r\n"
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
        "LOCATION: http://%s:%i/root.xml\r\n"
        "NT: upnp:rootdevice\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::upnp:rootdevice\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=0; ssdp_byebye_list=ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/root.xml\r\n"
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
        "LOCATION: http://%s:%i/root.xml\r\n"
        "NT: urn:schemas-upnp-org:service:ContentDirectory:1\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:service:ContentDirectory:1\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

/*
    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/root.xml\r\n"
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

    int upnp_added=0;

    for(int i=0,j=0;i<len;i++)
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
                                if(strcasecmp(p,"urn:schemas-upnp-org:device:MediaServer:1") &&
                                    strcasecmp(p,"ssdp:all") && strcasecmp(p,"upnp:rootdevice") &&
                                        strcasecmp(p,"urn:schemas-upnp-org:service:ContentDirectory:1"))
                                            ignore=1;
                            }else if(!strcasecmp(tmp,"Location"))
                            {
                                add_upnp_device(sin,p);
                                upnp_added=1;
                            }
                        }
                    }
                }
                j=0;
                line++;
            }
        }
    }

    if(!upnp_added)
        add_upnp_device(sin,0);

    if(!ignore)
        send_ssdp_msearch_response(sin);

    return 0;
}

int dlna::send_ssdp_msearch_response(sockaddr_in* sin)
{
    char date[64], tmp[1024];

    get_gmt_date(date,sizeof(date));

    int n=snprintf(tmp,sizeof(tmp),
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "DATE: %s\r\n"
        "EXT:\r\n"
        "LOCATION: http://%s:%i/root.xml\r\n"
        "Server: %s\r\n"
        "ST: urn:schemas-upnp-org:device:MediaServer:1\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:device:MediaServer:1\r\n\r\n",
        date,mcast_grp.interface,http_port,device_name,device_uuid);

    mcast_grp.send(sock_up,tmp,n,sin);

    return 0;
}

int dlna::on_http_connection(FILE* fp,sockaddr_in* sin)
{
    int keep_alive=0;

    alarm(http_timeout);

    do
    {
        enum {m_get=1, m_post=2};

        int method=0;

        char req[128]="";

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

                        if(!strcasecmp(ver,"HTTP/1.1"))
                            keep_alive=1;
                        else
                            keep_alive=0;

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

                    if(!strcasecmp(tmp,"Connection"))
                    {
                        if(!strcasecmp(p,"Keep-Alive"))
                            keep_alive=1;
                        else
                            keep_alive=0;
                    }else if(!strcasecmp(tmp,"Content-Length"))
                    {
                        char* endptr=0;
                        int ll=strtol(p,&endptr,10);

                        if((!*endptr || *endptr==' ') && ll>=0)
                            content_length=ll;
                        else
                            keep_alive=0;
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

        keep_alive=0;

        char date[64];

        get_gmt_date(date,sizeof(date));


        fprintf(fp,"HTTP/1.1 200 OK\r\nPragma: no-cache\r\nDate: %s\r\nServer: %s\r\nConnection: %s\r\n",
            date,device_name,keep_alive?"Keep-Alive":"close");

        if(!strcmp(req,"/root.xml"))
        {
            fprintf(fp,
                "Content-Type: text/xml\r\n\r\n"
                "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
                "<root xmlns:dlna=\"urn:schemas-dlna-org:device-1-0\" xmlns=\"urn:schemas-upnp-org:device-1-0\">\n"
                "    <specVersion>\n"
                "        <major>1</major>\n"
                "        <minor>0</minor>\n"
                "    </specVersion>\n"
                "    <device>\n"
                "        <deviceType>urn:schemas-upnp-org:device:MediaServer:1</deviceType>\n"
                "        <friendlyName>%s</friendlyName>\n"
                "        <manufacturer>Anton Burdinuk</manufacturer>\n"
                "        <manufacturerURL>http://code.google.com/p/tsdemuxer</manufacturerURL>\n"
                "        <modelDescription>UPnP Content Directory server from clark15b@doom9</modelDescription>\n"
                "        <modelName>%s</modelName>\n"
                "        <modelNumber>0.0.1</modelNumber>\n"
                "        <modelURL>http://code.google.com/p/tsdemuxer</modelURL>\n"
                "        <serialNumber/>\n"
                "        <UPC/>\n"
                "        <UDN>uuid:%s</UDN>\n"
                "        <iconList>\n"
                "        </iconList>\n"
                "        <presentationURL>/</presentationURL>\n"
                "        <serviceList>\n"
                "            <service>\n"
                "                <serviceType>urn:schemas-upnp-org:service:ContentDirectory:1</serviceType>\n"
                "                <serviceId>urn:upnp-org:serviceId:ContentDirectory</serviceId>\n"
                "                <SCPDURL>/cds.xml</SCPDURL>\n"
                "                <controlURL>/cds_control</controlURL>\n"
                "                <eventSubURL>/cds_event</eventSubURL>\n"
                "            </service>\n"
/*
                "            <service>\n"
                "               <serviceType>urn:schemas-upnp-org:service:ConnectionManager:1</serviceType>\n"
                "               <serviceId>urn:upnp-org:serviceId:ConnectionManager</serviceId>\n"
                "               <SCPDURL>/cms.xml</SCPDURL>\n"
                "               <controlURL>/cms_control</controlURL>\n"
                "               <eventSubURL>/cms_event</eventSubURL>\n"
                "            </service>\n"
*/
                "        </serviceList>\n"
                "    </device>\n"
                "    <URLBase>http://%s:%i/</URLBase>\n"
                "</root>\n",
                device_name,device_name,device_uuid,mcast_grp.interface,http_port);
        }
        else if(!strcmp(req,"/cds.xml"))
        {
            static const char cds[]=
                "Content-Type: text/xml\r\n\r\n"
                "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                "<scpd xmlns=\"urn:schemas-upnp-org:service-1-0\">"
                "   <specVersion>"
                "      <major>1</major>"
                "      <minor>0</minor>"
                "   </specVersion>"
                "   <actionList>"
                "      <action>"
                "         <name>GetSystemUpdateID</name>"
                "         <argumentList>"
                "            <argument>"
                "               <name>Id</name>"
                "               <direction>out</direction>"
                "               <relatedStateVariable>SystemUpdateID</relatedStateVariable>"
                "            </argument>"
                "         </argumentList>"
                "      </action>"
                "      <action>"
                "         <name>Browse</name>"
                "         <argumentList>"
                "            <argument>"
                "               <name>ObjectID</name>"
                "               <direction>in</direction>"
                "               <relatedStateVariable>A_ARG_TYPE_ObjectID</relatedStateVariable>"
                "            </argument>"
                "            <argument>"
                "               <name>BrowseFlag</name>"
                "               <direction>in</direction>"
                "               <relatedStateVariable>A_ARG_TYPE_BrowseFlag</relatedStateVariable>"
                "            </argument>"
                "            <argument>"
                "               <name>Filter</name>"
                "               <direction>in</direction>"
                "               <relatedStateVariable>A_ARG_TYPE_Filter</relatedStateVariable>"
                "            </argument>"
                "            <argument>"
                "               <name>StartingIndex</name>"
                "               <direction>in</direction>"
                "               <relatedStateVariable>A_ARG_TYPE_Index</relatedStateVariable>"
                "            </argument>"
                "            <argument>"
                "               <name>RequestedCount</name>"
                "               <direction>in</direction>"
                "               <relatedStateVariable>A_ARG_TYPE_Count</relatedStateVariable>"
                "            </argument>"
                "            <argument>"
                "               <name>SortCriteria</name>"
                "               <direction>in</direction>"
                "               <relatedStateVariable>A_ARG_TYPE_SortCriteria</relatedStateVariable>"
                "            </argument>"
                "            <argument>"
                "               <name>Result</name>"
                "               <direction>out</direction>"
                "               <relatedStateVariable>A_ARG_TYPE_Result</relatedStateVariable>"
                "            </argument>"
                "            <argument>"
                "               <name>NumberReturned</name>"
                "               <direction>out</direction>"
                "               <relatedStateVariable>A_ARG_TYPE_Count</relatedStateVariable>"
                "            </argument>"
                "            <argument>"
                "               <name>TotalMatches</name>"
                "               <direction>out</direction>"
                "               <relatedStateVariable>A_ARG_TYPE_Count</relatedStateVariable>"
                "            </argument>"
                "            <argument>"
                "               <name>UpdateID</name>"
                "               <direction>out</direction>"
                "               <relatedStateVariable>A_ARG_TYPE_UpdateID</relatedStateVariable>"
                "            </argument>"
                "         </argumentList>"
                "      </action>"
                "   </actionList>"
                "   <serviceStateTable>"
                "      <stateVariable sendEvents=\"yes\">"
                "         <name>SystemUpdateID</name>"
                "         <dataType>ui4</dataType>"
                "      </stateVariable>"
                "      <stateVariable sendEvents=\"no\">"
                "         <name>A_ARG_TYPE_ObjectID</name>"
                "         <dataType>string</dataType>"
                "      </stateVariable>"
                "      <stateVariable sendEvents=\"no\">"
                "         <name>A_ARG_TYPE_BrowseFlag</name>"
                "         <dataType>string</dataType>"
                "         <allowedValueList>"
                "            <allowedValue>BrowseMetadata</allowedValue>"
                "            <allowedValue>BrowseDirectChildren</allowedValue>"
                "         </allowedValueList>"
                "      </stateVariable>"
                "      <stateVariable sendEvents=\"no\">"
                "         <name>A_ARG_TYPE_Filter</name>"
                "         <dataType>string</dataType>"
                "      </stateVariable>"
                "      <stateVariable sendEvents=\"no\">"
                "         <name>A_ARG_TYPE_Index</name>"
                "         <dataType>ui4</dataType>"
                "      </stateVariable>"
                "      <stateVariable sendEvents=\"no\">"
                "         <name>A_ARG_TYPE_Count</name>"
                "         <dataType>ui4</dataType>"
                "      </stateVariable>"
                "      <stateVariable sendEvents=\"no\">"
                "         <name>A_ARG_TYPE_SortCriteria</name>"
                "         <dataType>string</dataType>"
                "      </stateVariable>"
                "      <stateVariable sendEvents=\"no\">"
                "         <name>A_ARG_TYPE_Result</name>"
                "         <dataType>string</dataType>"
                "      </stateVariable>"
                "      <stateVariable sendEvents=\"no\">"
                "         <name>A_ARG_TYPE_UpdateID</name>"
                "         <dataType>ui4</dataType>"
                "      </stateVariable>"
                "   </serviceStateTable>"
                "</scpd>";

                fwrite(cds,sizeof(cds)-1,1,fp);
        }
        else if(!strcmp(req,"/cds_control"))
        {
        }
        else if(!strcmp(req,"/cds_event"))
        {
        }
        else
        {
            fprintf(fp,
                "Content-Type: text/html\r\n\r\n"
                "<html>\n"
                "    <head>\n"
                "        <title>%s</title>\n"
                "    </head>\n"
                "    <body>\n"
                "        Device name: %s<br>\n"
                "        UUID: %s<br>\n"
                "        Multicast interface: %s<br>\n"
                "        TCP port: %i<br>\n"
                "        SSDP notify: %i sec<br>\n"
                "        <a href=\'/root.xml\'>Root Device Description</a><br>\n"
                "        <a href=\'/cds.xml\'>Content Directory Description</a><br>\n"
                "        <br>Other UPNP devices:<br>\n",
                device_name,device_name,device_uuid,mcast_grp.interface,http_port,upnp_notify_timeout);

            time_t t=time(0);

            for(int i=0;i<max_upnp_dev_num;i++)
                if(devices[i].sin.sin_addr.s_addr)
                {
                    if(*devices[i].location)
                        fprintf(fp,
                "        <a href=\'%s\'>%s:%i</a> (%lu sec)<br>\n",
                            devices[i].location,inet_ntoa(devices[i].sin.sin_addr),ntohs(devices[i].sin.sin_port),
                                t-devices[i].timestamp);
                    else
                        fprintf(fp,
                "        %s:%i (%lu sec)<br>\n",
                            inet_ntoa(devices[i].sin.sin_addr),ntohs(devices[i].sin.sin_port),t-devices[i].timestamp);
                }


            fprintf(fp,
                "    </body>\n"
                "</html>\n");
        }

        fflush(fp);

        if(content)
            free(content);

        alarm(http_timeout);


    }while(keep_alive);

    alarm(0);

    return 0;
}

