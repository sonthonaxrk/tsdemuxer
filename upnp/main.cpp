#include "upnp.h"
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>


namespace dlna
{
    struct list
    {
        char* buf;
        int len;
        list* next;
    };

    volatile int __sig_quit=0;
    volatile int __sig_alarm=0;

    int sock_up=-1;
    int sock_down=-1;

    FILE* verb_fp=0;

    static const char upnp_mgrp[]="239.255.255.250:1900";

    static const int upnp_notify_timeout=180;

    static const char device_name[]="Lolipop Media Server";

    char device_uuid[64]="";

    int http_port=4044;

    list* ssdp_alive_list=0;
    list* ssdp_byebye_list=0;
    list* ssdp_msearch_resp_list=0;

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

    list* add_to_list(list* lst,const char* s,int len);
    void free_list(list* lst);
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
    }

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


    const char* app_name=strrchr(argv[0],'/');
    if(app_name)
        app_name++;
    else
        app_name=argv[0];

    int opt;
    while((opt=getopt(argc,argv,"vli:u:t:p:h?"))>=0)
        switch(opt)
        {
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

    dlna::signal(SIGHUP ,SIG_IGN);
    dlna::signal(SIGPIPE,SIG_IGN);
    dlna::signal(SIGINT ,dlna::__sig_handler);
    dlna::signal(SIGQUIT,dlna::__sig_handler);
    dlna::signal(SIGTERM,dlna::__sig_handler);
    dlna::signal(SIGALRM,dlna::__sig_handler);

    dlna::mcast_grp.init(dlna::upnp_mgrp,mcast_iface,mcast_ttl,mcast_loop);

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
            dlna::init_ssdp();

            dlna::send_ssdp_alive_notify();

            alarm(dlna::upnp_notify_timeout);

            while(!dlna::__sig_quit)
            {
                char tmp[1024];

                sockaddr_in sin;

                int n=dlna::mcast_grp.recv(dlna::sock_down,tmp,sizeof(tmp)-1,&sin);

                if(n>0)
                {
                    tmp[n]=0;

//                    if(dlna::verb_fp)
//                        fprintf(dlna::verb_fp,"%s\n",tmp);

                    dlna::on_ssdp_message(tmp,n,&sin);

                }else if(n==-1)
                {
                    if(errno!=EINTR)
                        break;
                }else
                    break;

                if(dlna::__sig_alarm)
                {
                    dlna::__sig_alarm=0;
                    alarm(dlna::upnp_notify_timeout);

                    dlna::send_ssdp_alive_notify();
                }
            }

            dlna::send_ssdp_byebye_notify();

            dlna::mcast_grp.leave(dlna::sock_down);
            dlna::mcast_grp.close(dlna::sock_down);

            dlna::done_ssdp();
        }

        dlna::mcast_grp.close(dlna::sock_up);
    }

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
        "LOCATION: http://%s:%i/description\r\n"
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
        "LOCATION: http://%s:%i/description\r\n"
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
        "LOCATION: http://%s:%i/description\r\n"
        "NT: urn:schemas-upnp-org:service:ContentDirectory:1\r\n"
        "NTS: ssdp:alive\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:service:ContentDirectory:1\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=add_to_list(ll,tmp,n);

    n=snprintf(tmp,sizeof(tmp),
        "NOTIFY * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "LOCATION: http://%s:%i/description\r\n"
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
        "LOCATION: http://%s:%i/description\r\n"
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
        "LOCATION: http://%s:%i/description\r\n"
        "NT: urn:schemas-upnp-org:service:ContentDirectory:1\r\n"
        "NTS: ssdp:byebye\r\n"
        "Server: %s\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:service:ContentDirectory:1\r\n\r\n",
        mcast_grp.interface,http_port,device_name,device_uuid);

    ll=add_to_list(ll,tmp,n);


    char date[64];
    get_gmt_date(date,sizeof(date));

    n=snprintf(tmp,sizeof(tmp),
        "HTTP/1.1 200 OK\r\n"
        "CACHE-CONTROL: max-age=1800\r\n"
        "DATE: %s\r\n"
        "EXT:\r\n"
        "LOCATION: http://%s:%i/description\r\n"
        "Server: %s\r\n"
        "ST: urn:schemas-upnp-org:device:MediaServer:1\r\n"
        "USN: uuid:%s::urn:schemas-upnp-org:device:MediaServer:1\r\n\r\n",
        date,mcast_grp.interface,http_port,device_name,device_uuid);

    ll=0; ssdp_msearch_resp_list=ll=add_to_list(ll,tmp,n);

    return 0;
}

int dlna::done_ssdp(void)
{
    free_list(ssdp_alive_list);
    free_list(ssdp_byebye_list);
    free_list(ssdp_msearch_resp_list);
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
                                if(strcasecmp(p,"urn:schemas-upnp-org:device:MediaServer:1"))
                                    ignore=1;
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
        send_ssdp_msearch_response(sin);

    return 0;
}

int dlna::send_ssdp_msearch_response(sockaddr_in* sin)
{
    for(list* l=ssdp_msearch_resp_list;l;l=l->next)
        mcast_grp.send(sock_up,l->buf,l->len,sin);

    return 0;
}

