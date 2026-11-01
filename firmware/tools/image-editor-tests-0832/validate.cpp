#include "Tab5ElfValidate.h"
#include <fstream>
#include <vector>
#include <cstdio>
int main(int argc,char**argv){if(argc!=2)return 1;std::ifstream f(argv[1],std::ios::binary);if(!f)return 2;std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)),{});Tab5ElfValidate::Image image;const char* reason=nullptr;if(!Tab5ElfValidate::validate(bytes.data(),bytes.size(),image,&reason)){fprintf(stderr,"%s\n",reason?reason:"rejected");return 3;}if(image.stack!=48*1024)return 4;printf("{\"accepted\":true,\"imageBytes\":%u,\"entry\":%u,\"stackBytes\":%u}\n",image.bytes,image.entry,image.stack);}
