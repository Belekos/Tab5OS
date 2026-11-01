#pragma once
#include <string>
#include <vector>
#include <functional>
#include <utility>
#include <memory>
#include "Tab5ShellBuffer.h"
namespace Tab5Shell {
// Native SD shell. It does not share an interpreter or task with MicroPython.
class Engine {
 public:
  using Output=std::function<void(const char*,size_t)>;
  using Cancel=std::function<bool()>;
  struct Environment {
    std::function<int(const std::string&,std::string&)> get;
    std::function<int(const std::string&,const std::string&,bool)> set;
    std::function<int(const std::string&)> erase;
    std::function<std::vector<std::string>()> names;
  };
  void environment(Environment hooks){environment_=std::move(hooks);}
  using Input=std::function<size_t(char*,size_t)>;
  explicit Engine(std::string root,Output output,Cancel cancel={});
  void storageAvailable(bool value){storageAvailable_=value;}
  int run(const std::string& text);
  const std::string& cwd()const{return cwd_;}
  std::string complete(const std::string& text);
  void translate(std::function<std::string(const char*)> callback){translate_=callback;}
  void admin(std::function<int(const std::vector<std::string>&,Output)> callback){admin_=callback;}
  Output diagnostics()const{return errors_;}
  Input input()const{return input_;}
  static const char* help(const char* command=nullptr);
 private:
  struct Token {std::string raw;bool op=false;};
  std::string root_,cwd_="/";Output output_,errors_;Cancel cancel_;
  Input input_;bool outputFailed_=false;unsigned substitutions_=0;
  int substitutionStatus_=0;bool substituted_=false;
  std::function<std::string(const char*)> translate_;
  std::string tr(const char* text){return translate_?translate_(text):std::string(text);}
  std::vector<std::pair<std::string,std::shared_ptr<Buffer>>> variables_;
  std::function<int(const std::vector<std::string>&,Output)> admin_;
  Environment environment_;mutable bool environmentFailed_=false;
  bool storageAvailable_=true;
  unsigned steps_=0,depth_=0;int last_=0;
  bool stopped();
  int error(const std::string& message);
  void out(const std::string& text){if(!outputFailed_)output_(text.data(),text.size());}
  bool lex(const std::string&,std::vector<Token>&);
  bool expand(const Token&,std::vector<std::string>&,bool allowAssignment=false);
  bool substitute(const std::string&,std::string&);
  std::string value(const std::string&)const;
  bool variable(const std::string&,const std::string&);
  bool path(const std::string&,std::string&,bool write=false);
  std::string disk(const std::string& p)const{return root_+p;}
  int block(const std::vector<Token>&,size_t,size_t);
  int command(const std::vector<Token>&,size_t,size_t);
  int pipeline(const std::vector<Token>&,size_t,size_t);
  int builtin(const std::vector<std::string>&);
  int execute(const std::vector<std::string>&);
  int script(const std::vector<std::string>&);
  int readScript(const std::string&,std::string&);
  int appendOutput(const std::string&,const std::string&);
  int printHelp(const char* command);
  int visit(const std::string&,const std::string&,bool,bool,unsigned=0);
  int copyFile(const std::string&,const std::string&);
};
}
