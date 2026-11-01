#include "Tab5AppApi.h"
static size_t length(const char* s){size_t n=0;while(s[n])++n;return n;}
static void say(const struct tab5_app_api* a,const char* s){a->log(s,length(s));}
int tab5_app_main(int argc,char** argv){
 const struct tab5_app_api* a=tab5_app_api_v1();
 if(!a||a->version!=TAB5_APP_ABI_V1||a->size<sizeof(*a))return 1;
 int fr=a->language[0]=='f';
 say(a,fr?"Empreinte FNV-1a 32 bits (non cryptographique)\n":"FNV-1a 32-bit checksum (not cryptographic)\n");
 if(argc<2||!argv[1][0]){say(a,fr?"Indiquez le chemin SD du fichier en argument.\n":"Enter the file's SD path as the argument.\n");return 2;}
 int fd=a->open_read(argv[1]);if(fd<0){say(a,fr?"Ouverture impossible.\n":"Cannot open file.\n");return 3;}
 unsigned char* data=a->allocate(4096);if(!data){a->close(fd);say(a,fr?"Mémoire insuffisante.\n":"Not enough memory.\n");return 4;}
 uint32_t hash=2166136261u,blocks=0;int n;
 while(!a->should_stop()&&(n=a->read(fd,data,4096))>0){
  for(int i=0;i<n;++i){hash^=data[i];hash*=16777619u;}
  if((++blocks&15)==0)say(a,".");
  if(a->sleep_ms(1))break;
 }
 a->release(data);a->close(fd);
 if(a->should_stop()){say(a,fr?"\nArrêt demandé.\n":"\nStop requested.\n");return 0;}
 if(n<0){say(a,fr?"\nLecture impossible.\n":"\nRead failed.\n");return 5;}
 char text[11];text[0]='\n';for(unsigned i=0;i<8;++i)text[1+i]="0123456789abcdef"[(hash>>(28-4*i))&15];text[9]='\n';text[10]=0;
 say(a,text);return 0;
}
