#pragma once
#include <cstddef>
#include <cstdint>
namespace Tab5PythonServices {
int call(int op,int a,int b,int c,const void* input,size_t length,void* output,size_t capacity);
void reset();
bool storageIdle();

// Catalogue scan owner only (worker, or UI fallback). Never recurse into libraries.
// The caller owns each descriptor; LVGL event data must outlive its card.
enum class AppIcon : uint8_t { Python, Workshop, Libraries, Map, Audio, Theme, Storage, Demo };
struct AppInfo { char path[272]{}; char title[96]{}; AppIcon icon=AppIcon::Python; };
constexpr unsigned MaxApps=32;
bool beginAppScan();
// 1 = app, 0 = continue next UI tick, 2 = complete, 3 = bounded limit, <0 = errno.
int nextApp(AppInfo& app);
void endAppScan();
}
