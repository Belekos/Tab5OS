#pragma once

// Small, checksummed RTC breadcrumbs survive supported warm resets. They are
// the last observed work phases, NEVER proof of the cause of a restart.
#ifdef __cplusplus
extern "C" {
#endif
// Slots: 0 boot, 1 Python, 2 printing, 3 SD, 4 video, 5 Pentest, 6 vault, 7 Git.
// Strings are copied immediately. Avoid credentials or target addresses.
// active=0 ends that activity; unchanged marks do not rewrite RTC memory.
// Vault and Git phases are constant tokens only; their detail argument is discarded.
// An interactive vault operation stays active until the UI handoff finishes,
// not just until its worker returns. These marks never write to the SD card.
void tab5DiagnosticMark(unsigned slot, const char* phase, const char* detail, int active);
#ifdef __cplusplus
}
// Call once early in setup(), before recording the new boot stages.
void tab5DiagnosticsBegin();
// Immutable for this boot. Empty on power-on, brownout, invalid RTC record,
// or when no activity was still in progress. Safe to display from the UI.
const char* tab5DiagnosticPrevious();
#endif
