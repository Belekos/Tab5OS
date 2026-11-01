#pragma once
#include "Tab5LaunchProfile.h"

namespace Tab5PythonArguments {
// The document is an argv value, never Python source or a shell fragment.
// No argument keeps the ordinary launch contract (argv contains only script).
inline bool copy(const char* argument,char* destination,size_t capacity){
 if(!destination||!capacity)return false;
 destination[0]=0;
 if(!argument||!*argument)return true;
 return tab5LaunchPath(argument,destination,capacity);
}
}
