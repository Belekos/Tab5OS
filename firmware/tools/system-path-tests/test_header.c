#include "Tab5SystemPath.h"
#include <assert.h>
#include <stdio.h>
int main(void){char path[64]="/M5Writer/A.txt";assert(tab5SystemPath(path,sizeof(path),path,strlen(path)));assert(!strcmp(path,"/System/A.txt"));assert(tab5SystemPythonPath(path,sizeof(path),"/sd/m5writer/./A.txt"));assert(!strcmp(path,"/sd/System/A.txt"));puts("PASS: production System helpers compile and execute as C17.");return 0;}
