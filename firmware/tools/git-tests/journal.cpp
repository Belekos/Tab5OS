#include "Tab5GitJournal.h"
#include <map>
#include <iostream>
#include <stdexcept>
#include <cassert>
using namespace Tab5GitJournal;
struct Disk {std::map<std::string,std::string> files;int event=0,cut=-1,fail=-1;
 bool boundary(){++event;if(event==cut)throw std::runtime_error("power failure");return event!=fail;}
 Ops ops(){return {
  [&](const std::string& p){return files.count(p)?1:0;},
  [&](const std::string& a,const std::string& b){if(!boundary()||!files.count(a)||files.count(b))return false;files[b]=files[a];files.erase(a);return true;},
  [&](const std::string& p){if(!boundary())return false;return files.erase(p)!=0;},
  [&](const std::string& p,const void* d,size_t n){if(!boundary()||files.count(p))return false;files[p]=std::string(static_cast<const char*>(d),n);return true;},
  [&](const std::string& p,void* d,size_t n){if(!files.count(p)||files[p].size()!=n)return false;memcpy(d,files[p].data(),n);return true;}
 };}
};
int main(){unsigned checks=0;auto check=[&](bool b){++checks;if(!b){std::cerr<<"failed "<<checks<<"\n";exit(1);}};
 for(const std::string target:{"/repo/.git/index","/repo/.git/refs/heads/main","/repo/.git/config"}){
  for(int cut=1;cut<=7;++cut){Disk d;d.files[target]="OLD";d.files[target+".lock"]="NEW";d.cut=cut;
   try{replace("/repo",target+".lock",target,d.ops());}catch(const std::runtime_error&){}
   const bool logged=d.files.count(journal("/repo"));d.cut=-1;check(recover("/repo",d.ops()));check(d.files[target]=="OLD"||d.files[target]=="NEW");check(!d.files.count(journal("/repo")));check(!d.files.count(backup(target)));if(logged)check(!d.files.count(target+".lock"));
  }
  for(int failure=1;failure<=5;++failure){Disk d;d.files[target]="OLD";d.files[target+".lock"]="NEW";d.fail=failure;
   check(!replace("/repo",target+".lock",target,d.ops()));d.fail=-1;check(recover("/repo",d.ops()));check(d.files[target]=="OLD"||d.files[target]=="NEW");}
 }
 {Disk d;d.files["/outside"]="KEEP";d.files["/repo/new"]="NEW";check(!replace("/repo","/repo/new","/outside",d.ops()));check(d.files["/outside"]=="KEEP");}
 {Disk d;Record r;strcpy(r.from,"/repo/.git/index.lock");strcpy(r.to,"/outside");r.checksum=checksum(r);d.files[journal("/repo")]=std::string((char*)&r,sizeof(r));check(!recover("/repo",d.ops()));}
 {Disk d;d.files[journal("/repo")]="torn write";check(!recover("/repo",d.ops()));}
 std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"scope\":\"FAT semantics; power loss before each write/rename/remove and I/O error return; previous or complete new metadata preserved\"}\n";
}
