#include "tmpl.h"
#include <stdlib.h>
#include <string.h>

namespace tmpl
{
// TODO: Content-Type, Content-Length
    int validate_file_name(const char* src)
    {
        if(strstr(src,"/../") || strchr(src,'\\'))
            return -1;
        return 0;
    }

    int get_file_env(const char* src,FILE* dst)
    {
        if(validate_file_name(src))
            return -1;

        FILE* fp=fopen(src,"rb");
        if(!fp)
            return -1;

        int st=0;

        char var[64]="";
        int nvar=0;

        for(;;)
        {
            int ch=fgetc(fp);
            if(ch==EOF)
                break;

            switch(st)
            {
            case 0:
                if(ch=='%')
                    st=1;
                else
                    fputc(ch,dst);
                break;
            case 1:
                if(ch=='%')
                {
                    var[nvar]=0;
                    const char* p=getenv(var);
                    if(!p)
                        p="(null)";

                    fprintf(dst,"%s",p);
                    st=0;
                }else
                {
                    if(nvar<sizeof(var)-1)
                        var[nvar++]=ch;
                }
                break;
            }
        }

        fclose(fp);

        return 0;
    }

    int get_file(const char* src,FILE* dst)
    {
        if(validate_file_name(src))
            return -1;

        FILE* fp=fopen(src,"rb");
        if(!fp)
            return -1;

        char tmp[512];

        int n=0;

        while((n=fread(tmp,1,sizeof(tmp),fp))>0)
        {
            if(!fwrite(tmp,n,1,dst))
                break;
        }

        fclose(fp);

        return 0;
    }
}
