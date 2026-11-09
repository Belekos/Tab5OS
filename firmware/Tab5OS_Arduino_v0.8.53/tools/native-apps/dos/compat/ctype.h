#pragma once
static inline int isalnum(int c){return (c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z');}
static inline int toupper(int c){return c>='a'&&c<='z'?c-'a'+'A':c;}
static inline int tolower(int c){return c>='A'&&c<='Z'?c-'A'+'a':c;}
