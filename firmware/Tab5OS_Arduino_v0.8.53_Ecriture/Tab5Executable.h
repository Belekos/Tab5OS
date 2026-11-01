#pragma once
#include <stddef.h>
#include <stdint.h>
// UI task calls Run/Service/ClearOutput/ConsoleSnapshot. Stop is cooperative.
// Run accepts a request; SD reading/validation/relocation occur on the app task.
// A deferred load failure retains Status/SourcePath until displayed by the UI.
bool tab5ExecutableRun(const char* path,const char* argument=nullptr,const char* language="en");
void tab5ExecutableStop();
void tab5ExecutableService();
bool tab5ExecutableTakeLaunchError(); // UI-only one-shot, after Service
bool tab5ExecutableRunning();
bool tab5ExecutableBusy();
const char* tab5ExecutableStatus(); // stable localization key (translate at display time)
const char* tab5ExecutableSourcePath();
uint32_t tab5ExecutableRevision();
size_t tab5ExecutableCopyOutput(char* destination,size_t capacity);
const char* tab5ExecutableConsoleSnapshot(); // stable 8 KiB PSRAM cache, UI task only
void tab5ExecutableClearOutput();
