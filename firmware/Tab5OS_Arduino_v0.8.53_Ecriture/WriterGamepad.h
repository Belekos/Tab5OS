#pragma once
#include <cstddef>
#include <cstdint>
namespace writing {
struct GamepadReport {uint32_t buttons=0;uint8_t directions=0;bool help=false;int16_t axes[4]{};uint8_t axisMask=0;};
// Wired XInput (Xbox 360) packet, separate from USB HID report descriptors.
bool decodeXInputGamepad(const uint8_t*,size_t,GamepadReport&);
// Old SN30/SF30 Pro USB HID firmware sends a nine-byte packet without an ID.
// Use only for the matching USB devices; the first byte contains buttons,
// so it can accidentally equal a modern Report ID.
bool decodeLegacy8BitDoGamepad(uint16_t vendor,uint16_t product,const uint8_t*,size_t,GamepadReport&);
class HidGamepadDescriptor {
 public:
 bool parse(const uint8_t*,size_t);
 bool decode(const uint8_t*,size_t,GamepadReport&);
 bool matchesReportSize(const uint8_t*,size_t)const;
 bool gamepad()const{return gamepad_;}
 bool help()const{return help_;}
 private:
 struct Field{uint16_t offset,kind;uint8_t size,id;int32_t minimum,maximum;};
 Field fields_[64]{};unsigned count_=0,slotCount_=0;
 uint16_t lengths_[256]{};uint8_t slots_[256]{};
 GamepadReport reports_[8]{};bool ids_=false,gamepad_=false,help_=false;
};
struct GamepadInfo {uint32_t identity=0,buttons=0,reports=0,rejected=0,errors=0,restarts=0;uint16_t vendor=0,product=0;bool xinput=false;uint8_t directions=0;uint8_t mapping[8]={1,2,9,10,3,4,5,6};int16_t axes[4]{};uint8_t axisMask=0,interfaceNumber=0,rawSize=0,raw[16]{};};
bool usbGamepadInfo(GamepadInfo&);
bool usbGamepadMap(uint32_t identity,const uint8_t mapping[8]);
}
