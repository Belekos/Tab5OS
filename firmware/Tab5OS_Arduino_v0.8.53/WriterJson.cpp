#include "WriterJson.h"
#include <cstring>
namespace writing {
namespace {
class Parser {
 const Document& d; size_t p=0; const char* error="JSON invalide";
 char peek() const { return p<d.size()?d.at(p):0; }
 void ws(){while(peek()==' '||peek()=='\t'||peek()=='\r'||peek()=='\n')++p;}
 bool hex4(uint16_t& value){value=0;for(unsigned n=0;n<4;++n){char c=peek();unsigned v=c>='0'&&c<='9'?unsigned(c-'0'):c>='a'&&c<='f'?unsigned(c-'a'+10):c>='A'&&c<='F'?unsigned(c-'A'+10):16;if(v>15)return false;value=uint16_t((value<<4)|v);++p;}return true;}
 bool string(){if(peek()!='"')return false;++p;while(p<d.size()){unsigned char c=peek();++p;if(c=='"')return true;if(c<32)return false;if(c!='\\')continue;c=peek();if(!c)return false;++p;if(c=='"'||c=='\\'||c=='/'||c=='b'||c=='f'||c=='n'||c=='r'||c=='t')continue;if(c!='u')return false;uint16_t u;if(!hex4(u))return false;if(u>=0xd800&&u<=0xdbff){if(peek()!='\\')return false;++p;if(peek()!='u')return false;++p;uint16_t low;if(!hex4(low)||low<0xdc00||low>0xdfff)return false;}else if(u>=0xdc00&&u<=0xdfff)return false;}return false;}
 bool number(){if(peek()=='-')++p;if(peek()=='0')++p;else{if(peek()<'1'||peek()>'9')return false;while(peek()>='0'&&peek()<='9')++p;}if(peek()=='.'){++p;if(peek()<'0'||peek()>'9')return false;while(peek()>='0'&&peek()<='9')++p;}if(peek()=='e'||peek()=='E'){++p;if(peek()=='+'||peek()=='-')++p;if(peek()<'0'||peek()>'9')return false;while(peek()>='0'&&peek()<='9')++p;}return true;}
 bool word(const char* word){while(*word)if(peek()!=*word++)return false;else ++p;return true;}
 bool value(unsigned depth){ws();if(depth>64){error="JSON trop imbriqué (64 niveaux maximum)";return false;}char c=peek();if(c=='"')return string();if(c=='-'||(c>='0'&&c<='9'))return number();if(c=='t')return word("true");if(c=='f')return word("false");if(c=='n')return word("null");if(c!='{'&&c!='[')return false;++p;ws();char end=c=='{'?'}':']';if(peek()==end){++p;return true;}for(;;){if(c=='{'){if(!string())return false;ws();if(peek()!=':')return false;++p;}if(!value(depth+1))return false;ws();if(peek()==end){++p;return true;}if(peek()!=',')return false;++p;ws();}}
 public:
 explicit Parser(const Document& document):d(document){}
 JsonValidation run(){bool ok=value(0);ws();if(p!=d.size())ok=false;return {ok,p,ok?"JSON valide":error};}
};
}
JsonValidation validateJson(const Document& d){return Parser(d).run();}
}
