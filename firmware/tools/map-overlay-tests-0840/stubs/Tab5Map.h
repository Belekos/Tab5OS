#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
enum class Tab5MapMarkerKind:uint8_t{Saved,Mesh,Search};
struct Tab5MapMarker{char name[96]{};double latitude=0,longitude=0;uint32_t node=0,timestamp=0;Tab5MapMarkerKind kind=Tab5MapMarkerKind::Saved;};
struct MockMap{std::vector<Tab5MapMarker> markers;uint32_t revision=1;mutable unsigned reads=0;size_t markerCount()const{return markers.size();}bool marker(size_t i,Tab5MapMarker& out)const{++reads;if(i>=markers.size())return false;out=markers[i];return true;}uint32_t markerRevision()const{return revision;}};
extern MockMap Tab5Map;
