#include "Tab5FileSelection.h"
#include <cassert>
#include <iostream>
int main(){
 Tab5FileSelection selection;
 std::vector<std::string> rows={"/a","/b","/c","/d","/e"};
 selection.choose("/b",rows,false,false);assert(selection.paths.size()==1&&selection.contains("/b"));
 selection.choose("/d",rows,false,true);assert(selection.paths.size()==3&&selection.contains("/c"));
 selection.choose("/a",rows,false,true);assert(selection.paths.size()==2&&selection.contains("/a")&&!selection.contains("/c"));
 selection.choose("/e",rows,true,false);assert(selection.paths.size()==3);
 selection.choose("/e",rows,true,false);assert(selection.paths.size()==2);
 selection.choose("/d",rows,false,true,true);assert(selection.contains("/a")&&selection.contains("/e")&&selection.contains("/d"));
 // Paging invalidates the old visible-range anchor, not the saved selection.
 rows={"/f","/g"};selection.choose("/g",rows,true,true,true);assert(selection.contains("/a")&&selection.contains("/g"));
 selection.clear();selection.add("/");selection.add("/USB");assert(selection.paths.empty());
 selection.add("/folder/a");selection.add("/folder");selection.add("/folder.b");selection.add("/folder/a/b");selection.add("/USB/folder");
 auto roots=selection.roots();assert(roots.size()==3&&roots[0]=="/folder"&&roots[1]=="/folder.b"&&roots[2]=="/USB/folder");
 // More than one UI page, with no arbitrary seven/128-item selection limit.
 selection.clear();for(unsigned i=0;i<1007;++i)selection.add("/item"+std::to_string(i));
 assert(selection.paths.size()==1007);selection.add("/item9");assert(selection.paths.size()==1007);
 auto frozen=selection.roots();selection.clear();assert(frozen.size()==1007&&selection.paths.empty());
 std::cout<<"PASS: range reversal, Ctrl toggles, additive range, cross-page retention, root exclusion, recursive overlap, 1007 items, immutable batch snapshot\n";
}
