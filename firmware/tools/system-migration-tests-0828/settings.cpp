#include "Arduino.h"
#include "FS.h"
#include "Tab5SystemMigration.h"
#include <algorithm>
#include <utility>
#include <cstdio>
#include <cstdlib>
static unsigned checks=0;
static void check(bool v,const char* why){++checks;if(!v){fprintf(stderr,"FAIL %s\n",why);exit(1);}}
template<class T,class A,class B>T constrain(T value,A a,B b){return std::max(T(a),std::min(value,T(b)));}
namespace Tab5Mouse {struct Config {unsigned speed=100,scroll=3;bool leftHanded=false,natural=false;};}
/* SETTINGS STRUCT */
static AppSettings settings;
static String lastReaderPath="/old-reader.epub";
namespace Tab5Files {static bool showHidden=false;}
static fs::FS SD;static fs::FS& tab5FS=SD;
static bool sdReady=false,systemMigrationFailed=false,systemMigrationSettingsPending=true,settingsReadComplete=false;
static bool settingsDirty=false;
static uint32_t lastSettingsChangeMs=0;
static uint32_t millis(){return 1234;}
static void delay(int){}
static void tab5StorageFault(const char*,const char*,int=0){}
static struct {template<class...Args>void printf(const char*,Args...){}}Tab5DebugSerial;
static constexpr const char* SETTINGS_FILE="/System/settings.ini";
static constexpr uint8_t SETTINGS_FORMAT_VERSION=2;
static constexpr unsigned THEME_COUNT=21,TIME_ZONE_COUNT=16;
static bool themesReadable=true;
static bool loadCustomThemes(){return themesReadable;}
namespace Tab5MediaSession {
static uint8_t current=150;
static bool known=false;
static bool seedLegacyVolume(uint8_t v,uint32_t){if(known)return false;known=true;current=v;return true;}
static uint8_t volume(){return current;}
}
static struct {uint8_t last=150;unsigned calls=0;void setVolume(uint8_t v){last=v;++calls;}}Tab5Audio;
/* CONTROLLERS */
static const char* oldSettings="settings_version=2\naudio_volume=23\nbrightness=215\ntheme=6\nshow_hidden=1\ndocuments_root=/Documents\nlast_reader=/Livres/Résilience.epub\n";
static void reset(){SD=fs::FS{};settings=AppSettings{};lastReaderPath="/old-reader.epub";Tab5Files::showHidden=false;settingsDirty=false;lastSettingsChangeMs=0;sdReady=true;systemMigrationFailed=false;systemMigrationSettingsPending=true;settingsReadComplete=false;Tab5MediaSession::known=false;Tab5MediaSession::current=150;Tab5Audio.last=150;Tab5Audio.calls=0;themesReadable=true;}
static void activeUnchanged(){check(settings.audioVolume==150&&settings.brightness==190&&settings.themeIndex==0&&settings.documentsRoot=="/"&&lastReaderPath=="/old-reader.epub"&&!Tab5Files::showHidden,"incomplete read does not mutate any active setting");}
static void loaded(){check(settings.audioVolume==23&&settings.brightness==215&&settings.themeIndex==6&&settings.documentsRoot=="/Documents"&&lastReaderPath=="/Livres/Résilience.epub"&&Tab5Files::showHidden,"all settings committed together");check(settingsReadComplete&&!systemMigrationSettingsPending,"complete read unlocks settings writes");}
int main(){
 reset();sdReady=false;const auto empty=SD.data();check(!saveSettings()&&SD.data()==empty,"no SD cannot save default settings");
 sdReady=true;SD.seedDir("/System");check(!saveSettings(),"pending unread settings block save even when SD mounted");loadSettings();
 check(settingsReadComplete&&!systemMigrationSettingsPending,"confirmed absent settings permits fresh defaults");
 check(saveSettings()&&SD.tree.count(SETTINGS_FILE),"fresh defaults saved after absence confirmed");
 reset();SD.seedFile(SETTINGS_FILE,oldSettings);loadSettings();loaded();
 reset();SD.seedFile(std::string(SETTINGS_FILE)+".bak",oldSettings);loadSettings();loaded();
 // Interrupted read at every byte, including after several valid lines: no
 // earlier parsed line becomes live, and saveSettings cannot replace the file.
 for(size_t byte=0;byte<std::strlen(oldSettings);++byte){
  reset();SD.seedFile(SETTINGS_FILE,oldSettings);SD.failReadAfter=byte;auto before=SD.data();loadSettings();
  check(!settingsReadComplete&&systemMigrationSettingsPending,"partial read retains barrier");activeUnchanged();
  check(!settingsDirty&&!saveSettings()&&SD.data()==before,"partial read cannot schedule or execute destructive defaults save");
  SD.clearFaults();check(reloadSettingsAfterMigration(),"recovery retries complete read");loaded();
  check(Tab5Audio.last==23&&Tab5Audio.calls==1&&!settingsDirty,"legacy volume restored only after valid recovery");
 }
 for(unsigned failure=1;failure<=2;++failure){
  reset();SD.seedFile(std::string(SETTINGS_FILE)+".bak",oldSettings);SD.failAt=failure;loadSettings();
  check(!settingsReadComplete&&systemMigrationSettingsPending,"probe errors never imply file absent");activeUnchanged();
 }
 reset();SD.seedFile(SETTINGS_FILE,oldSettings);SD.failAt=2;loadSettings();check(!settingsReadComplete&&!saveSettings(),"open failure keeps original protected");activeUnchanged();
 for(const auto& payload:std::vector<std::string>{std::string("audio_volume=23\n")+std::string(600,'x'),std::string(65537,'x'),std::string("brightness=200\naudio_volume=2\0x\n",32)}){
  reset();SD.seedFile(SETTINGS_FILE,payload);auto before=SD.data();loadSettings();check(!settingsReadComplete&&!saveSettings()&&SD.data()==before,"overlong/malformed input preserved without partial commit");activeUnchanged();
 }
 reset();SD.seedDir(SETTINGS_FILE);loadSettings();check(!settingsReadComplete&&!saveSettings(),"directory masquerading as settings refused");
 reset();SD.seedFile(SETTINGS_FILE,oldSettings);SD.positionError=true;loadSettings();check(!settingsReadComplete&&!saveSettings(),"invalid file position cannot certify complete read");activeUnchanged();
 reset();SD.seedFile(SETTINGS_FILE,oldSettings);SD.sizeError=true;loadSettings();check(!settingsReadComplete&&!saveSettings(),"invalid file size cannot certify complete read");activeUnchanged();
 reset();SD.seedFile(SETTINGS_FILE,"settings_version=1\naudio_volume=0\n");loadSettings();check(settingsReadComplete&&settings.audioVolume==0&&settingsDirty,"legacy muted volume remains zero");
 reset();SD.seedFile("/M5Writer/settings.ini",oldSettings);SD.seedFile("/System/settings.ini","settings_version=2\naudio_volume=150\n");auto before=SD.data();
 SD.failAt=1;sdReady=migrateSystemDirectory();check(!sdReady&&systemMigrationFailed&&systemMigrationSettingsPending&&!saveSettings()&&SD.data()==before,"migration failure prevents fallback settings writes");
 SD.clearFaults();sdReady=migrateSystemDirectory();check(sdReady&&reloadSettingsAfterMigration(),"migration and settings recover together");loaded();check(SD.data()==before,"recovery retains old and distributed settings payloads");
 reset();SD.seedFile(SETTINGS_FILE,oldSettings);Tab5MediaSession::current=91;Tab5MediaSession::known=true;
 check(reloadSettingsAfterMigration()&&settings.audioVolume==91&&Tab5Audio.last==91,"saved NVS or deliberate user volume takes priority over legacy file");
 reset();SD.seedFile(SETTINGS_FILE,oldSettings);SD.seedFile("/System/themes.json","theme data");themesReadable=false;auto themeData=SD.data();
 check(!reloadSettingsAfterMigration()&&systemMigrationSettingsPending&&!saveSettings()&&SD.data()==themeData,"unreadable existing themes cannot clamp and overwrite personal theme choice");activeUnchanged();
 themesReadable=true;check(reloadSettingsAfterMigration(),"themes and settings retry together");loaded();
 reset();SD.seedFile(SETTINGS_FILE,oldSettings);themesReadable=false;check(reloadSettingsAfterMigration(),"absent custom themes is legitimate");loaded();
 printf("PASS %u production settings/migration/recovery controller checks; every settings byte interrupted then resumed\n",checks);
}
