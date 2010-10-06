#include "upnp.h"
#include <getopt.h>
#include <stdio.h>
#include <string.h>


int main(int argc,char** argv)
{
    upnp::mcast_grp grp("239.255.255.250:1900","eth0",1,1);
//    upnp::mcast_grp grp("239.255.255.250:1900","",1,1);

    const char* cmd="";

    if(argc>1)
        cmd=argv[1];

    if(!strcmp(cmd,"send"))
    {
        int sock_up=grp.upstream();

        if(sock_up!=-1)
        {
            grp.send(sock_up,"Hello",5);

            grp.close(sock_up);
        }
    }else if(!strcmp(cmd,"recv"))
    {
        int sock_down=grp.join();

        if(sock_down!=-1)
        {
            while(1)
            {
                char tmp[256];
                int n=grp.recv(sock_down,tmp,sizeof(tmp)-1);

                if(!n)
                    break;

                tmp[n]=0;
                printf("%s\n",tmp);
            }


            grp.leave(sock_down);
            grp.close(sock_down);
        }
    }

    return 0;
}

