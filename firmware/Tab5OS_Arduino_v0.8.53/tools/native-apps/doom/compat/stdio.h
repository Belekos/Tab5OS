#pragma once
#include_next <stdio.h>
#undef stdin
#undef stdout
#undef stderr
#define stdin ((FILE*)1)
#define stdout ((FILE*)2)
#define stderr ((FILE*)3)
