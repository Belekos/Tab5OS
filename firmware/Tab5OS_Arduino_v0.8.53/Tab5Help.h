#pragma once
#include <stddef.h>

// Returned strings follow the selected SD language pack. Source strings are
// French fallbacks; user documents and filenames are never passed here.
size_t tab5HelpCount();
const char* tab5HelpTitle(size_t index);
const char* tab5HelpText(size_t index);
