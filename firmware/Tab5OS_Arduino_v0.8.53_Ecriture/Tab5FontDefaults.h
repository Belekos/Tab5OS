#pragma once
#include <stddef.h>
#include <stdint.h>

// Font selections are applied at boot. Published LVGL fonts must remain alive
// until every widget/span using them has been destroyed.
namespace Tab5FontDefaults {
constexpr size_t MaxFontBytes=6u*1024u*1024u;
constexpr size_t MaxCustomBytes=8u*1024u*1024u;
constexpr size_t PathCapacity=261;
constexpr const char* StandardPath="/System/font.ttf";
enum class Result : uint8_t { Ok, InvalidPath, Unavailable, TooLarge, InvalidFont, ReadFailure, NoMemory, CombinedTooLarge };
enum class Role : uint8_t { Interface, Document };
struct Blob { uint8_t* data=nullptr; size_t size=0; };
// Absolute volume path, .ttf suffix, no control characters or traversal.
bool validPath(const char* path);
// Structural TrueType validation, before giving the buffer to TinyTTF.
// Collections/CFF/variable fonts are not admitted by this version.
bool validate(const uint8_t* bytes,size_t length);
// Read/validate a candidate without changing the existing setting or live font.
Result inspect(const char* path);
// After inspect(candidate), reject a pair whose two distinct custom blobs
// cannot fit at boot. An absent/invalid-path/oversized other file contributes
// zero because startup will use the standard font for that role.
Result checkSelectionBudget(const char* candidate,const char* other);
Result load(const char* path,Blob& destination,size_t budget=MaxFontBytes);
void release(Blob& blob);
const char* message(Result result);

class Sources {
 public:
  Sources()=default;
  ~Sources();
  Sources(const Sources&)=delete;
  Sources& operator=(const Sources&)=delete;
  // Call only during startup, before creating any LVGL font from these blobs.
  // Empty/standard paths reuse the caller's standard font, without allocation.
  void begin(const char* interfacePath,const char* documentPath);
  const Blob* custom(Role role) const;
  Result result(Role role) const {return results_[static_cast<unsigned>(role)];}
  size_t residentBytes() const {return blobs_[0].size+blobs_[1].size;}
  // Valid until reboot; begin refuses a second call to avoid dangling fonts.
 private:
  Blob blobs_[2];
  int8_t slots_[2]={-1,-1};
  Result results_[2]={Result::Ok,Result::Ok};
  bool started_=false;
};
}
