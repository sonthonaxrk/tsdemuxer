#include "mpls.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <memory>
#include <vector>


// Canon HF100 test only!

int mpls_parse(const char* filename,std::map<int,std::string>& dst)
{
    FILE* fp=fopen(filename,"rb");
    
    if(!fp)
	return -1;
	
    struct stat st;
    
    if(fstat(fileno(fp),&st)==-1)
    {
	fclose(fp);
	return -1;
    }

    unsigned int len=st.st_size;

    std::auto_ptr<unsigned char> p(new unsigned char[len]);
    
    unsigned char* ptr=p.get();
    
    if(!ptr)
    {
	fclose(fp);
	return -1;
    }

    if(fread(ptr,1,len,fp)!=len)
    {
	fclose(fp);
	return -1;
    }
    
    fclose(fp);
    
    if(memcmp(ptr,"MPLS0100",8))
	return -1;

    std::vector<int> chapters;
    chapters.reserve(512);

    int num=(((int)ptr[64])<<8)+ptr[65];

    unsigned char* pp=ptr+68;
    
    if(pp+num*82>ptr+len)
	return -1;
    
    for(int i=0;i<num;i++)
    {
	int chapter=0;
	
	for(int j=2;j<7;j++)
	    chapter=chapter*10+(pp[j]-48);
	
	chapters.push_back(chapter);

	pp+=82;
    }
    
    if(pp+6>ptr+len)
	return -1;
    
    num=(((int)pp[4])<<8)+pp[5];
    pp+=6;
    
    if(pp+num*14>ptr+len)
	return -1;

    for(int i=0;i<num;i++)
	pp+=14;

    pp+=352;    

    if(pp+2>ptr+len)
	return -1;

    num=(((int)pp[0])<<8)+pp[1];
    pp+=2;

    if(pp+num*66>ptr+len)
	return -1;

    for(int i=0;i<num;i++)
    {
	char tmp[64];
    
	sprintf(tmp,"20%.2x-%.2x-%.2x %.2x:%.2x:%.2x",pp[12],pp[13],pp[14],pp[15],pp[16],pp[17]);
	
	dst[chapters[i]]=tmp;
	
	pp+=66;
    }
    
    return 0;
}


/*
int main(void)
{
    std::map<int,std::string> dst;
    
    if(!mpls_parse("00000.mpl",dst))
	for(std::map<int,std::string>::iterator i=dst.begin();i!=dst.end();++i)
	    printf("%.5i: %s\n",i->first,i->second.c_str());

    
    return 0;
}
*/
