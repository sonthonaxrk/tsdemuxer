#include "tmpl.h"
#include <stdlib.h>
#include <string.h>

namespace tmpl
{
    int validate_file_name(const char* src)
    {
        if(strstr(src,"/../") || strchr(src,'\\'))
            return -1;
        return 0;
    }

    const char* get_mime_type(const char* src)
    {
        const char* p=strrchr(src,'.');

        if(p)
        {
            p++;

            if(!strcasecmp(p,"xml")) return "text/xml";
            else if(!strcasecmp(p,"html") || !strcasecmp(p,"htm")) return "text/html";
            else if(!strcasecmp(p,"txt")) return "text/plain";
            else if(!strcasecmp(p,"jpeg") || !strcasecmp(p,"jpg")) return "image/jpeg";
            else if(!strcasecmp(p,"png")) return "image/png";
        }

        return "application/x-octet-stream";
    }

    int get_file_env(FILE* src,FILE* dst)
    {
        int st=0;

        char var[64]="";
        int nvar=0;

        for(;;)
        {
            int ch=fgetc(src);
            if(ch==EOF)
                break;

            switch(st)
            {
            case 0:
                if(ch=='#')
                    st=1;
                else
                    fputc(ch,dst);
                break;
            case 1:
                if(ch=='#')
                {
                    var[nvar]=0;
                    const char* p=getenv(var);
                    if(!p)
                        p="(null)";

                    fprintf(dst,"%s",p);
                    nvar=0;
                    st=0;
                }else
                {
                    if(nvar<sizeof(var)-1)
                        var[nvar++]=ch;
                }
                break;
            }
        }

        return 0;
    }

    int get_file_plain(FILE* src,FILE* dst)
    {
        char tmp[512];

        int n=0;

        while((n=fread(tmp,1,sizeof(tmp),src))>0)
        {
            if(!fwrite(tmp,n,1,dst))
                break;
        }


        return 0;
    }


    int get_file(const char* filename,FILE* dst,int tmpl,const char* date,const char* device_name)
    {
        while(*filename && *filename=='/')
            filename++;

        FILE* fp=0;

        if(validate_file_name(filename) || !(fp=fopen(filename,"rb")))
        {
            fprintf(dst,"HTTP/1.1 404 Not found\r\nPragma: no-cache\r\nDate: %s\r\nServer: %s\r\nConnection: close\r\n\r\n",date,device_name);
            return -1;
        }

        fprintf(dst,"HTTP/1.1 200 Ok\r\nPragma: no-cache\r\nDate: %s\r\nServer: %s\r\nContent-Type: %s\r\nConnection: close\r\n\r\n",
            date,get_mime_type(filename),device_name);

        int rc;

        if(tmpl)
            rc=get_file_env(fp,dst);
        else
            rc=get_file_plain(fp,dst);

        fclose(fp);
        
        return rc;
    }

                
}
