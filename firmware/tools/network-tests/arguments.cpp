#include "Tab5NetworkArgs.h"
#include "Tab5TerminalPath.h"
#include <cassert>
#include <cstdio>
using namespace Tab5NetworkArgs;
int main(){
 Options o;assert(!parse({"other"},o));
 assert(parse({"ping","-c","3","-W","0.5","example.org"},o)&&o.error.empty()&&o.count==3&&o.timeoutMs==500);
 for(const char* n:{"0","101","-1","NaN","1e9",""})assert(parse({"ping","-c",n,"host"},o)&&!o.error.empty());
 for(const char* n:{"NaN","inf","-1","0","31"})assert(parse({"tcp","-w",n,"host","80"},o)&&!o.error.empty());
 assert(parse({"nc","-z","-w","0.35","host","443"},o)&&o.error.empty()&&o.timeoutMs==350&&o.ports[0]==443);
 assert(parse({"ports","host","22,80,443,8000-8010,22"},o)&&o.error.empty()&&o.ports.size()==14);
 for(const char* ports:{"","0","65536","4-2","1-1025","80,","1,,2","-2"})assert(parse({"ports","host",ports},o)&&!o.error.empty());
 assert(parse({"curl","-O","https://example.org/path/file.txt?x=1"},o)&&o.error.empty()&&o.destination=="file.txt");
 assert(parse({"curl","-O","-o","file","https://example.org"},o)&&!o.error.empty());
 assert(parse({"wget","-O","usb:/a.txt","-T","3600","https://example.org"},o)&&o.error.empty()&&o.timeoutMs==3600000&&o.destination=="usb:/a.txt");
 assert(parse({"headers","https://example.org"},o)&&o.error.empty()&&o.head&&o.timeoutMs==120000);
 for(const char* url:{"ftp://host/file","https://user:secret@host/","https://host:0/","https://host:65536/","https:///","https://host\nHeader:X","https://[::1]/"})assert(!httpUrl(url));
 assert(normalizeUrl("https://example.org?x=1#section")=="https://example.org/?x=1");
 assert(normalizeUrl("https://example.org/a//b/../c#x")=="https://example.org/a//c");
 assert(resolveUrl("https://example.org/releases/file.bin","?download=1")=="https://example.org/releases/file.bin?download=1");
 assert(resolveUrl("https://example.org?old=1","/next")=="https://example.org/next");
 assert(resolveUrl("https://example.org/releases/file.bin?old=1","#section")=="https://example.org/releases/file.bin?old=1");
 assert(resolveUrl("https://example.org/releases/file.bin?old=1","../next?x=1#section")=="https://example.org/next?x=1");
 assert(resolveUrl("https://example.org/releases/file.bin","//cdn.example.org?x=1")=="https://cdn.example.org/?x=1");
 assert(resolveUrl("https://example.org/file","mailto:test@example.org").empty());
 assert(resolveUrl("https://example.org/file","").empty());
 std::string path;
 assert(Tab5TerminalPath::logical("usb:/a/../File.txt","/",path,true)&&path=="/USB/File.txt");
 assert(Tab5TerminalPath::logical("/USB/../File.txt","/",path,true)&&path=="/File.txt");
 assert(Tab5TerminalPath::logical("sd:/Folder/file.txt","/USB",path,true)&&path=="/Folder/file.txt");
 assert(Tab5TerminalPath::logical("../b","/USB/folder",path,true)&&path=="/USB/b");
 assert(!Tab5TerminalPath::logical("usb:/","/",path,true));
 assert(!Tab5TerminalPath::logical("../../escape","/",path,true));
 puts("PASS: command options, bounded scans, URL validation, standard curl -O, long transfers, normalized SD/USB paths.");
}
