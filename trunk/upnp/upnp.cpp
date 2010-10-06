#include "upnp.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

namespace upnp
{
    FILE* verb_fp=stderr;

    void trace(const char* fmt,...)
    {
        if(!verb_fp)
            return;

        va_list ap;
        va_start(ap,fmt);
        vfprintf(verb_fp,fmt,ap);
        va_end(ap);
    }
}

void upnp::uuid_gen(char* dst)
{
    uuid_t uuid;
    uuid_generate(uuid);

    uuid_unparse_lower(uuid,dst);
}

int upnp::get_if_info(const char* if_name,if_info* ifi)
{
    *ifi->if_name=0;
    ifi->if_flags=0;
    *ifi->if_addr=0;
    memset((char*)&ifi->if_sin,0,sizeof(sockaddr_in));

    int s=socket(PF_INET,SOCK_DGRAM,0);

    if(s!=-1)
    {
        ifreq ifr;
        snprintf(ifr.ifr_ifrn.ifrn_name,IFNAMSIZ,"%s",if_name);

        snprintf(ifi->if_name,IF_NAME_LEN,"%s",if_name);

        if(ioctl(s,SIOCGIFADDR,&ifr)!=-1)
        {
            if(ifr.ifr_addr.sa_family==AF_INET)
            {
                ifi->if_sin.sin_family=ifr.ifr_addr.sa_family;
                ifi->if_sin.sin_addr=((sockaddr_in*)&ifr.ifr_addr)->sin_addr;
                ifi->if_sin.sin_port=0;
                snprintf(ifi->if_addr,IF_ADDR_LEN,"%s",inet_ntoa(ifi->if_sin.sin_addr));
            }
        }

        if(ioctl(s,SIOCGIFFLAGS,&ifr)!=-1)
        {
            if(ifr.ifr_flags&IFF_UP)            ifi->if_flags|=IF_UP;
            if(ifr.ifr_flags&IFF_BROADCAST)     ifi->if_flags|=IF_BROADCAST;
            if(ifr.ifr_flags&IFF_LOOPBACK)      ifi->if_flags|=IF_LOOPBACK;
            if(ifr.ifr_flags&IFF_POINTOPOINT)   ifi->if_flags|=IF_POINTOPOINT;
            if(ifr.ifr_flags&IFF_RUNNING)       ifi->if_flags|=IF_RUNNING;
            if(ifr.ifr_flags&IFF_MULTICAST)     ifi->if_flags|=IF_MULTICAST;
        }

        close(s);
    }

    return *ifi->if_addr?0:-1;
}

int upnp::get_if_list(if_info* ifi,int nifi)
{
    int s=socket(PF_INET,SOCK_DGRAM,0);

    int n=0;

    if(s!=-1)
    {
        int l=sizeof(ifreq)*nifi;

        ifreq* ifr=(ifreq*)malloc(l);

        if(ifi)
        {
            ifconf ifc;

            memset((char*)&ifc,0,sizeof(ifc));

            ifc.ifc_buf=(char*)ifr;
            ifc.ifc_len=l;

            if(ioctl(s,SIOCGIFCONF,&ifc)!=-1)
            {
                n=ifc.ifc_len/sizeof(ifreq);
                if(n>nifi)
                    n=nifi;

                for(int i=0;i<n;i++)
                    get_if_info(ifr[i].ifr_ifrn.ifrn_name,ifi+i);
            }

            free(ifr);
        }

        close(s);
    }

    return n;
}

in_addr upnp::get_best_mcast_if_addr(void)
{
    upnp::if_info ifi[16];

    for(int i=0;i<upnp::get_if_list(ifi,sizeof(ifi)/sizeof(*ifi));i++)
    {
        if(!(ifi[i].if_flags&IF_LOOPBACK) && ifi[i].if_flags&IF_UP && ifi[i].if_flags&IF_MULTICAST)
            return ifi[i].if_sin.sin_addr;
    }

    in_addr tt;
    tt.s_addr=INADDR_ANY;
    return tt;
}

int upnp::get_socket_port(int s)
{
    sockaddr_in sin;
    socklen_t sin_len=sizeof(sin);

    if(getsockname(s,(sockaddr*)&sin,&sin_len))
        return 0;

    return ntohs(sin.sin_port);
}


upnp::mcast_grp::mcast_grp(const char* addr,const char* iface,int ttl,int loop)
{
    mcast_ttl=ttl;
    mcast_loop=loop;

    mcast_sin.sin_family=AF_INET;
    mcast_sin.sin_port=0;
    mcast_sin.sin_addr.s_addr=INADDR_ANY;

    mcast_if_sin.sin_family=AF_INET;
    mcast_if_sin.sin_port=0;
    mcast_if_sin.sin_addr.s_addr=INADDR_ANY;

    char tmp[256];
    strcpy(tmp,addr);

    if(mcast_ttl<1)
        mcast_ttl=1;
    else if(mcast_ttl>4)
        mcast_ttl=4;

    char* port=strchr(tmp,':');

    if(port)
    {
        *port=0;
        mcast_sin.sin_port=htons(atoi(port+1));
    }

    mcast_sin.sin_addr.s_addr=inet_addr(tmp);

    int if_by_name=0;
    for(const char* p=iface;*p;p++)
        if(isalpha(*p))
            { if_by_name=1; break; }

    if(iface && *iface)
    {
        if(if_by_name)
        {
            trace("find multicast interface address by name '%s'\n",iface);

            if_info ifi;
            get_if_info(iface,&ifi);
            memcpy((char*)&mcast_if_sin,&ifi.if_sin,sizeof(sockaddr_in));
        }else
            mcast_if_sin.sin_addr.s_addr=inet_addr(iface);
    }else
    {
        trace("find multicast default interface address\n");
        mcast_if_sin.sin_addr=get_best_mcast_if_addr();
    }

    if(verb_fp)
    {
        trace("multicast interface address: '%s'\n",mcast_if_sin.sin_addr.s_addr==INADDR_ANY?"any":inet_ntoa(mcast_if_sin.sin_addr));
        trace("multicast group address: '%s:%i'\n",inet_ntoa(mcast_sin.sin_addr),ntohs(mcast_sin.sin_port));
    }
}

