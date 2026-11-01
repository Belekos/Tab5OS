#!/usr/bin/env python3
"""Execute production WAV/header/finalization against File flush/write fault adapter."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
root=Path(__file__).resolve().parents[2];sketch=root/'Tab5OS_Arduino_v0.8.42_Ecriture';source=(sketch/'Tab5Audio.cpp').read_text()
def block(start):
 pos=source.index(start);brace=source.index('{',pos);depth=0
 for end in range(brace,len(source)):
  if source[end]=='{':depth+=1
  elif source[end]=='}':
   depth-=1
   if not depth:return source[pos:end+1]
 raise AssertionError(start)
methods='\n'.join(block(n) for n in ['static bool writeLe16(', 'static bool writeLe32(', 'static bool flushRecordingFile(', 'bool Tab5AudioEngine::writeWavHeader(', 'bool Tab5AudioEngine::writeRecordChunk(', 'void Tab5AudioEngine::finishRecording('])
fixture=r'''
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
struct String{std::string value;String()=default;String(const char* s):value(s){}size_t length()const{return value.size();}const char* c_str()const{return value.c_str();}bool operator==(const char* s)const{return value==s;}};
struct File{
 bool open=true,seekError=false,flushError=false,closeError=false;int failedWrite=-1,writeCalls=0;size_t pos=0;
 std::vector<uint8_t> data;explicit operator bool()const{return open;}
 bool seek(size_t p){if(seekError){errno=EIO;return false;}pos=p;return true;}
 size_t write(const uint8_t* bytes,size_t n){++writeCalls;if(writeCalls==failedWrite){errno=ENOSPC;return n-1;}if(data.size()<pos+n)data.resize(pos+n);memcpy(data.data()+pos,bytes,n);pos+=n;return n;}
 void flush(){if(flushError)errno=EIO;}
 int getWriteError()const{return 0;} // Arduino FS does not set Print::write_error for fsync.
 void close(){open=false;if(closeError)errno=EIO;}
};
struct {int removed=0;void remove(const char*){++removed;}} tab5FS;
struct Mic{bool isRecording(){return false;}void end(){}};
struct {Mic Mic;} M5;
struct {bool resume(){return true;}} Tab5AudioDevice;
void heap_caps_free(void* p){free(p);}uint32_t millis(){return 0;}void delay(int){assert(false);}
int faultCount=0,lastFault=0;void tab5StorageFault(const char*,const char*,int error){++faultCount;lastFault=error;}
String baseName(const String& s){return s;}
enum class Tab5AudioMode{Idle,Recording};
class Tab5AudioEngine{public:
 static constexpr size_t RECORD_SAMPLES=1024;static constexpr uint32_t RECORD_SAMPLE_RATE=16000;
 File _recordFile;int16_t (*_recordBuffer)[RECORD_SAMPLES]=nullptr;
 uint32_t _recordDataBytes=0;size_t _recordQueueIndex=0;uint8_t _recordPendingCount=0;uint16_t _recordLevel=0;bool _uiDirty=false;
 String _recordPath="/Recordings/new.wav",_lastRecordingPath="previous.wav",_currentLocalPath="previous.wav",status,now;
 Tab5AudioMode _mode=Tab5AudioMode::Recording;int reloads=0;
 Tab5AudioEngine(){_recordBuffer=static_cast<int16_t(*)[RECORD_SAMPLES]>(calloc(3,RECORD_SAMPLES*sizeof(int16_t)));}
 ~Tab5AudioEngine(){free(_recordBuffer);}
 bool writeWavHeader(File&,uint32_t);bool writeRecordChunk(size_t);void finishRecording(bool);
 void setStatus(const String& s){status=s;}void setNowPlaying(const String& s){now=s;}void reloadLibrary(){++reloads;}
};
// METHODS
int main(){
 // Exact RIFF bytes preserved, and every short header write is observable.
 Tab5AudioEngine header;File good;assert(header.writeWavHeader(good,2048));assert(good.data.size()==44);assert(!memcmp(good.data.data(),"RIFF",4));assert(!memcmp(good.data.data()+36,"data",4));assert(good.data[40]==0&&good.data[41]==8);
 for(int fail=1;fail<=13;++fail){File shortHeader;shortHeader.failedWrite=fail;assert(!header.writeWavHeader(shortHeader,2048));}
 File seek;seek.seekError=true;assert(!header.writeWavHeader(seek,1));
 // A stale unrelated errno must not reject a genuinely successful flush.
 {File f;errno=ENOENT;assert(flushRecordingFile(f));}
 {Tab5AudioEngine e;e._recordDataBytes=131072-2048;e._recordFile.flushError=true;assert(!e.writeRecordChunk(0));assert(errno==EIO);}
 for(int fault=0;fault<5;++fault){
  Tab5AudioEngine e;tab5FS.removed=0;faultCount=lastFault=0;
  if(fault==1)e._recordFile.flushError=true;
  if(fault==2)e._recordFile.closeError=true;
  if(fault==3)e._recordFile.failedWrite=1;
  if(fault==4){e._recordPendingCount=1;e._recordFile.failedWrite=1;}
  e.finishRecording(true);assert(!e._recordBuffer);assert(!e._recordFile.open);assert(e._mode==Tab5AudioMode::Idle);assert(tab5FS.removed==0);
  if(!fault){assert(e.status=="Enregistrement sauvegardé");assert(e._lastRecordingPath=="/Recordings/new.wav");assert(e.reloads==1);assert(!faultCount);}
  else{assert(e.status=="Erreur d’écriture sur la carte SD");assert(e._lastRecordingPath=="previous.wav");assert(e._currentLocalPath=="previous.wav");assert(e.reloads==0);assert(faultCount==1&&lastFault!=0);}
 }
 {Tab5AudioEngine e;tab5FS.removed=0;e.finishRecording(false);assert(tab5FS.removed==1);assert(e.status=="Enregistrement abandonné");assert(e._lastRecordingPath=="previous.wav");}
 std::cout<<"{\"passed\":true,\"shortHeaderWrites\":13,\"finalizationFailureCases\":4,\"periodicFlushEio\":true,\"staleErrnoCleared\":true,\"failedSaveKeepsPartialFile\":true,\"discardUnchanged\":true}"<<std::endl;
}
'''.replace('// METHODS',methods)
# M5 API uses an identically named member/type; use an unambiguous fixture type.
fixture=fixture.replace('struct Mic{','struct FakeMic{').replace('struct {Mic Mic;}','struct {FakeMic Mic;}')
build=Path(tempfile.mkdtemp(prefix='tab5-recording-0842-'));(build/'test.cpp').write_text(fixture)
subprocess.run(['g++','-std=c++17','-O1','-g','-Wall','-Wextra','-Werror','-fno-pie','-no-pie','-fno-omit-frame-pointer','-fsanitize=address,undefined',str(build/'test.cpp'),'-o',str(build/'test')],check=True)
run=subprocess.run([str(build/'test')],capture_output=True,text=True,timeout=30,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1'));print(run.stdout+run.stderr,end='');run.check_returncode()
report={'version':'0.8.42',**json.loads(run.stdout.strip()),'sourceSha256':hashlib.sha256(source.encode()).hexdigest(),'scope':'Exact production WAV/header helpers, writeRecordChunk and finishRecording with File/Mic/driver adapter. Short writes, fflush/fsync-like errno without Print write_error, close EIO, final pending PCM failure. ASan/UBSan; no hardware Mic/FAT execution.'}
(root/'tools/audit-0842/recording.json').write_text(json.dumps(report,indent=2)+'\n')
