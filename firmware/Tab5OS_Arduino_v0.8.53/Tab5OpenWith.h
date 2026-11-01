#pragma once
#include <stdint.h>
#include <initializer_list>
#include <cstring>
#include <strings.h>

// An explicit attempt chosen by the user. Ordinary file associations remain
// unchanged; a native parser is responsible for reporting unsupported input.
enum class Tab5NativeFileApp : uint8_t { TextEditor, Reader, ImageViewer, Drawing, Audio, Video, Count };
namespace Tab5OpenWith {
inline const char* nativeName(Tab5NativeFileApp app){
 switch(app){
  case Tab5NativeFileApp::TextEditor:return "Éditeur";
  case Tab5NativeFileApp::Reader:return "Lecteur";
  case Tab5NativeFileApp::ImageViewer:return "Images";
  case Tab5NativeFileApp::Drawing:return "Dessin";
  case Tab5NativeFileApp::Audio:return "Audio";
  case Tab5NativeFileApp::Video:return "Vidéo";
  default:return "";
 }
}
inline Tab5NativeFileApp preferred(const char* path){
 const char* extension=path?strrchr(path,'.'):nullptr;
 if(!extension)return Tab5NativeFileApp::TextEditor;
 if(!strcasecmp(extension,".epub"))return Tab5NativeFileApp::Reader;
 for(const char* x:{".png",".jpg",".jpeg",".bmp",".gif"})if(!strcasecmp(extension,x))return Tab5NativeFileApp::ImageViewer;
 for(const char* x:{".mp3",".wav",".aac",".flac",".ogg",".m4a"})if(!strcasecmp(extension,x))return Tab5NativeFileApp::Audio;
 for(const char* x:{".mjpeg",".mjpg",".avi"})if(!strcasecmp(extension,x))return Tab5NativeFileApp::Video;
 return Tab5NativeFileApp::TextEditor;
}
}
