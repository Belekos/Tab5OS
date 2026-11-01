#include "WriterSyntax.h"
#include "WriterRich.h"
#include "WriterDocument.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
namespace writing {
namespace {
using L=SyntaxLanguage;using T=SyntaxToken;using S=SyntaxState;
std::string lower(const char* s){std::string r=s?s:"";for(auto& c:r)c=char(std::tolower(static_cast<unsigned char>(c)));return r;}
std::string suffix(const char* path){const auto p=lower(path);auto slash=p.find_last_of('/'),dot=p.find_last_of('.');return dot!=std::string::npos&&(slash==std::string::npos||dot>slash)?p.substr(dot):"";}
bool alpha(uint32_t c){return c>=128||c=='_'||(c>='A'&&c<='Z')||(c>='a'&&c<='z');}
bool digit(uint32_t c){return c>='0'&&c<='9';}
bool identifier(uint32_t c){return alpha(c)||digit(c);}
bool space(uint32_t c){return c==' '||c=='\t'||c=='\r';}
bool oneOf(L l,std::initializer_list<L> values){for(auto v:values)if(l==v)return true;return false;}
bool slashComments(L l){return oneOf(l,{L::C,L::Cpp,L::JavaScript,L::TypeScript,L::JsonComments,L::Css,L::Rust,L::Go,L::Java,L::Kotlin,L::Swift,L::Php});}
bool hashComments(L l){return oneOf(l,{L::Python,L::Shell,L::Yaml,L::Toml,L::Ini,L::Ruby,L::Php,L::PowerShell,L::CMake,L::Makefile});}
bool multilineQuote(L l,uint32_t quote){return (quote=='`'&&oneOf(l,{L::JavaScript,L::TypeScript,L::Go}))||oneOf(l,{L::Sql,L::PowerShell,L::Ruby});}
bool quotes(L l,uint32_t c){if(c=='"')return true;if(c=='\'')return l!=L::Json&&l!=L::JsonComments&&l!=L::CMake;if(c=='`')return oneOf(l,{L::JavaScript,L::TypeScript,L::Go});return false;}
#include "WriterSyntaxWords.inc"
T wordKind(L language,const char* word){
 for(T kind:{T::Keyword,T::Type,T::Builtin}){auto v=vocabulary(language,kind);size_t a=0,b=v.count;while(a<b){size_t m=a+(b-a)/2;int c=std::strcmp(word,v.words[m]);if(!c)return kind;if(c<0)b=m;else a=m+1;}}
 return T::Plain;
}
void newline(S& s,L l){
 if(s.mode==S::LineComment)s.mode=S::Normal;
 else if(s.mode==S::Quote&&!multilineQuote(l,s.quote)&&!s.escaped)s.mode=S::Normal;
 s.escaped=false;s.lineStart=true;s.directive=0;s.previous='\n';
}
template<class Read> bool matches(Read read,size_t at,size_t available,const char* word){for(size_t j=0;word[j];++j)if(at+j>=available||read(at+j)!=uint8_t(word[j]))return false;return true;}
template<class Read> size_t blanks(Read read,size_t at,size_t end){while(at<end&&space(read(at)))++at;return at;}
template<class Read> unsigned longBracket(Read read,size_t at,size_t available){
 if(at>=available||read(at)!='[')return 0;
 size_t i=at+1;while(i<available&&read(i)=='='&&i-at<=16)++i;
 return i<available&&read(i)=='['?unsigned(i-at+1):0;
}
template<class Read> bool closeLong(Read read,size_t at,size_t available,unsigned width){
 if(width<2||at+width>available||read(at)!=']'||read(at+width-1)!=']')return false;
 for(unsigned j=1;j+1<width;++j)if(read(at+j)!='=')return false;
 return true;
}
template<class Read> bool jsonKey(Read read,size_t at,size_t available){
 // This is presentation only. Very long keys retain string coloring; lexical
 // context scanning never runs this look-ahead.
 const size_t end=std::min(available,at+1024);bool escaped=false;
 for(size_t i=at+1;i<end;++i){const auto c=read(i);if(c=='\n')break;if(escaped){escaped=false;continue;}if(c=='\\'){escaped=true;continue;}if(c=='"'){i=blanks(read,i+1,end);return i<end&&read(i)==':';}}
 return false;
}
template<class Read> size_t numberEnd(Read read,size_t at,size_t end,L l){
 size_t i=at;const bool json=l==L::Json||l==L::JsonComments;bool hex=false;
 if(i<end&&read(i)=='-')++i;
 if(!json&&i+1<end&&read(i)=='0'&&(read(i+1)=='x'||read(i+1)=='X')){hex=true;i+=2;while(i<end&&(digit(read(i))||(read(i)>='a'&&read(i)<='f')||(read(i)>='A'&&read(i)<='F')||read(i)=='_'||(l==L::Cpp&&read(i)=='\'')))++i;}
 else if(!json&&i+1<end&&read(i)=='0'&&(read(i+1)=='b'||read(i+1)=='B'||read(i+1)=='o'||read(i+1)=='O')){const bool binary=read(i+1)=='b'||read(i+1)=='B';i+=2;while(i<end&&((read(i)>='0'&&read(i)<=(binary?'1':'7'))||read(i)=='_'))++i;}
 else {while(i<end&&(digit(read(i))||(!json&&(read(i)=='_'||(l==L::Cpp&&read(i)=='\'')))))++i;}
 if(i<end&&read(i)=='.'&&(i+1==end||read(i+1)!='.')){++i;while(i<end&&(digit(read(i))||(!json&&read(i)=='_')||(hex&&((read(i)>='a'&&read(i)<='f')||(read(i)>='A'&&read(i)<='F')))))++i;}
 if(i<end&&(hex?(read(i)=='p'||read(i)=='P'):(read(i)=='e'||read(i)=='E'))){size_t j=i+1;if(j<end&&(read(j)=='+'||read(j)=='-'))++j;if(j<end&&digit(read(j))){i=j+1;while(i<end&&(digit(read(i))||(!json&&read(i)=='_')))++i;}}
 if(oneOf(l,{L::C,L::Cpp,L::Java,L::Kotlin,L::Swift})){while(i<end&&(read(i)=='u'||read(i)=='U'||read(i)=='l'||read(i)=='L'||read(i)=='f'||read(i)=='F'))++i;}
 else if(l==L::Python&&i<end&&(read(i)=='j'||read(i)=='J'))++i;
 else if(oneOf(l,{L::JavaScript,L::TypeScript})&&i<end&&read(i)=='n')++i;
 else if(l==L::Go&&i<end&&read(i)=='i')++i;
 else if(l==L::Rust&&i<end&&(read(i)=='u'||read(i)=='i'||read(i)=='f')){++i;while(i<end&&identifier(read(i)))++i;}
 else if(l==L::Css){while(i<end&&((read(i)>='a'&&read(i)<='z')||read(i)=='%'))++i;}
 return std::max(i,at+1);
}
// One scanner serves Unicode glyph windows and raw document bytes. The raw
// pass only maintains context; it does no word/number classification or string
// allocation and consumes exactly [begin,end), including delimiter fragments.
template<bool Colors,class Read,class Paint> void scan(Read read,size_t available,size_t begin,size_t end,L l,S& state,Paint paint){
 if(l==L::Plain||l==L::Auto){state={};return;}
 const bool slash=slashComments(l),nested=oneOf(l,{L::Rust,L::Kotlin,L::Swift});
 for(size_t i=begin;i<end;){const size_t first=i;const auto c=read(i);auto emit=[&](size_t a,size_t b,T token){if(Colors)paint(a,b,token);};
  if(state.skip){emit(i,i+1,state.skipToken);--state.skip;state.previous=c;++i;continue;}
  if(c=='\n'){if(state.mode!=S::Normal)emit(i,i+1,state.mode==S::LineComment||state.mode==S::BlockComment||state.mode==S::HtmlComment||state.mode==S::LongComment?T::Comment:T::String);newline(state,l);++i;continue;}
  if(state.mode==S::LineComment){emit(i,i+1,T::Comment);state.previous=c;++i;continue;}
  if(state.mode==S::BlockComment){
   emit(i,i+1,T::Comment);
   if(c=='*'&&i+1<available&&read(i+1)=='/'){if(state.depth>1)--state.depth;else{state.depth=0;state.mode=S::Normal;}state.skip=1;state.skipToken=T::Comment;}
   else if(nested&&c=='/'&&i+1<available&&read(i+1)=='*'){if(state.depth<65535)++state.depth;state.skip=1;state.skipToken=T::Comment;}
   state.previous=c;++i;continue;
  }
  if(state.mode==S::HtmlComment){emit(i,i+1,T::Comment);if(matches(read,i,available,"-->")){state.mode=S::Normal;state.skip=2;state.skipToken=T::Comment;}state.previous=c;++i;continue;}
  if(state.mode==S::LongComment||state.mode==S::LongString){const auto token=state.mode==S::LongComment?T::Comment:T::String;emit(i,i+1,token);if(closeLong(read,i,available,state.brackets)){state.mode=S::Normal;state.skip=state.brackets-1;state.skipToken=token;}state.previous=c;++i;continue;}
  if(state.mode==S::RawString){emit(i,i+1,T::String);bool close=c==')'&&i+state.rawLength+1<available&&read(i+state.rawLength+1)=='"';for(unsigned n=0;close&&n<state.rawLength;++n)close=read(i+n+1)==uint8_t(state.raw[n]);if(close){state.mode=S::Normal;state.skip=state.rawLength+1;state.skipToken=T::String;}state.previous=c;++i;continue;}
  if(state.mode==S::Quote||state.mode==S::TripleQuote){
   emit(i,i+1,state.quoteToken);
   if(state.escaped){state.escaped=false;}
   else if((c=='\\'&&!(l==L::Go&&state.quote=='`'))||(l==L::PowerShell&&c=='`')){state.escaped=true;}
   else if(c==state.quote){
    if(state.mode==S::TripleQuote){if(i+2<available&&read(i+1)==c&&read(i+2)==c){state.mode=S::Normal;state.skip=2;state.skipToken=state.quoteToken;}}
    else if(l==L::Sql&&i+1<available&&read(i+1)==c){state.skip=1;state.skipToken=state.quoteToken;}
    else state.mode=S::Normal;
   }
   state.previous=c;++i;continue;
  }
  if(space(c)){state.previous=c;++i;continue;}
  const bool firstOnLine=state.lineStart;state.lineStart=false;
  if(l==L::Markup){
   if(matches(read,i,available,"<!--")){state.mode=S::HtmlComment;state.skip=3;state.skipToken=T::Comment;emit(i,i+1,T::Comment);state.previous=c;++i;continue;}
   if(c=='<'){state.inTag=true;state.tagName=true;emit(i,i+1,T::Tag);if(i+1<available&&(read(i+1)=='/'||read(i+1)=='!'||read(i+1)=='?')){state.skip=1;state.skipToken=T::Tag;}state.previous=c;++i;continue;}
   if(c=='>'){state.inTag=false;state.tagName=false;emit(i,i+1,T::Tag);state.previous=c;++i;continue;}
   if(!state.inTag){state.previous=c;++i;continue;}
  }
  bool hash=hashComments(l)&&c=='#';
  if(hash&&oneOf(l,{L::Shell,L::Yaml,L::Makefile})&&!firstOnLine&&!space(state.previous))hash=false;
  if((slash&&l!=L::Css&&c=='/'&&i+1<available&&read(i+1)=='/')||hash||(oneOf(l,{L::Sql,L::Lua})&&c=='-'&&i+1<available&&read(i+1)=='-')||(l==L::Ini&&c==';')){
   if(l==L::Lua&&c=='-'&&i+2<available){auto width=longBracket(read,i+2,available);if(width){state.mode=S::LongComment;state.brackets=width;state.skip=width+1;state.skipToken=T::Comment;emit(i,i+1,T::Comment);state.previous=c;++i;continue;}}
   state.mode=S::LineComment;emit(i,i+1,T::Comment);state.previous=c;++i;continue;
  }
  if((slash||l==L::Sql)&&c=='/'&&i+1<available&&read(i+1)=='*'){state.mode=S::BlockComment;state.depth=1;state.skip=1;state.skipToken=T::Comment;emit(i,i+1,T::Comment);state.previous=c;++i;continue;}
  if(l==L::Lua&&c=='['){auto width=longBracket(read,i,available);if(width){state.mode=S::LongString;state.brackets=width;state.skip=width-1;state.skipToken=T::String;emit(i,i+1,T::String);state.previous=c;++i;continue;}}
  size_t rawAt=i;
  if(l==L::Cpp){for(const char* prefix:{"u8R\"","uR\"","UR\"","LR\""})if(matches(read,i,available,prefix)){rawAt=i+std::strlen(prefix)-2;break;}}
  if(l==L::Cpp&&rawAt+1<available&&read(rawAt)=='R'&&read(rawAt+1)=='"'){
   size_t j=rawAt+2;while(j<available&&j-rawAt<18&&read(j)!='('&&read(j)!=' '&&read(j)!='\n'&&read(j)!='\\'&&read(j)!=')')++j;
   if(j<available&&read(j)=='('){state.mode=S::RawString;state.rawLength=uint8_t(j-rawAt-2);for(unsigned n=0;n<state.rawLength;++n)state.raw[n]=char(read(rawAt+2+n));state.skip=uint8_t(j-i);state.skipToken=T::String;emit(i,i+1,T::String);state.previous=c;++i;continue;}
  }
  if(oneOf(l,{L::C,L::Cpp})&&state.directive==2&&c=='<'){state.mode=S::Quote;state.quote='>';state.quoteToken=T::String;state.directive=0;emit(i,i+1,T::String);state.previous=c;++i;continue;}
  if(Colors&&l==L::Python&&alpha(c)){
   size_t j=i;while(j<available&&j-i<2&&(read(j)=='f'||read(j)=='F'||read(j)=='r'||read(j)=='R'||read(j)=='b'||read(j)=='B'||read(j)=='u'||read(j)=='U'))++j;
   if(j>i&&j<available&&(read(j)=='\"'||read(j)=='\'')){emit(i,std::min(j,end),T::String);i=std::min(j,end);state.previous=read(i-1);continue;}
  }
  if(quotes(l,c)){
   // Rust lifetimes ('a, 'static) are identifiers, not open character literals.
   if(l==L::Rust&&c=='\''&&i+1<available&&alpha(read(i+1))&&!(i+2<available&&read(i+2)=='\'')){emit(i,i+1,T::Type);state.previous=c;++i;continue;}
   state.quote=uint8_t(c);state.quoteToken=T::String;state.escaped=false;
   if(Colors&&(l==L::Json||l==L::JsonComments)&&c=='"'&&jsonKey(read,i,available))state.quoteToken=T::Property;
   state.mode=S::Quote;
   if(oneOf(l,{L::Python,L::Toml,L::Kotlin,L::Swift})&&i+2<available&&read(i+1)==c&&read(i+2)==c){state.mode=S::TripleQuote;state.skip=2;state.skipToken=state.quoteToken;}
   emit(i,i+1,state.quoteToken);state.previous=c;++i;continue;
  }
  if(oneOf(l,{L::C,L::Cpp})&&c=='#'&&firstOnLine){state.directive=1;emit(i,i+1,T::Directive);state.previous=c;++i;continue;}
  if(l==L::Rust&&c=='#'&&i+1<available&&(read(i+1)=='['||read(i+1)=='!')){emit(i,i+1,T::Directive);state.previous=c;++i;continue;}
  if(Colors&&l==L::Python&&c=='@'&&firstOnLine){++i;while(i<end&&(identifier(read(i))||read(i)=='.'))++i;emit(first,i,T::Decorator);state.previous=read(i-1);continue;}
  if(Colors&&l==L::Css&&c=='#'){++i;while(i<end&&identifier(read(i)))++i;emit(first,i,T::Number);state.previous=read(i-1);continue;}
  if(Colors&&c=='$'&&oneOf(l,{L::Shell,L::Php,L::PowerShell,L::Makefile})){++i;while(i<end&&(identifier(read(i))||read(i)==':'))++i;emit(first,i,T::Builtin);state.previous=read(i-1);continue;}
  if(Colors&&(digit(c)||(c=='.'&&i+1<end&&digit(read(i+1)))||((l==L::Json||l==L::JsonComments)&&c=='-'&&i+1<end&&digit(read(i+1))))&&!identifier(state.previous)){
   i=numberEnd(read,i,end,l);emit(first,i,T::Number);state.previous=read(i-1);continue;
  }
  if(Colors&&alpha(c)){
   char word[72];size_t n=0;bool fits=true;
   do{auto code=read(i++);if(code>=128||n+1>=sizeof(word))fits=false;else if(fits)word[n++]=char(l==L::Sql?std::toupper(static_cast<unsigned char>(code)):code);}while(i<end&&(identifier(read(i))||((l==L::Css||l==L::Markup||l==L::PowerShell)&&read(i)=='-')));
   word[n]=0;const bool continuation=identifier(state.previous);
   T token=fits&&!continuation?wordKind(l,word):T::Plain;
   size_t next=blanks(read,i,std::min(available,i+128));
   if(l==L::Markup){token=state.tagName?T::Tag:T::Attribute;state.tagName=false;}
   else if(state.directive){token=T::Directive;if(state.directive==1)state.directive=!std::strcmp(word,"include")?2:!std::strcmp(word,"define")?3:0;else state.directive=0;}
   else if(oneOf(l,{L::Yaml,L::Css,L::Makefile})&&next<available&&read(next)==':')token=T::Property;
   else if(oneOf(l,{L::Toml,L::Ini})&&next<available&&read(next)=='=')token=T::Property;
   else if(!continuation&&token==T::Plain&&next<available&&read(next)=='('&&!oneOf(l,{L::Json,L::JsonComments,L::Yaml,L::Toml,L::Ini}))token=T::Function;
   emit(first,i,token);state.previous=read(i-1);continue;
  }
  // Only the directive's lexical context matters during the raw pass.
  if(!Colors&&state.directive&&alpha(c)){
   if(state.directive==1&&matches(read,i,available,"include")&&(i+7==available||!identifier(read(i+7)))){state.directive=2;state.skip=6;state.skipToken=T::Directive;}
   else if(state.directive==1&&matches(read,i,available,"define")&&(i+6==available||!identifier(read(i+6)))){state.directive=3;state.skip=5;state.skipToken=T::Directive;}
   else state.directive=0;
  }
  if(!Colors&&state.inTag&&state.tagName&&alpha(c))state.tagName=false;
  if(Colors&&c<128&&std::strchr("+-*/=<>!&|^~%?:",int(c)))emit(i,i+1,T::Operator);
  state.previous=c;++i;
 }
}
}
bool SyntaxState::operator==(const SyntaxState& o)const{return mode==o.mode&&quote==o.quote&&skip==o.skip&&brackets==o.brackets&&rawLength==o.rawLength&&directive==o.directive&&depth==o.depth&&escaped==o.escaped&&lineStart==o.lineStart&&inTag==o.inTag&&tagName==o.tagName&&skipToken==o.skipToken&&quoteToken==o.quoteToken&&previous==o.previous&&!std::memcmp(raw,o.raw,sizeof(raw));}
bool pythonFile(const char* path){return suffix(path)==".py";}
bool sourceFile(const char* path){
 const auto p=lower(path),ext=suffix(path);const auto slash=p.find_last_of('/');const auto base=p.substr(slash==std::string::npos?0:slash+1);
 if(base=="makefile"||base=="cmakelists.txt"||base=="dockerfile"||base==".env"||base==".gitignore"||base==".gitattributes"||base==".editorconfig"||base==".dockerignore"||base==".clang-format"||base.rfind(".env.",0)==0)return true;
 static const char* const extensions[]={".py",".pyi",".c",".h",".cc",".hh",".cpp",".hpp",".cxx",".hxx",".ino",".js",".jsx",".mjs",".cjs",".ts",".tsx",".json",".jsonc",".css",".scss",".html",".htm",".xml",".svg",".yaml",".yml",".toml",".ini",".cfg",".conf",".sh",".bash",".zsh",".bat",".ps1",".psm1",".psd1",".lua",".rs",".go",".java",".kt",".kts",".swift",".php",".rb",".sql",".r",".cmake",".mk",".csv",".tsv",".log",".vue",".svelte",".dart",".pl",".ex",".exs",".erl",".m",".mm",".gradle",".properties",".lock",".tex",".ipynb",".graphql",".proto"};
 for(auto* e:extensions)if(ext==e)return true;
 return false;
}
bool editableFile(const char* path){const auto e=suffix(path);return sourceFile(path)||e==".md"||e==".markdown"||e==".txt";}
SyntaxLanguage syntaxLanguage(const char* path){
 const auto p=lower(path),e=suffix(path);const auto slash=p.find_last_of('/');const auto base=p.substr(slash==std::string::npos?0:slash+1);
 if(base=="makefile"||e==".mk")return L::Makefile;
 if(base=="cmakelists.txt"||e==".cmake")return L::CMake;
 if(base==".env"||base.rfind(".env.",0)==0||base==".editorconfig")return L::Ini;
 if(e==".c"||e==".h")return L::C;
 if(e==".cc"||e==".cpp"||e==".cxx"||e==".hh"||e==".hpp"||e==".hxx"||e==".ino")return L::Cpp;
 if(e==".py"||e==".pyi")return L::Python;
 if(e==".json"||e==".ipynb")return L::Json;
 if(e==".jsonc")return L::JsonComments;
 if(e==".js"||e==".jsx"||e==".mjs"||e==".cjs")return L::JavaScript;
 if(e==".ts"||e==".tsx")return L::TypeScript;
 if(e==".css"||e==".scss")return L::Css;
 if(e==".html"||e==".htm"||e==".xml"||e==".svg"||e==".vue"||e==".svelte")return L::Markup;
 if(e==".yaml"||e==".yml")return L::Yaml;
 if(e==".toml")return L::Toml;
 if(e==".ini"||e==".cfg"||e==".conf"||e==".properties")return L::Ini;
 if(e==".sh"||e==".bash"||e==".zsh")return L::Shell;
 if(e==".lua")return L::Lua;
 if(e==".sql")return L::Sql;
 if(e==".rs")return L::Rust;
 if(e==".go")return L::Go;
 if(e==".java")return L::Java;
 if(e==".kt"||e==".kts")return L::Kotlin;
 if(e==".swift")return L::Swift;
 if(e==".rb")return L::Ruby;
 if(e==".php")return L::Php;
 if(e==".ps1"||e==".psm1"||e==".psd1")return L::PowerShell;
 // Unknown source formats remain editable verbatim, without borrowed colors.
 return L::Plain;
}
const char* syntaxLanguageName(L l){
 static const char* const names[]={"Automatique (extension)","Texte brut","C","C++ / Arduino","Python","JSON","JSON avec commentaires","JavaScript","TypeScript","CSS / SCSS","HTML / XML","YAML","TOML","INI","Shell","Lua","SQL","Rust","Go","Java","Kotlin","Swift","Ruby","PHP","PowerShell","CMake","Makefile"};
 return names[unsigned(l)<unsigned(L::Count)?unsigned(l):0];
}
uint32_t syntaxColor(T token,bool dark){
 static const uint32_t colors[][2]={{NoColor,NoColor},{0x83958a,0x566956},{0xc3d69b,0x436922},{0xe3ba83,0x86501c},{0xaebcfa,0x454da1},{0x80c9ce,0x006b73},{0xe0cc95,0x795b08},{0xe5c890,0x775519},{0xd4a9d7,0x86408e},{0xdaabb3,0x954659},{0xa1d5cf,0x176e67},{0x94bbf0,0x2c5b9b},{0xdcc497,0x785a1d},{0xb8c6cb,0x4a5c62}};
 return colors[unsigned(token)<14?unsigned(token):0][dark?0:1];
}
void colorSyntax(RichText& text,const char* path,bool dark,L language,S initial){
 const L l=language==L::Auto?syntaxLanguage(path):language;
 for(auto& line:text.lines){auto& g=line.glyphs;for(auto& glyph:g){glyph.foreground=NoColor;glyph.background=NoColor;}
  scan<true>([&](size_t at)->uint32_t{return g[at].code;},g.size(),0,g.size(),l,initial,[&](size_t a,size_t b,T token){const auto color=syntaxColor(token,dark);for(size_t i=a;i<b;++i)g[i].foreground=color;});
  if(line.ending.find('\n')!=std::string::npos)newline(initial,l);
 }
}
void scanSyntax(const Document& document,size_t begin,size_t end,L language,S& state){
 scan<false>([&](size_t at)->uint32_t{return uint8_t(document.at(at));},document.size(),begin,std::min(end,document.size()),language,state,[](size_t,size_t,T){});
}
}
