#include "Tab5NetDiagnostics.cpp"
#include <cassert>
#include <thread>

int main() {
  rtcNetworkState.magic = 0;
  rtcNetworkState.flags = UINT32_MAX;
  tab5NetDiagnosticsBegin();
  assert(loadFlags() == 0);
  assert(Tab5DebugSerial.text.find("aucune operation") != std::string::npos);
  Tab5DebugSerial.text.clear();
  tab5NetDiagnosticsMapBegin();
  tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage::Playing);
  tab5NetDiagnosticsVideoStage(Tab5VideoNetworkStage::HttpOpen);
  assert(loadFlags() == (NET_DIAG_MAP | NET_DIAG_RADIO_PLAYING | NET_DIAG_VIDEO_HTTP));
  tab5NetDiagnosticsVideoStage(Tab5VideoNetworkStage::Streaming);
  assert(loadFlags() == (NET_DIAG_MAP | NET_DIAG_RADIO_PLAYING | NET_DIAG_VIDEO_STREAM));
  // Simulate warm restart: same RTC record; begin reports and clears old state.
  tab5NetDiagnosticsBegin();
  assert(loadFlags() == 0);
  assert(Tab5DebugSerial.text.find("requete Cartes + lecture WebRadio + lecture flux video") != std::string::npos);
  Tab5DebugSerial.text.clear();
  tab5NetDiagnosticsVideoStage(Tab5VideoNetworkStage::HttpOpen);
  tab5NetDiagnosticsBegin();
  assert(Tab5DebugSerial.text.find("connexion HTTP/TLS video") != std::string::npos);
  for (unsigned i = 0; i < 1000; ++i) {
    tab5NetDiagnosticsMapBegin();
    tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage::HttpOpen);
    tab5NetDiagnosticsVideoEnd();
    assert(loadFlags() == (NET_DIAG_MAP | NET_DIAG_RADIO_HTTP));
    tab5NetDiagnosticsVideoStage(Tab5VideoNetworkStage::Streaming);
    tab5NetDiagnosticsRadioEnd();
    assert(loadFlags() == (NET_DIAG_MAP | NET_DIAG_VIDEO_STREAM));
    tab5NetDiagnosticsMapEnd();
    tab5NetDiagnosticsVideoEnd();
    assert(loadFlags() == 0);
  }
  auto map=[] { for(unsigned i=0;i<10000;++i) {tab5NetDiagnosticsMapBegin();tab5NetDiagnosticsMapEnd();} };
  auto radio=[] { for(unsigned i=0;i<10000;++i) {tab5NetDiagnosticsRadioStage(Tab5RadioNetworkStage::Playing);tab5NetDiagnosticsRadioEnd();} };
  auto video=[] { for(unsigned i=0;i<10000;++i) {tab5NetDiagnosticsVideoStage(Tab5VideoNetworkStage::Streaming);tab5NetDiagnosticsVideoEnd();} };
  std::thread a(map), b(radio), c(video);a.join();b.join();c.join();
  assert(loadFlags() == 0 && sizeof(rtcNetworkState) == 8);
  std::puts("PASS: RTC video/map/radio attribution, 1000 independent context transitions, 30000 concurrent stage/clear cycles, retained record remains 8 bytes");
}
