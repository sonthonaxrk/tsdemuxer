#include "upnp.h"
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>

volatile int __sig_quit=0;
volatile int __sig_alarm=0;

int sock_up=-1;


void __sig_handler(int n)
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

int __signal(int num,void (*handler)(int))
{
    struct sigaction action;

    sigfillset(&action.sa_mask);

    action.sa_flags=(num==SIGCHLD)?SA_NOCLDSTOP:0;

    action.sa_handler=handler;

    return sigaction(num,&action,0);
}

int main(int argc,char** argv)
{
    static const char upnp_mgrp[] ="239.255.255.250";
    static const int upnp_mport   =1900;

    const char* mcast_if="";
    int verb=0;
    int mcast_ttl=0;

    char dev_uuid[64]="";

/*    upnp::if_info ifi;
    upnp::get_if_info("eth0",&ifi);
    printf("%s\n",ifi.if_addr);*/

/*    upnp::if_info ifi[16];
    int nif=upnp::get_if_list(ifi,16);
    printf("if_num=%i\n",nif);
    for(int i=0;i<nif;i++)
    {
        printf("%s, %s, ",ifi[i].if_name,ifi[i].if_addr);

        for(int j=0;j<32;j++,ifi[i].if_flags<<=1)
            printf(ifi[i].if_flags&0x80000000?"x":".");
        printf("\n");
    }*/

    int opt;
    while((opt=getopt(argc,argv,"i:vu:t:"))>=0)
        switch(opt)
        {
        case 'i':
            mcast_if=optarg;
            break;
        case 'v':
            verb=1;
            break;
        case 'u':
            snprintf(dev_uuid,sizeof(dev_uuid),"%s",optarg);
            break;
        case 't':
            mcast_ttl=atoi(optarg);
            break;
        }

    if(mcast_ttl<1)
        mcast_ttl=1;
    else if(mcast_ttl>4)
        mcast_ttl=4;

    __signal(SIGHUP ,SIG_IGN);
    __signal(SIGPIPE,SIG_IGN);
    __signal(SIGINT ,__sig_handler);
    __signal(SIGQUIT,__sig_handler);
    __signal(SIGTERM,__sig_handler);
    __signal(SIGALRM,__sig_handler);

    if(!*dev_uuid)
        upnp::uuid_gen(dev_uuid);

    if(verb)
        printf("device 'uuid:%s'\n",dev_uuid);


    int upnp_sock=upnp::mcast_join(upnp_mgrp,upnp_mport,mcast_if);

    if(upnp_sock==-1)
        perror("mcast_join");
    else
    {
        if(verb)
            printf("join to mcast group '%s:%i', interface '%s'\n",upnp_mgrp,upnp_mport,(*mcast_if)?mcast_if:"any");


        sock_up=upnp::mcast_upstream_socket(mcast_ttl,1);

        if(sock_up==-1)
            perror("mcast_upstream_socket");
        else
        {
            char local_addr[64]="";

            upnp::get_local_address(local_addr,sizeof(local_addr));

            int local_port=upnp::get_socket_local_port(sock_up);

            if(!local_port || !*local_addr)
                fprintf(stderr,"get_socket_local_port or get_local_address fail\n");
            else
            {
                if(verb)
                    printf("bind to '%s:%i'\n",local_addr,local_port);

                while(1)
                {
                    sockaddr_in src_sin;
                    socklen_t src_sin_len=sizeof(src_sin);

                    char buf[1024];

                    int n=recvfrom(upnp_sock,buf,sizeof(buf)-1,0,(sockaddr*)&src_sin,&src_sin_len);

                    if(n==-1 || !n)
                        break;

                    buf[n]=0;

                    if(verb)
                    {
                        printf("%i bytes received from %s:%i:\n",n,inet_ntoa(src_sin.sin_addr),ntohs(src_sin.sin_port));
                        printf("%s\n",buf);
                    }
                }
            }
            close(sock_up);
        }

        upnp::mcast_drop(upnp_sock,upnp_mgrp);

        if(verb)
            printf("leave mcast group '%s:%i'\n",upnp_mgrp,upnp_mport);
    }






/*    using namespace upnp;

    const char* cmd="";

    if(argc>1)
        cmd=argv[1];

    if(!strcmp(cmd,"send"))
    {
        int sock_up=mcast_upstream_socket(1,1);

        if(sock_up!=-1)
        {
            sockaddr_in sin;
            sin.sin_family=AF_INET;
            sin.sin_port=htons(1900);
            sin.sin_addr.s_addr=inet_addr("239.255.255.250");

            printf("%i\n",sendto(sock_up,"hello",5,0,(sockaddr*)&sin,sizeof(sin)));

            close(sock_up);
        }
    }else if(!strcmp(cmd,"recv"))
    {
        int sock=mcast_join("239.255.255.250",1900,"192.168.6.36");

        if(sock!=-1)
        {
            while(1)
            {
                sockaddr_in sin;
                socklen_t sin_len=sizeof(sin);

                char buf[1024];

                int n=recvfrom(sock,buf,sizeof(buf)-1,0,(sockaddr*)&sin,&sin_len);

                if(n==-1 || !n)
                    break;

                buf[n]=0;

                printf("%s:%i send %i bytes:\n",inet_ntoa(sin.sin_addr),ntohs(sin.sin_port),n);
                printf("%s\n\n",buf);
            }

            mcast_drop(sock,"239.255.255.250");
        }
    }

*/

    return 0;
}

