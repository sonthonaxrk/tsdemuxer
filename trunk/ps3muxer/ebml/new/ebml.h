#ifndef __EBML_H
#define __EBML_H

#include <stdexcept>
#include <string>
#include <stdio.h>
#include <sys/types.h>

namespace ebml
{
    class exception : public std::exception
    {
    protected:
        std::string _what;
    public:
        explicit exception(const std::string& s):_what(s) {}
        virtual ~exception(void) throw() {}
        virtual const char* what(void) const throw() {return _what.c_str();}
    };


    class file
    {
    protected:
        FILE* fp;
        u_int32_t cluster_timecode;
    public:
        file(void):fp(0),cluster_timecode(0) {}
        ~file(void) { close(); }

        int open(const char* filename) throw();
        void close(void) throw();

        u_int64_t tell(void) throw(std::exception);
        void seek(u_int64_t offset,int whence) throw(std::exception);
        u_int32_t gettag(void) throw(std::exception);          // 0 - eof
        u_int64_t getlen(void) throw(std::exception);
        std::string getstring(int len) throw(std::exception);
        u_int32_t getint(int len) throw(std::exception);

        int parse(int depth=0) throw(std::exception);
    };
}


#endif
