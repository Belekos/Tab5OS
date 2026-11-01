#include "WriterSyntax.h"
#include "WriterSyntaxIndex.h"
#include "WriterRich.h"
#include "WriterWorkspace.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
using namespace writing;
static unsigned checks=0;static bool failNext=false;static size_t allocations=0;
void* heap_caps_malloc(size_t n,unsigned caps){if(caps!=3)abort();if(failNext){failNext=false;return nullptr;}void* p=malloc(n);if(p)++allocations;return p;}
void heap_caps_free(void*p){if(p)--allocations;free(p);}
static void check(bool v,const char* why){++checks;if(!v){fprintf(stderr,"FAIL %u: %s\n",checks,why);exit(1);}}
using L=SyntaxLanguage;using T=SyntaxToken;
static void token(const std::string& source,L language,const std::string& word,T expected,bool dark=true,size_t occurrence=0){
 RichText r;r.parse(source,true,true);const auto before=r.markdown();colorSyntax(r,"file",dark,language);check(r.markdown()==before,"coloring preserves UTF8 source and raw syntax");size_t p=0;for(size_t n=0;n<=occurrence;++n){p=source.find(word,p);check(p!=std::string::npos,"expected token exists in fixture");if(n<occurrence)++p;}
 const uint32_t found=r.glyph(r.indexAtSource(p)).foreground;
 if(found!=syntaxColor(expected,dark))fprintf(stderr,"source=%s\nword=%s lang=%s expected=%x found=%x\n",source.c_str(),word.c_str(),syntaxLanguageName(language),syntaxColor(expected,dark),found);
 check(found==syntaxColor(expected,dark),"token uses language-specific category");
}
static void languages(){
 for(bool dark:{false,true}){
  const std::string c="#include <stdint.h>\n#define CAPACITY 16\nconst uint32_t value = 0x2Au;\nint main(void) { printf(\"é 日本語 😀\"); return 0; } // class def True\ndef None class function null\n";
  token(c,L::C,"include",T::Directive,dark);token(c,L::C,"stdint.h",T::String,dark);token(c,L::C,"CAPACITY",T::Directive,dark);token(c,L::C,"const",T::Keyword,dark);token(c,L::C,"uint32_t",T::Type,dark);token(c,L::C,"0x2A",T::Number,dark);token(c,L::C,"main",T::Function,dark);token(c,L::C,"printf",T::Builtin,dark);token(c,L::C,"é",T::String,dark);token(c,L::C,"//",T::Comment,dark);token(c,L::C,"None",T::Plain,dark);token(c,L::C,"function",T::Plain,dark);token(c,L::C,"null",T::Plain,dark);
  const std::string py="@functools.cache\nasync def greet(value: str = 'é') -> None:\n    print(len(value))\n    return True\n# uint32_t printf\nconst uint32_t null function\nmessage = f\"héllo\"\n";
  token(py,L::Python,"@functools.cache",T::Decorator,dark);token(py,L::Python,"async",T::Keyword,dark);token(py,L::Python,"def",T::Keyword,dark);token(py,L::Python,"greet",T::Function,dark);token(py,L::Python,"str",T::Type,dark);token(py,L::Python,"print",T::Builtin,dark);token(py,L::Python,"len",T::Builtin,dark);token(py,L::Python,"None",T::Keyword,dark);token(py,L::Python,"#",T::Comment,dark);token(py,L::Python,"const",T::Plain,dark);token(py,L::Python,"function",T::Plain,dark);token(py,L::Python,"null",T::Plain,dark);token(py,L::Python,"f\"",T::String,dark);
  const std::string json="{\"name\":\"café\", \"enabled\":true, \"count\":-12.5e+2, \"empty\":null}\nint class def printf None undefined # comment // comment 'invalid'\n";
  token(json,L::Json,"name",T::Property,dark);token(json,L::Json,"café",T::String,dark);token(json,L::Json,"true",T::Keyword,dark);token(json,L::Json,"-12.5",T::Number,dark);token(json,L::Json,"null",T::Keyword,dark);token(json,L::Json,"int",T::Plain,dark);token(json,L::Json,"class",T::Plain,dark);token(json,L::Json,"def",T::Plain,dark);token(json,L::Json,"None",T::Plain,dark);token(json,L::Json,"#",T::Plain,dark);token(json,L::Json,"invalid",T::Plain,dark);token("// note\n{}",L::JsonComments,"note",T::Comment,dark);
 }
 token("class vector std uint32_t",L::Cpp,"class",T::Keyword);token("class vector std uint32_t",L::Cpp,"vector",T::Type);
 token("const result = JSON.parse(value); interface type None",L::JavaScript,"const",T::Keyword);token("interface type None",L::JavaScript,"interface",T::Plain);token("interface type None",L::TypeScript,"interface",T::Keyword);
 token("select count(*) from table_name; -- note",L::Sql,"select",T::Keyword);token("print int None null def",L::Sql,"def",T::Plain);
 token("body { color: #aaccee; /* note */ }",L::Css,"color",T::Property);token("body { color: #aaccee; /* note */ }",L::Css,"note",T::Comment);
 token("<section name=\"café\">class def return</section>",L::Markup,"section",T::Tag);token("<section name=\"café\">class def return</section>",L::Markup,"name",T::Attribute);token("<section name=\"café\">class def return</section>",L::Markup,"class",T::Plain);
 token("title: true # note",L::Yaml,"title",T::Property);token("title = true # note",L::Toml,"title",T::Property);token("path = /tmp # note",L::Ini,"path",T::Property);
 token("if [ -f file ]; then echo $HOME; fi",L::Shell,"then",T::Keyword);token("if [ -f file ]; then echo $HOME; fi",L::Shell,"$HOME",T::Builtin);
 token("local x = nil -- note",L::Lua,"local",T::Keyword);token("fn main() { let x: u32 = 3; }",L::Rust,"fn",T::Keyword);token("fn main() { let x: u32 = 3; }",L::Rust,"u32",T::Type);token("func main() { var x string }",L::Go,"func",T::Keyword);
 token("public class Main { int x; }",L::Java,"public",T::Keyword);token("fun test(value: String) {}",L::Kotlin,"fun",T::Keyword);token("func test(value: String) {}",L::Swift,"func",T::Keyword);token("def test\n nil\nend",L::Ruby,"def",T::Keyword);token("function test() { return null; }",L::Php,"function",T::Keyword);token("function test { $value = $true }",L::PowerShell,"function",T::Keyword);token("project(test)",L::CMake,"project",T::Builtin);token("target: source\n\t$(CC) test.c",L::Makefile,"target",T::Property);
 for(unsigned i=1;i<unsigned(L::Count);++i){token("# class def null \"text\"",L::Plain,"class",T::Plain);check(*syntaxLanguageName(L(i)),"every selectable profile has a name");}
 for(auto pair:{std::pair<const char*,L>{"/SD/TEST.C",L::C},{"/SD/demo.ino",L::Cpp},{"/SD/demo.PY",L::Python},{"/SD/DATA.JSON",L::Json},{"CMakeLists.txt",L::CMake},{"Makefile",L::Makefile},{"/SD/.env.local",L::Ini},{"/SD/trace.log",L::Plain},{"/SD/unknown.zzz",L::Plain}})check(syntaxLanguage(pair.first)==pair.second,"extension or exact filename selects expected profile");
 check(sourceFile("trace.log")&&sourceFile("demo.dart")&&editableFile("notes.md")&&!pythonFile("demo.pyc"),"file policy preserves supported source formats without borrowing a lexer");
 token("/* comment\n class \"str\"\n*/ int value;",L::C,"class",T::Comment);token("/* comment\n class \"str\"\n*/ int value;",L::C,"int",T::Type);
 token("doc = \"\"\"alpha\nclass int # text\n\"\"\"\ndef test(): pass",L::Python,"class",T::String);token("doc = \"\"\"alpha\nclass int # text\n\"\"\"\ndef test(): pass",L::Python,"def",T::Keyword);
 token("auto text = u8R\"tag(alpha\nclass /* text */\n)tag\"; int x;",L::Cpp,"class",T::String);token("auto text = u8R\"tag(alpha\nclass /* text */\n)tag\"; int x;",L::Cpp,"int x",T::Type);
 token("--[==[ comment\nlocal text\n]==]\nlocal x = [=[text\nreturn]=]",L::Lua,"local",T::Comment);token("--[==[ comment\nlocal text\n]==]\nlocal x = [=[text\nreturn]=]",L::Lua,"return",T::String);
 token("<!-- start\nclass def\n--> <tag/>",L::Markup,"class",T::Comment);token("/* one /* two */ still */ fn main() {}",L::Rust,"still",T::Comment);
 token("étiquette = '日本語 😀'\nreturn étiquette",L::Python,"return",T::Keyword);token("int myint = 1; integer className",L::C,"integer",T::Plain);
}
static std::string textOf(const Document& d){std::string s(d.size(),' ');d.copy(0,s.data(),s.size());return s;}
static void contexts(){
 const std::vector<std::pair<L,std::string>> fixtures={{L::C,"#include <a/b.h>\n/* alpha\nbeta */ int x; // line\nchar *s=\"hello\\\"\";\n"},{L::Python,"x = '''alpha\nβeta'''\n# note\nprint(\"ok\")"},{L::Cpp,"auto s=u8R\"_x(alpha\nβeta)_x\"; /* note */"},{L::Lua,"--[==[comment\nword]==]\nlocal s=[=[one\ntwo]=]"},{L::Markup,"<section name=\"value\">hello\n<!-- note\n--> x</section>"},{L::Rust,"/* first /* nested */ continued */\nfn main() {}"},{L::Sql,"select 'it''s\nquoted'; -- note\nselect 1;"},{L::JavaScript,"let text = `line\nline`; // comment\nconst x=1;"}};
 for(auto& f:fixtures){Document d;check(d.assign(f.second.data(),f.second.size()),"context fixture load");for(size_t split=0;split<=d.size();++split){SyntaxState whole,chunked;scanSyntax(d,0,d.size(),f.first,whole);scanSyntax(d,0,split,f.first,chunked);scanSyntax(d,split,d.size(),f.first,chunked);check(whole==chunked,"raw lexer state independent of arbitrary split");}SyntaxState whole,byte;scanSyntax(d,0,d.size(),f.first,whole);for(size_t p=0;p<d.size();++p)scanSyntax(d,p,p+1,f.first,byte);check(byte==whole,"one-byte incremental scanner equals whole document");}
 for(auto& f:fixtures){
  Document d;check(d.assign(f.second.data(),f.second.size()),"paged fixture loaded");RichText full;full.parse(f.second,true,true);colorSyntax(full,"source",true,f.first);
  for(size_t at=0;at<f.second.size();++at)if(at==0||f.second[at-1]=='\n'){
   SyntaxState initial;scanSyntax(d,0,at,f.first,initial);RichText page;page.parse(f.second.substr(at),true,true);colorSyntax(page,"page",true,f.first,initial);
   for(size_t i=0;i<page.size();++i){const auto glyph=page.glyph(i);if(glyph.code=='\n')continue;const size_t absolute=at+page.sourceAt(i);check(glyph.foreground==full.glyph(full.indexAtSource(absolute)).foreground,"paged multiline rendering matches whole-document token colors");}
  }
 }
 {Document d;const std::string s="myreturn value;";check(d.assign(s.data(),s.size()),"partial identifier fixture");SyntaxState state;scanSyntax(d,0,2,L::C,state);RichText page;page.parse(s.substr(2),true,true);colorSyntax(page,"page.c",true,L::C,state);check(page.glyph(0).foreground==NoColor,"partial identifier does not become a keyword at a page boundary");}
 Document d;std::string source="/* start\n"+std::string(18000,'a')+"\nend */\nint value = 42;\n";check(d.assign(source.data(),source.size()),"paged large comment load");SyntaxIndex index;SyntaxState state;check(!index.stateAt(d,L::C,17000,state),"far jump context initially unavailable");size_t loops=0;while(index.scannedBytes()<d.size()){const auto before=index.scannedBytes();index.step(d,L::C,257);check(index.scannedBytes()-before<=257,"index scan respects per-pass byte budget");check(++loops<100,"bounded progress to final page");}check(index.bytes()<40000,"2MiB index bounded to under40KiB on64bit host");
 for(size_t offset:{0u,1u,4095u,4096u,4097u,17000u,18010u}){SyntaxState expected;scanSyntax(d,0,offset,L::C,expected);check(index.stateAt(d,L::C,offset,state)&&state==expected,"indexed window context matches full-prefix reference");}
 size_t offset=source.find("end */");check(index.stateAt(d,L::C,offset,state),"page beginning inside block comment resolved");RichText r;r.parse(source.substr(offset),true,true);colorSyntax(r,"page.c",true,L::C,state);check(r.glyph(0).foreground==syntaxColor(T::Comment)&&r.glyph(r.indexAtSource(7)).foreground==syntaxColor(T::Type),"continued comment ends and following C type renders correctly");
 check(d.replace(0,2,"//",2,0,0),"edit opener before cached far page");index.edited(d,0);check(index.scannedBytes()==0&&!index.stateAt(d,L::C,17000,state),"edit invalidates downstream context");while(index.scannedBytes()<d.size())index.step(d,L::C);check(index.stateAt(d,L::C,17000,state)&&state.mode==SyntaxState::Normal,"reindexed comment removal fixes downstream colors");
 check(d.replace(4094,4094,"/*",2,0,0),"insert opener across a checkpoint");index.edited(d,4094);while(index.scannedBytes()<d.size())index.step(d,L::C,4096);SyntaxState exact;scanSyntax(d,0,9000,L::C,exact);check(index.stateAt(d,L::C,9000,state)&&state==exact&&state.mode==SyntaxState::BlockComment,"lookahead invalidation covers nearby boundary edit");
 check(!index.stateAt(d,L::Python,9000,state),"language change invalidates previous profile checkpoints");index.reset();check(index.stateAt(d,L::Plain,9000,state)&&state.mode==SyntaxState::Normal,"plain mode needs no prefix scan");
 {SyntaxIndex unavailable;const auto before=textOf(d);failNext=true;check(!unavailable.stateAt(d,L::C,9000,state)&&unavailable.failed(),"PSRAM exhaustion disables coloring context gracefully");check(textOf(d)==before,"color context failure preserves document");check(unavailable.stateAt(d,L::C,0,state),"first page remains usable without checkpoint memory");unavailable.reset();unavailable.step(d,L::C);check(!unavailable.failed()&&unavailable.scannedBytes()==4096,"explicit reset retries allocation");}
 {Document max;std::string longLine(Document::Limit,'x');check(max.assign(longLine.data(),longLine.size()),"maximum-size doc admitted");SyntaxIndex bounded;size_t steps=0;while(bounded.scannedBytes()<max.size()){bounded.step(max,L::C);++steps;}check(steps==512&&!bounded.failed(),"2MiB document requires exactly512 bounded passes");check(bounded.stateAt(max,L::C,max.size(),state),"final checkpoint valid at size limit");}
}
struct MemoryFS:FileSystem{std::map<std::string,std::string> files;std::string active;size_t at=0;unsigned renames=0,failRenameAt=0;bool writeError=false;bool exists(const char*p)override{return files.count(p);}bool remove(const char*p)override{return files.erase(p);}bool rename(const char*a,const char*b)override{if(++renames==failRenameAt||!files.count(a))return false;files[b]=files[a];files.erase(a);return true;}bool openRead(const char*p)override{if(!exists(p))return false;active=p;at=0;return true;}bool openWrite(const char*p)override{active=p;files[p].clear();at=0;return true;}size_t length()override{return files[active].size();}size_t write(const char*p,size_t n)override{if(writeError)return 0;files[active].append(p,n);return n;}size_t read(char*p,size_t n)override{auto&s=files[active];n=std::min(n,s.size()-at);memcpy(p,s.data()+at,n);at+=n;return n;}void close()override{active.clear();}};
static void put32(std::string&s,size_t p,uint32_t v){for(unsigned i=0;i<4;++i)s[p+i]=char(v>>(8*i));}
static void hashFile(std::string&s){put32(s,12,s.size());uint32_t h=2166136261u;for(size_t i=20;i<s.size();++i){h^=uint8_t(s[i]);h*=16777619u;}put32(s,16,h);}
static void workspace(){
 const char* path="/System/editor_workspace.dat";MemoryFS fs;{Workspace w;check(w.begin(),"workspace allocated");int a=w.add("/Code/demo.h"),b=w.add("/Code/data.txt");check(a==0&&b==1,"two independent entries");w.entry(a)->syntaxLanguage=uint8_t(L::Cpp);w.entry(a)->cursor=132;w.entry(b)->syntaxLanguage=uint8_t(L::Json);check(w.save(fs),"v2 workspace saved");}
 check(fs.files[path].substr(0,8)=="T5WS0002","new workspace version explicit");{Workspace w;check(w.load(fs)&&w.count()==2,"v2 reload");check(w.entry(0)->syntaxLanguage==uint8_t(L::Cpp)&&w.entry(1)->syntaxLanguage==uint8_t(L::Json)&&w.entry(0)->cursor==132,"per-document overrides and positions persisted independently");}
 const auto modern=fs.files[path];auto legacy=modern;legacy.erase(28+318+317,1);legacy.erase(28+317,1);legacy.replace(0,8,"T5WS0001");put32(legacy,8,1);hashFile(legacy);fs.files.clear();fs.files[path]=legacy;
 {Workspace w;check(w.load(fs)&&w.count()==2,"0.8.39 v1 workspace loads");check(w.entry(0)->syntaxLanguage==0&&w.entry(1)->syntaxLanguage==0&&w.entry(0)->cursor==132,"v1 uses Auto and preserves positions");w.entry(0)->syntaxLanguage=uint8_t(L::Python);w.markDirty();check(w.save(fs),"legacy workspace migrates transactionally");check(fs.files[path].substr(0,8)=="T5WS0002","migration writesv2");}
 fs.files.clear();auto bad=modern;bad[28+317]=char(255);hashFile(bad);fs.files[path]=bad;{Workspace w;check(!w.load(fs)&&w.count()==0,"invalid persisted language cannot escape enum bounds");}
 for(unsigned failure:{1u,2u}){fs.files.clear();fs.files[path]=modern;fs.renames=0;fs.failRenameAt=failure;Workspace w;check(w.load(fs),"rollback fixture load");w.entry(0)->syntaxLanguage=uint8_t(L::Plain);w.markDirty();check(!w.save(fs),"rename failure propagated");check(fs.files[path]==modern,"transaction preserves previously saved workspace on failed rename");}
 fs.failRenameAt=0;fs.files.clear();fs.files[path]=modern;fs.writeError=true;{Workspace w;check(w.load(fs),"write-failure fixture load");w.markDirty();check(!w.save(fs)&&fs.files[path]==modern,"failed workspace write preserves last valid file");}
}
int main(){const auto started=std::chrono::steady_clock::now();languages();contexts();workspace();check(allocations==0,"all production PSRAM allocations released after documents/indexes/workspaces");printf("{\"passed\":true,\"checks\":%u,\"hostMilliseconds\":%lld,\"maximumDocumentBytes\":%zu}\n",checks,(long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count(),Document::Limit);}
