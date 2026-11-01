#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
extern int tab5_mp_run_with_argument(const char*,const char*,const char*,void*,size_t);
extern int tab5_mp_run(const char*,const char*,void*,size_t);
static char output[4096];static size_t used=0;static unsigned checks=0;
int tab5_mp_cancelled(void){return 0;}
void tab5_mp_yield(void){}
void tab5_mp_phase(const char* phase,const char* filename){(void)phase;(void)filename;}
void tab5_mp_output(const char* p,size_t n){if(n>sizeof(output)-used-1)abort();memcpy(output+used,p,n);used+=n;output[used]=0;}
int tab5_mp_io(int op,int a,int b,int c,const void* in,size_t len,void* out,size_t capacity){return -ENOSYS;}
static void check(int condition,const char* message){++checks;if(!condition){fprintf(stderr,"FAIL %s: %s\n",message,output);exit(1);}}
int main(void){
 void* heap=malloc(2*1024*1024);if(!heap)return 2;
 const char* path="/MicroPython/open_file.py";
 const char* argument="/USB/Documents/L'été 漢字;print(123).txt";
 used=0;output[0]=0;
 check(tab5_mp_run_with_argument("import sys\nprint(len(sys.argv))\nprint(sys.argv[0])\nprint(sys.argv[1])\n",path,argument,heap,2*1024*1024)==0,"VM argument launch succeeds");
 char expected[1024];snprintf(expected,sizeof(expected),"2\n%s\n%s\n",path,argument);
 check(strcmp(expected,output)==0,"UTF-8, apostrophes and punctuation remain one inert argv value");
 used=0;output[0]=0;
 check(tab5_mp_run("import sys\nprint(len(sys.argv))\nprint(sys.argv[0])\n",path,heap,2*1024*1024)==0,"ordinary VM launch succeeds");
 snprintf(expected,sizeof(expected),"1\n%s\n",path);check(!strcmp(expected,output),"ordinary argv contains script only; previous document cleared");
 used=0;output[0]=0;
 check(tab5_mp_run_with_argument("import sys\nprint(len(sys.argv))\n",path,"",heap,2*1024*1024)==0,"empty argument launch succeeds");
 check(!strcmp(output,"1\n"),"empty document omitted");
 free(heap);printf("{\"checks\":%u,\"passed\":true}\n",checks);return 0;
}
