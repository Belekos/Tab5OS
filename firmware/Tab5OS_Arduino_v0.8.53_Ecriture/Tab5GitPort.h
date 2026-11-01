#pragma once
#include <string>
#include <functional>
namespace Tab5GitPort {
bool project(const std::string& physicalRoot);
void clear();
void cancellation(std::function<bool()> callback);
void networking(bool required);
// Constant phase tokens only: never pass a repository URL, path or secret.
void phase(const char* token);
void idle();
}
extern "C" int tab5_git_rename(const char* from,const char* to);
extern "C" int tab5_git_cancelled(void);
extern "C" int tab5_git_needs_tls(void);
