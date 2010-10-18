#ifndef __TMPL_H
#define __TMPL_H

#include <stdio.h>

namespace tmpl
{
    int get_file_env(const char* src,FILE* dst);
    int get_file(const char* src,FILE* dst);
}


#endif
