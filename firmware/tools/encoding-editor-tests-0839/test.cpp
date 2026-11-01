#include "WriterEncoding.h"
#include "WriterDocument.h"
#include "WriterStorage.h"
#include "WriterJson.h"
#include "WriterSyntax.h"
#include "WriterRich.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
using namespace writing;
static size_t checks=0;
static void check(bool pass,const char* why){++checks;if(!pass){fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static std::string snapshot(Document& d){char* p=d.snapshot();check(p!=nullptr,"snapshot allocation");std::string s(p,d.size());release(p);return s;}
static bool sink(const char* p,size_t n,void* ctx){static_cast<std::string*>(ctx)->append(p,n);return true;}
static bool decode(const std::string& s,TextEncoding requested,std::string& text,TextEncoding& actual,size_t chunk=1){TextDecoder d;d.reset(requested);text.clear();size_t p=0;do{size_t n=std::min(chunk,s.size()-p);if(!d.feed(s.data()+p,n,p+n==s.size(),sink,&text))return false;p+=n;}while(p<s.size());actual=d.encoding();return true;}
static bool encode(const std::string& s,TextEncoding enc,std::string& out,size_t chunk=7){TextEncoder e;e.reset(s.data(),s.size(),enc);out.clear();char b[4096];do{size_t n=0;if(!e.next(b,std::min(chunk,sizeof(b)),n))return false;check(n<=chunk,"encoder block bound");out.append(b,n);}while(!e.done());return true;}
struct MemoryFS:FileSystem{
 std::map<std::string,std::string> files;std::string active;size_t at=0,maxRead=0,maxWrite=0,reads=0,writes=0,opens=0,renames=0;int failRenameAt=0;bool readError=false,writeError=false,verifyError=false,openError=false;
 bool exists(const char* p)override{return files.count(p);}
 bool remove(const char* p)override{return files.erase(p);}
 bool rename(const char* a,const char* b)override{++renames;if(int(renames)==failRenameAt||!files.count(a))return false;files[b]=files[a];files.erase(a);return true;}
 bool openRead(const char* p)override{++opens;if(openError||!files.count(p))return false;active=p;at=0;return true;}
 bool openWrite(const char* p)override{++opens;if(openError)return false;active=p;at=0;files[p].clear();return true;}
 size_t length()override{return files[active].size();}
 size_t read(char* p,size_t n)override{++reads;maxRead=std::max(maxRead,n);if(readError)return 0;auto& s=files[active];n=std::min(n,s.size()-at);memcpy(p,s.data()+at,n);at+=n;if(n&&verifyError)p[0]^=1;return n;}
 size_t write(const char* p,size_t n)override{++writes;maxWrite=std::max(maxWrite,n);if(writeError)return 0;files[active].append(p,n);return n;}
 void close()override{active.clear();}
};
static void drain(SaveJob& s,MemoryFS& fs){size_t steps=0;while(s.busy()){size_t io=fs.reads+fs.writes;s.step();check(fs.reads+fs.writes-io<=1,"one data I/O per save step");check(++steps<10000,"save bounded completion");}}
static void codecs(){
 const std::string multilingual=u8"Bonjour Éléonore — 12 €\r\n日本語 / العربية / 😀\n";
 for(auto enc:{TextEncoding::Utf8,TextEncoding::Utf8Bom,TextEncoding::Utf16LE,TextEncoding::Utf16BE}){
  std::string encoded;check(encode(multilingual,enc,encoded),"unicode encode");
  for(size_t chunk=1;chunk<39;++chunk){std::string result;TextEncoding actual;check(decode(encoded,TextEncoding::Auto,result,actual,chunk),"decode with arbitrary boundaries");check(result==multilingual,"unicode roundtrip");check(actual==enc,"encoding detected and retained");}
 }
 const std::string western=u8"Œuvre d’été — déjà 25 € ; œ æ ÿ ™\r\n";std::string bytes,out;TextEncoding actual;
 check(encode(western,TextEncoding::Windows1252,bytes),"western encode");check(!decode(bytes,TextEncoding::Auto,out,actual),"no automatic ANSI fallback");check(decode(bytes,TextEncoding::Windows1252,out,actual),"explicit ANSI decode");check(out==western,"western roundtrip");
 check(!encode(multilingual,TextEncoding::Windows1252,bytes),"unrepresentable refused");
 for(unsigned c=1;c<256;++c){std::string b(1,char(c));bool undefined=c==0x81||c==0x8d||c==0x8f||c==0x90||c==0x9d;bool ok=decode(b,TextEncoding::Windows1252,out,actual);check(ok!=undefined,"CP1252 undefined bytes strict");if(ok){check(encode(out,TextEncoding::Windows1252,bytes),"CP1252 inverse");check(bytes==b,"all defined CP1252 bytes roundtrip");}}
 for(const std::string& invalid:std::vector<std::string>{std::string("\0",1),"\xc0\xaf","\xe0\x80\x80","\xed\xa0\x80","\xf4\x90\x80\x80","\xf5\x80\x80\x80","\xe2\x82","\x80"})check(!decode(invalid,TextEncoding::Utf8,out,actual),"invalid UTF8 rejected");
 for(const std::string& invalid:std::vector<std::string>{std::string("\xff\xfe\0",3),std::string("\xff\xfe\0\xd8",4),std::string("\xff\xfe\0\xdc",4),std::string("\xff\xfe\0\xd8x\0",6),std::string("\0\0\xfe\xff",4),std::string("\xff\xfe\0\0",4)})check(!decode(invalid,TextEncoding::Auto,out,actual),"invalid surrogate/truncated/UTF32 refused");
 for(auto enc:{TextEncoding::Utf8,TextEncoding::Utf8Bom,TextEncoding::Utf16LE,TextEncoding::Utf16BE,TextEncoding::Windows1252}){check(encode("",enc,bytes),"empty encode");check(decode(bytes,enc,out,actual),"empty decode");check(out.empty(),"empty retained");}
 check(decode("\xc3\xa9",TextEncoding::Utf8,out,actual)&&out==u8"é","valid UTF8 interpretation");check(decode("\xc3\xa9",TextEncoding::Windows1252,out,actual)&&out==u8"Ã©","explicit ambiguous byte interpretation");
}
static void transactions(){
 const std::string content=u8"À Paris — 45 €\r\n";
 for(auto enc:{TextEncoding::Utf8,TextEncoding::Utf8Bom,TextEncoding::Utf16LE,TextEncoding::Utf16BE,TextEncoding::Windows1252}){
  MemoryFS fs;std::string bytes;check(encode(content,enc,bytes,4096),"fixture encoded");fs.files["/test.txt"]=bytes;Document d;bool recovered=false;
  check(loadDocument(fs,"/test.txt",d,recovered,nullptr,enc),"load encoded doc");check(snapshot(d)==content,"load correct UTF8 model");check(d.encoding()==enc&&!d.dirty(),"loaded encoding clean");
  check(d.replace(d.size(),d.size(),u8"été",5,0,0),"edit encoded doc");SaveJob save(fs);check(save.begin(d,"/test.txt"),"save starts");drain(save,fs);check(save.state()==SaveJob::State::Done&&!d.dirty(),"encoded save committed clean");
  check(encode(content+u8"été",enc,bytes,4096)&&fs.files["/test.txt"]==bytes,"save preserves encoding and BOM");check(fs.maxRead<=4096&&fs.maxWrite<=4096,"bounded disk transfers");Document reloaded;check(loadDocument(fs,"/test.txt",reloaded,recovered,nullptr,enc),"reload saved doc");check(snapshot(reloaded)==content+u8"été","save/load text intact");
 }
 MemoryFS fs;fs.files["/keep.txt"]="original";Document d;check(d.assign(u8"🙂",4),"nonwestern document");d.setEncoding(TextEncoding::Windows1252);SaveJob failed(fs);check(failed.begin(d,"/keep.txt"),"preflight asynchronous");check(fs.opens==0,"no filesystem touched in save begin");drain(failed,fs);check(failed.state()==SaveJob::State::Error&&fs.files["/keep.txt"]=="original"&&fs.writes==0&&fs.opens==0,"unrepresentable save preserves original before any disk write");
 check(d.assign("replacement",11),"replacement document");d.setEncoding(TextEncoding::Utf16LE);SaveJob cancel(fs);check(cancel.begin(d,"/keep.txt"),"cancel start");cancel.cancel();check(fs.files["/keep.txt"]=="original"&&d.dirty(),"cancel preserves disk and dirty encoding");
 for(int failAt:{1,2}){MemoryFS f;f.files["/keep.txt"]="original";f.failRenameAt=failAt;SaveJob job(f);check(job.begin(d,"/keep.txt"),"rename error fixture");drain(job,f);check(job.state()==SaveJob::State::Error,"rename failure reported");check(f.files["/keep.txt"]=="original","rename rollback original intact");}
 {MemoryFS f;f.files["/keep.txt"]="original";f.verifyError=true;SaveJob job(f);check(job.begin(d,"/keep.txt"),"corruption fixture");drain(job,f);check(job.state()==SaveJob::State::Error&&f.files["/keep.txt"]=="original","readback checksum protects original");}
 {MemoryFS f;SaveJob job(f);check(job.begin(d,"/target.txt"),"concurrent encoding fixture");d.setEncoding(TextEncoding::Utf8);drain(job,f);check(job.state()==SaveJob::State::Done&&d.dirty(),"encoding changed during snapshot save stays dirty");}
 {MemoryFS f;f.files["/bad.txt"]="\xe2\x82";Document keep;check(keep.assign("keep",4),"load rollback fixture");bool recovered=false;CodecError error;check(!loadDocument(f,"/bad.txt",keep,recovered,nullptr,TextEncoding::Auto,&error),"invalid file refused");check(error==CodecError::InvalidSequence&&snapshot(keep)=="keep","invalid load keeps original model");}
 {MemoryFS f;f.files["/long.txt"]=std::string(2*Document::Limit+5,'x');Document keep;LoadJob job(f);check(!job.begin(keep,"/long.txt"),"raw input bound before read");check(f.reads==0,"oversize no read");}
 {MemoryFS f;f.files["/small.txt"]=std::string("\xff\xfe",2)+std::string("A\0B\0C\0",6);Document keep;LoadJob job(f);check(job.begin(keep,"/small.txt",2),"decoded limit fixture accepted raw bound");while(job.busy())job.step();check(job.state()==LoadJob::State::Error,"decoded limit prevents expansion");}
 {MemoryFS f;f.files["/.tab5-backups/lost.txt.writer.bak"]=std::string("\xff\xfe",2)+std::string("A\0",2);Document keep;bool recovered=false;check(loadDocument(f,"/lost.txt",keep,recovered)&&recovered&&snapshot(keep)=="A"&&keep.encoding()==TextEncoding::Utf16LE,"BOM backup recovery");}
 {MemoryFS f;std::string content(Document::Limit,'A');Document big;check(big.assign(content.data(),content.size()),"max document allocated");big.setEncoding(TextEncoding::Utf16LE);SaveJob job(f);check(job.begin(big,"/big.txt"),"large UTF16 save");drain(job,f);check(job.state()==SaveJob::State::Done&&f.files["/big.txt"].size()==Document::Limit*2+2,"UTF16 larger than UTF8 limit serialized");Document reloaded;bool recovered;check(loadDocument(f,"/big.txt",reloaded,recovered)&&reloaded.size()==Document::Limit,"maximum encoded file reloaded");}
}
static void jsonAndCode(){
 for(auto text:{"{}","[]","null","true","false","0","-0.25e+6"," {\"a\":[1,true,null,\"ok\"],\"é\":\"\\ud83d\\ude00\"} \n"}){Document d;check(d.assign(text,strlen(text)),"valid JSON text");check(validateJson(d).valid,"valid JSON accepted");}
 for(auto text:{"","{","[1,]","{\"a\":1,}","{'a':1}","01","1.","-.2","1e+","undefined","TRUE","true false","{\"a\" 1}","{1:2}","\"bad\\x\"","\"\\ud800\"","\"\\udc00\"","\"newline\n\"","/*comment*/{}"}){Document d;check(d.assign(text,strlen(text)),"invalid JSON fixture text");check(!validateJson(d).valid,"invalid JSON rejected");}
 {Document d;std::string deep(66,'[');deep+='0';deep+=std::string(66,']');check(d.assign(deep.data(),deep.size()),"deep JSON fixture");check(!validateJson(d).valid,"JSON depth bounded");}
 for(auto path:{"program.c","header.H","data.JSON","script.py"})check(sourceFile(path)&&editableFile(path),"C JSON Python file admission");
 RichText c;c.parse("unsigned int value = 3; // note\n",true,true);const std::string raw=c.markdown();colorSyntax(c,"program.c",false);check(c.markdown()==raw,"C coloring preserves source bytes");check(c.glyph(0).foreground!=NoColor&&c.glyph(9).foreground!=NoColor,"C types colored");
 RichText json;json.parse("{\"int\":true,\"x\":3}",true,true);auto rawJson=json.markdown();colorSyntax(json,"data.json",false);check(json.markdown()==rawJson,"JSON coloring preserves source bytes");
}
int main(){codecs();transactions();jsonAndCode();printf("{\"passed\":true,\"checks\":%zu}\n",checks);}