int upnp::mcast_grp::join(void) const
{
    int sock=socket(PF_INET,SOCK_DGRAM,0);

    if(sock!=-1)
    {
        sockaddr_in sin;

        sin.sin_family=AF_INET;
        sin.sin_port=mcast_sin.sin_port;
        sin.sin_addr.s_addr=INADDR_ANY;

        int reuse=1;
        setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));

        if(!bind(sock,(sockaddr*)&sin,sizeof(sin)))
        {
            ip_mreq mcast_group;
            memset((char*)&mcast_group,0,sizeof(mcast_group));
            mcast_group.imr_multiaddr.s_addr=mcast_sin.sin_addr.s_addr;
            mcast_group.imr_interface.s_addr=mcast_if_sin.sin_addr.s_addr;

            if(!setsockopt(sock,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mcast_group,sizeof(mcast_group)))
            {
                if(verb_fp)
                {
                    trace("join to multicast group '%s:%i' on ",inet_ntoa(mcast_group.imr_multiaddr),ntohs(sin.sin_port));
                    trace("interface '%s'\n",mcast_group.imr_interface.s_addr==INADDR_ANY?"any":inet_ntoa(mcast_group.imr_interface));
                }

                return sock;
            }

            close(sock);
        }
        close(sock);
    }

    return -1;
}

int upnp::mcast_grp::leave(int sock) const
{
    ip_mreq mcast_group;
    memset((char*)&mcast_group,0,sizeof(mcast_group));
    mcast_group.imr_multiaddr.s_addr=mcast_sin.sin_addr.s_addr;
    mcast_group.imr_interface.s_addr=mcast_if_sin.sin_addr.s_addr;

    if(!setsockopt(sock,IPPROTO_IP,IP_DROP_MEMBERSHIP,&mcast_group,sizeof(mcast_group)))
    {
        if(verb_fp)
        {
            trace("leave multicast group '%s' on ",inet_ntoa(mcast_group.imr_multiaddr));
            trace("interface '%s'\n",mcast_group.imr_interface.s_addr==INADDR_ANY?"any":inet_ntoa(mcast_group.imr_interface));
        }
    }

    close(sock);

    return 0;
}

int upnp::mcast_grp::upstream(void) const
{
    int sock=socket(PF_INET,SOCK_DGRAM,0);

    if(sock!=-1)
    {
        setsockopt(sock,IPPROTO_IP,IP_MULTICAST_TTL,&mcast_ttl,sizeof(mcast_ttl));
        setsockopt(sock,IPPROTO_IP,IP_MULTICAST_LOOP,&mcast_loop,sizeof(mcast_loop));
        setsockopt(sock,IPPROTO_IP,IP_MULTICAST_IF,&mcast_if_sin.sin_addr,sizeof(in_addr));

        sockaddr_in sin;
        sin.sin_family=AF_INET;
        sin.sin_addr.s_addr=mcast_if_sin.sin_addr.s_addr;
        sin.sin_port=0;
        bind(sock,(sockaddr*)&sin,sizeof(sin));

        if(verb_fp)
        {
            trace("multicast upstream address: '%s:%i'\n",inet_ntoa(sin.sin_addr),get_socket_port(sock));
            trace("multicast upstream ttl: %i\n",mcast_ttl);
        }

        return sock;
    }

    return -1;
}

void upnp::mcast_grp::close(int sock)
{
    ::close(sock);
}

int upnp::mcast_grp::send(int sock,const char* buf,int len) const
{
    int n=sendto(sock,buf,len,0,(sockaddr*)&mcast_sin,sizeof(mcast_sin));

    if(n>0 && verb_fp)
    {
        trace("send %i bytes to multicast group '%s:%i' via ",n,inet_ntoa(mcast_sin.sin_addr),ntohs(mcast_sin.sin_port));
        trace("interface '%s'\n",mcast_if_sin.sin_addr.s_addr==INADDR_ANY?"any":inet_ntoa(mcast_if_sin.sin_addr));
    }

    return n>0?n:0;
}

int upnp::mcast_grp::recv(int sock,char* buf,int len) const
{
   sockaddr_in sin;
   socklen_t sin_len=sizeof(sin);

   int n=recvfrom(sock,buf,len,0,(sockaddr*)&sin,&sin_len);

    if(n>0 && verb_fp)
        trace("recv %i bytes from '%s:%i'\n",n,inet_ntoa(sin.sin_addr),ntohs(sin.sin_port));

    return n>0?n:0;
}
