#include "Tab5SystemMigration.h"
#include <cstdio>
#include <cstdlib>
using namespace Tab5SystemMigration;
static unsigned checks=0,yields=0;
static void check(bool value,const char* why){++checks;if(!value){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
static void cooperate(){++yields;}
static fs::FS fixture(){
 fs::FS f;
 f.seedFile("/M5Writer/settings.ini","old settings");f.seedFile("/System/settings.ini","new settings");
 f.seedFile("/M5Writer/themes.json","old themes");f.seedFile("/System/themes.json","new themes");
 f.seedFile("/M5Writer/maps.ini","old maps");f.seedFile("/System/maps.ini","new maps");
 f.seedFile("/M5Writer/keyboards.json","old keyboards");f.seedFile("/System/keyboards.json","new keyboards");
 f.seedFile("/M5Writer/languages/fr-FR.json","old language");f.seedFile("/System/languages/fr-FR.json","new language");
 f.seedFile("/M5Writer/languages/custom.json","custom language");
 f.seedFile("/M5Writer/fonts/custom.ttf","custom font");f.seedFile("/System/fonts/builtin.ttf","new builtin font");
 f.seedFile("/M5Writer/font.ttf","old main font");f.seedFile("/System/font.ttf","new main font");
 f.seedFile("/M5Writer/ime/dict.bin","old ime");f.seedFile("/System/ime/dict.bin","new ime");
 f.seedFile("/M5Writer/favorites.json","old favorites");f.seedFile("/System/favorites.json/new.dat","new file-dir mismatch");
 f.seedFile("/M5Writer/state/book.dat","old bookmark");f.seedFile("/System/state","new dir-file mismatch");
 f.seedFile("/M5Writer/languages/type-mismatch","old resource file");f.seedFile("/System/languages/type-mismatch/data","new resource directory");
 f.seedFile("/M5Writer/maps/cache/custom/5/4/3.png","old tile");f.seedDir("/System/maps/cache");
 return f;
}
static void verifyActive(const fs::FS& f){
 auto value=[&](const char* p,const char* v){auto it=f.tree.find(p);check(it!=f.tree.end()&&!it->second.directory&&it->second.contents==v,p);};
 value("/System/settings.ini","old settings");value("/System/themes.json","old themes");
 value("/System/maps.ini","old maps");value("/System/keyboards.json","old keyboards");
 value("/System/languages/fr-FR.json","new language");value("/System/languages/custom.json","custom language");
 value("/System/font.ttf","new main font");value("/System/ime/dict.bin","new ime");
 value("/System/favorites.json","old favorites");value("/System/state/book.dat","old bookmark");
 value("/System/languages/type-mismatch/data","new resource directory");
 value("/System/maps/cache/custom/5/4/3.png","old tile");
 check(!f.tree.count("/M5Writer")&&!f.tree.count("/System/migration_0824/pending"),"no active migration after success");
}
int main(){
 fs::FS empty;auto r=run(empty,cooperate);check(r.status==Status::NotNeeded&&empty.mutations==0,"fresh SD unchanged");
 fs::FS simple;simple.seedFile("/M5Writer/settings.ini","settings");simple.seedFile("/M5Writer/maps/cache/1/tile","tile");
 auto before=simple.data();r=run(simple,cooperate);check(r.status==Status::Renamed&&simple.mutations==1&&simple.enums==0,"single old root renamed without creating System first");
 check(simple.data()==before&&simple.tree.count("/System/settings.ini"),"single rename preserves all content");
 auto initial=fixture();before=initial.data();auto complete=initial;r=run(complete,cooperate);
 check(r.status==Status::Merged&&yields>0,"dual-root merge completes cooperatively");verifyActive(complete);check(complete.data()==before,"all old and new file data preserved exactly");
 check(complete.handles==0&&complete.maxHandles<=3,"bounded descriptors and no retained open handles");
 const auto mutations=complete.mutations,operations=complete.operations;
 const auto done=complete.data();complete.clearFaults();r=run(complete,cooperate);
 check(r.status==Status::NotNeeded&&complete.mutations==0&&complete.enums==0&&complete.operations==2&&complete.data()==done,"later boot skips backup tree completely");
 // Fault every FS boundary before its operation, then reboot and resume.
 for(unsigned at=1;at<=operations;++at){
  auto f=initial;f.failAt=at;auto result=run(f,cooperate);
  check(!result.ok(),"I/O fault cannot be reported as completed");check(f.data()==before,"I/O fault never loses any old/new contents");
  check(f.handles==0&&f.maxHandles<=3,"I/O failure closes all descriptors");
  f.clearFaults();result=run(f,cooperate);check(result.ok(),"I/O fault resumes automatically");verifyActive(f);check(f.data()==before,"resumed contents preserved");
 }
 // Abrupt interruption after every successful rename/mkdir, including the
 // critical gap between backing up destination and activating legacy data.
 for(unsigned after=1;after<=mutations;++after){
  auto f=initial;f.cutAfter=after;bool cut=false;try{run(f,cooperate);}catch(fs::PowerCut&){cut=true;}
  check(cut&&f.data()==before,"cut after metadata mutation preserves file contents");
  f.clearFaults();r=run(f,cooperate);check(r.ok(),"reboot resumes interrupted metadata sequence");verifyActive(f);check(f.data()==before,"resumed power-cut content set complete");
 }
 fs::FS large;for(unsigned i=0;i<10000;++i)large.seedFile("/M5Writer/maps/cache/osm/19/120/"+std::to_string(i)+".png",std::to_string(i));large.seedDir("/System");
 r=run(large,cooperate);check(r.ok()&&large.enums<10&&large.mutations<10,"ten thousand tiles moved as single subtree without enumeration");
 check(large.tree.count("/System/maps/cache/osm/19/120/9999.png"),"large cache installed");
 auto denied=initial;denied.denyAll=true;r=run(denied);check(!r.ok()&&denied.mutations==0&&denied.data()==before,"refused SD aborts without mutations");
 auto reserved=initial;reserved.seedFile("/M5Writer/migration_0824/user.txt","reserved user data");auto reservedData=reserved.data();r=run(reserved);
 check(r.error==Error::ReservedPath&&reserved.mutations==0&&reserved.data()==reservedData,"reserved ancestor collision refused before staging");
 auto occupied=initial;occupied.seedFile("/System/migration_0824/pending/legacy/saved","pending prior migration");auto occupiedData=occupied.data();r=run(occupied);
 check(r.error==Error::BackupCollision&&occupied.data()==occupiedData,"second old root never overwrites interrupted migration");
 fs::FS deep;std::string path="/M5Writer",target="/System";for(unsigned i=0;i<35;++i){path+="/d";target+="/d";deep.seedDir(target);}deep.seedFile(path+"/valuable","deep value");auto deepData=deep.data();r=run(deep);
 check(r.error==Error::DepthLimit&&deep.data()==deepData,"excess depth stops safely without deleting data");
 check(deep.handles==0&&deep.maxHandles<=3,"deep tree never exhausts SD descriptors");
 fs::FS longPath;path="/M5Writer";target="/System";for(unsigned i=0;i<7;++i){path+="/"+std::string(150,'a'+i);target+="/"+std::string(150,'a'+i);longPath.seedDir(target);}longPath.seedFile(path+"/valuable","long value");auto longData=longPath.data();r=run(longPath);
 check(r.error==Error::PathTooLong&&longPath.data()==longData,"oversized path fails without deletion or truncation");
 printf("PASS %u production migration checks; %u injected I/O boundaries, %u mutation cut points; 10000-tile subtree optimization\n",checks,operations,mutations);
}
