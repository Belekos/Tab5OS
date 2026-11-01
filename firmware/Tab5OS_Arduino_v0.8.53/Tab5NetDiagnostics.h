#pragma once

#include <Arduino.h>

enum class Tab5RadioNetworkStage : uint8_t {
  None,
  HttpOpen,
  Buffering,
  DecoderStart,
  Playing,
};

enum class Tab5VideoNetworkStage : uint8_t {
  None,
  HttpOpen,
  Streaming,
};

// Conserve dans la memoire RTC l'operation reseau en cours. Si le P4
// redemarre, le prochain boot peut ainsi dire si le reset a eu lieu pendant
// une requete Cartes, une etape WebRadio ou un flux video.
void tab5NetDiagnosticsBegin();
void tab5NetDiagnosticsService();

void tab5NetDiagnosticsMapBegin();
void tab5NetDiagnosticsMapEnd();
void tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage stage);
void tab5NetDiagnosticsRadioEnd();
void tab5NetDiagnosticsVideoStage(Tab5VideoNetworkStage stage);
void tab5NetDiagnosticsVideoEnd();

