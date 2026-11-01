#include <Arduino.h>
#include <AudioGeneratorAAC.h>
#include <libhelix-aac/aaccommon.h>
#ifdef AAC_ENABLE_SBR
#include <libhelix-aac/sbr.h>
#endif

// Keep Helix's SBR private declarations separate from coder.h, which defines
// different private HuffInfo types under the same name.
size_t tab5AacSbrWorkspaceBytes() {
#ifdef AAC_ENABLE_SBR
  return (sizeof(PSInfoSBR) + 7U) & ~7U;
#else
  return 0;
#endif
}
