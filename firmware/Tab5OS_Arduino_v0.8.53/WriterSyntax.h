#pragma once
#include <cstddef>
#include <cstdint>
namespace writing {
class RichText;
class Document;
// Values are stored in the workspace. Append new languages, never reorder.
enum class SyntaxLanguage : uint8_t { Auto, Plain, C, Cpp, Python, Json, JsonComments,
 JavaScript, TypeScript, Css, Markup, Yaml, Toml, Ini, Shell, Lua, Sql, Rust, Go,
 Java, Kotlin, Swift, Ruby, Php, PowerShell, CMake, Makefile, Count };
enum class SyntaxToken : uint8_t { Plain, Comment, String, Number, Keyword, Type,
 Builtin, Function, Directive, Decorator, Property, Tag, Attribute, Operator };
struct SyntaxState {
 enum Mode : uint8_t { Normal, LineComment, BlockComment, Quote, TripleQuote,
                      HtmlComment, LongComment, LongString, RawString };
 Mode mode=Normal;
 uint8_t quote=0,skip=0,brackets=0,rawLength=0,directive=0;
 uint16_t depth=0;
 bool escaped=false,lineStart=true,inTag=false,tagName=false;
 SyntaxToken skipToken=SyntaxToken::Plain,quoteToken=SyntaxToken::String;
 uint32_t previous=0;
 char raw[17]{};
 bool operator==(const SyntaxState& other) const;
 bool operator!=(const SyntaxState& other) const { return !(*this==other); }
};
bool sourceFile(const char* path);
bool editableFile(const char* path);
bool pythonFile(const char* path);
SyntaxLanguage syntaxLanguage(const char* path);
const char* syntaxLanguageName(SyntaxLanguage language);
uint32_t syntaxColor(SyntaxToken token, bool dark=true);
// Presentation only: colors never enter the serialized document. initial is
// the lexical context at the beginning of this editing window.
void colorSyntax(RichText&, const char* path, bool dark=true,
 SyntaxLanguage language=SyntaxLanguage::Auto, SyntaxState initial={});
// Exact byte-offset scanning, no snapshot, allocation, vocabulary lookup or
// unbounded look-ahead. Used by the incremental page-context index.
void scanSyntax(const Document&, size_t begin, size_t end, SyntaxLanguage, SyntaxState&);
}
