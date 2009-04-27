#ifndef __MPLS_H
#define __MPLS_H

#include <map>
#include <string>

int mpls_parse(const char* filename,std::map<int,std::string>& dst);

#endif
