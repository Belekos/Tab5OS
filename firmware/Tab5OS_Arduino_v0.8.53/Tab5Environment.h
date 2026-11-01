#pragma once
#include <stddef.h>
#ifdef __cplusplus
#include <string>
#include <vector>
extern "C" {
#endif
/* Boot-session public values and explicit encrypted-secret references.
 * Copies UTF-8, never returns a borrowed pointer. >=0 byte length; negative
 * errno on error (and clears output). Names <=48, values <=512 bytes.
 * No libc environment, no SD persistence, no application isolation. */
int tab5_env_get(const char* name,char* output,size_t capacity);
int tab5_env_set(const char* name,const char* value);
int tab5_env_bind(const char* name,const char* secret_name);
int tab5_env_unset(const char* name);
int tab5_env_import_secrets(void);
/* Broker 107=get (name+NUL); 108=set/bind (a=0/1,name+NUL+value+NUL);
 * 109=unset (a=0,name+NUL), names (a=1,no input, newline separated names).
 * Accessible from MicroPython and ABI 3+ hardware(). */
int tab5_env_call(int operation,int a,const void* input,size_t length,void* output,size_t capacity);
#ifdef __cplusplus
}
namespace Tab5Environment {
std::vector<std::string> names();
void clear();
// A command containing any assignment, set/export or a credential option is
// never kept in terminal history or echoed. Explicit program output is allowed.
bool privateCommand(const char* command);
}
#endif
