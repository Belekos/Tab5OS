#include "Tab5HiddenFiles.h"
#include <cassert>
#include <cstdio>
int main(){
 using namespace Tab5HiddenFiles;
 assert(!hidden("/book.epub"));assert(hidden("/.old"));assert(hidden("/old.bak"));
 assert(set("/book.epub",true));assert(hidden("/BOOK.EPUB"));
 assert(set("/.old",false));assert(!hidden("/.old"));
 ++sdGeneration;assert(hidden("/book.epub"));assert(!hidden("/.old"));
 assert(!hidden("/USB/book.epub"));assert(set("/USB/book.epub",true));
 assert(disk.count("/USB/.tab5-hidden.json"));++usbGeneration;assert(hidden("/USB/book.epub"));
 const auto saved=disk["/.tab5-hidden.json"];
 failWrite=true;assert(!set("/book.epub",false));assert(hidden("/book.epub"));assert(disk["/.tab5-hidden.json"]==saved);failWrite=false;
 assert(!set("/",true));assert(!set("/USB",true));assert(!set("/.tab5-hidden.json",false));assert(!set("/../escape",true));
 disk["/.tab5-hidden.json"]="{broken";++sdGeneration;assert(!set("/another",true));assert(disk["/.tab5-hidden.json"]=="{broken");
 disk["/.tab5-hidden.json"]=saved;++sdGeneration;blocked=true;assert(!set("/another",true));blocked=false;assert(set("/another",true));
 assert(hidden("/another"));assert(disk.count("/book.epub")==0); // Visibility never creates/renames user documents.
 sdMounted=false;++sdGeneration;assert(!set("/after-mount",true));sdMounted=true;assert(set("/after-mount",true));assert(hidden("/after-mount"));
 std::puts("PASS visibility persistence/reload, case normalization, dotfile overrides, independent SD/USB, failed write preserves preferences, corrupt JSON protected, transient storage contention recovers, no user rename");
}
