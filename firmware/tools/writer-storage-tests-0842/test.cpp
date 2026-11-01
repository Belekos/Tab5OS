#include "WriterDocument.h"
#include "WriterStorage.h"
#include "WriterWorkspace.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
struct FS:writing::FileSystem{
 std::map<std::string,std::string> files;std::string active,statFailure;size_t at=0;bool unavailable=false,flushError=false;int failAt=-1,operations=0;
 bool ok(){++operations;if(unavailable)return false;return failAt<0||failAt--!=0;}
 bool exists(const char* p)override{bool present=false;probe(p,present);return present;}
 bool probe(const char* p,bool& present){present=false;if(!ok()||statFailure==p)return false;present=files.count(p);return true;}
 bool remove(const char* p)override{return ok()&&files.erase(p);}
 bool rename(const char* a,const char* b)override{if(!ok()||!files.count(a)||files.count(b))return false;files[b]=files[a];files.erase(a);return true;}
 bool openWrite(const char* p)override{if(!ok())return false;active=p;files[p].clear();at=0;return true;}
 bool openRead(const char* p)override{if(!ok()||!files.count(p))return false;active=p;at=0;return true;}
 size_t length()override{return files[active].size();}
 size_t write(const char* p,size_t n)override{if(!ok())return 0;files[active].append(p,n);return n;}
 size_t read(char* p,size_t n)override{if(!ok())return 0;const auto& s=files[active];n=std::min(n,s.size()-at);memcpy(p,s.data()+at,n);at+=n;return n;}
 void close()override{active.clear();}
 bool finishWrite(){close();return !flushError;}
};
#define REQUIRE(x) do{if(!(x)){std::cerr<<"fixture: "<<#x<<"\n";return 2;}}while(0)
std::string text(writing::Document& d){std::string s(d.size(),' ');d.copy(0,s.data(),s.size());return s;}
int main(int argc,char**){const bool baseline=argc>1;const char* primary="/System/editor_workspace.dat";FS fs;writing::Workspace ws;REQUIRE(ws.begin());REQUIRE(ws.add("/Documents/original.md")==0);REQUIRE(ws.save(fs));auto stored=fs.files;
 fs.unavailable=true;bool loaded=ws.load(fs);bool retained=ws.count()==1;fs.unavailable=false;ws.add("/Documents/new.md");bool saved=ws.save(fs);bool workspaceClosed=!loaded&&retained&&!saved&&fs.files==stored;
 fs.files=stored;fs.statFailure=primary;writing::Workspace unknown;bool known=unknown.load(fs);fs.statFailure.clear();unknown.add("/Documents/replacement.md");bool unknownSaved=unknown.save(fs);bool singleClosed=!known&&!unknownSaved&&fs.files==stored;
 fs.files.clear();fs.files["/document.md"]="current";fs.files["/.tab5-backups/document.md.writer.bak"]="older";fs.statFailure="/document.md";
 writing::Document document;REQUIRE(document.assign("unsaved",7));bool recovered=false;bool documentLoaded=writing::loadDocument(fs,"/document.md",document,recovered);bool documentClosed=!documentLoaded&&text(document)=="unsaved";
 fs.statFailure.clear();REQUIRE(writing::loadDocument(fs,"/document.md",document,recovered)&&text(document)=="current"&&!recovered);
 fs.files.erase("/document.md");REQUIRE(writing::loadDocument(fs,"/document.md",document,recovered)&&text(document)=="older"&&recovered);
 bool commitPreserved=true;unsigned faults=0;
 for(int n=0;n<80;++n){FS f;f.files=stored;writing::Workspace w;REQUIRE(w.load(f));w.add("/Documents/new.md");f.failAt=n;bool committed=w.save(f);f.failAt=-1;
   if(!committed){bool preserved=false;for(const auto& p:f.files)preserved|=p.second==stored[primary];commitPreserved&=preserved;}else {writing::Workspace again;commitPreserved&=again.load(f)&&again.count()==2;}++faults;}
 bool documentsPreserved=true;for(int n=0;n<40;++n){FS f;f.files["/document.md"]="original";writing::Document d;REQUIRE(d.assign("replacement",11));writing::SaveJob job(f);REQUIRE(job.begin(d,"/document.md"));f.failAt=n;while(job.busy())job.step();f.failAt=-1;bool success=job.state()==writing::SaveJob::State::Done;
  documentsPreserved&=success?f.files["/document.md"]=="replacement":f.files["/document.md"]=="original"||f.files["/.tab5-backups/document.md.writer.bak"]=="original";}
 FS flushFS;flushFS.files=stored;writing::Workspace fw;REQUIRE(fw.load(flushFS));fw.add("/Documents/new.md");flushFS.flushError=true;bool flushWorkspace=!fw.save(flushFS)&&flushFS.files[primary]==stored[primary];
 FS docFS;docFS.files["/document.md"]="original";docFS.flushError=true;writing::Document fd;REQUIRE(fd.assign("changed",7));writing::SaveJob fj(docFS);REQUIRE(fj.begin(fd,"/document.md"));while(fj.busy())fj.step();bool flushDocument=fj.state()==writing::SaveJob::State::Error&&docFS.files["/document.md"]=="original";
 bool all=flushWorkspace&&flushDocument&&workspaceClosed&&singleClosed&&documentClosed&&commitPreserved&&documentsPreserved;
 std::cout<<"{\"unavailableWorkspacePreserved\":"<<workspaceClosed<<",\"unknownWorkspaceCannotSaveEmpty\":"<<singleClosed<<",\"unknownPrimaryCannotLoadStaleBackup\":"<<documentClosed<<",\"workspaceCommitFaultsPreserved\":"<<commitPreserved<<",\"documentCommitFaultsPreserved\":"<<documentsPreserved<<",\"workspaceFaults\":"<<faults<<",\"workspaceFlushErrorPreserved\":"<<flushWorkspace<<",\"documentFlushErrorPreserved\":"<<flushDocument<<",\"documentFaults\":40,\"passed\":"<<all<<"}\n";return baseline||all?0:1;
}
