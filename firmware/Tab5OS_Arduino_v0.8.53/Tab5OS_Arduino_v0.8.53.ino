#include "Tab5Alarm.h"
#include "Tab5Terminal.h"
#include "Tab5Sdr.h"
#include "Tab5Meshtastic.h"
#include "Tab5MeshBadge.h"
#include "Tab5MapOverlay.h"
#include "Tab5UsbStorage.h"
#include "Tab5UsbPrinter.h"
#include "Tab5SystemInfoView.h"
#include "Tab5MemoryLayout.h"
#include "Tab5FirmwareFootprint.h"
#include "Tab5MapWheel.h"
#include <esp_ota_ops.h>
#include "Tab5VolumeFS.h"
#include "Tab5FilePicker.h"
#include "Tab5Print.h"
#include "Tab5Vault.h"
#include "Tab5VaultPrompt.h"
#include "Tab5KeyStore.h"
#include "Tab5CryptoRandom.h"
#include "Tab5Firmware.h"
#include "Tab5Executable.h"
#include "Tab5NativeView.h"
#include "Tab5Python.h"
#include "Tab5AppCatalogue.h"
#include "Tab5ShellLayout.h"
#include "Tab5Serial.h"
#include "WriterDocument.h"
#include "Tab5FileShare.h"
#include "Tab5DisplayPresent.h"
#include "Tab5DisplaySignals.h"
#include "Tab5DisplayActivity.h"
#include "Tab5DisplayRefresh.h"
#include "Tab5UiDiagnostics.h"
#include "Tab5Storage.h"
#include "Tab5SystemResources.h"
#include "Tab5SystemMigration.h"
#include "Tab5ImageLock.h"
#include "Tab5Json.h"
#include "Tab5PythonUI.h"
#include "Tab5PythonReader.h"
#include "Tab5PythonServices.h"
#include <Arduino.h>
#include <esp_arduino_version.h>

// Filesystem headers MUST be included before M5Unified/M5GFX.
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <M5GFX.h>
#include <algorithm>
#include <vector>
#include <math.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#if __has_include(<esp_sntp.h>)
#include <esp_sntp.h>
#define TAB5OS_HAS_SNTP_CALLBACK 1
#else
#define TAB5OS_HAS_SNTP_CALLBACK 0
#endif
#include <M5Unified.h>
#include "Tab5Boot.h"
#include "Tab5Diagnostics.h"
#include "Tab5DisplayCache.h"
#include "Tab5DisplayBandwidth.h"
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include "EpubLite.h"
#include "EpubPageIndex.h"
#include "EpubRender.h"
#include "Tab5Audio.h"
#include "Tab5MediaSession.h"
#include "Tab5MediaList.h"
#include "Tab5AudioDriver.h"
#include "Tab5Favorites.h"
#include <misc/cache/instance/lv_image_cache.h>
#include "Tab5Video.h"
#include "Tab5Map.h"
#include "Tab5MapImportPlan.h"
#include "Tab5Jpeg.h"
#include "Tab5MapPrint.h"
#include "Tab5TileRaster.h"
#include "Tab5Tools.h"
#include "Tab5GitView.h"
#include "Tab5Git.h"
#include "Tab5ContextMenu.h"
#include "Tab5Terminal.h"
#include "Tab5FileApplications.h"
#include "Tab5ImageEditor.h"
#include "Tab5ImageFormats.h"
#include "Tab5WebPPort.h"
#include "tools/native-apps/image-editor/image_core.h"
#include "Tab5Icons.h"
#include "Tab5OSTypes.h"
#include "Tab5KeyboardLayouts.h"
#include "Tab5Ime.h"
#include "Tab5ImeDictionary.h"
#include "Tab5CjkFonts.h"
#include "Tab5DisplayOrientation.h"
#include "Tab5FontDefaults.h"
#include "Tab5Localization.h"
#include "Tab5Help.h"
#include "Tab5ThemeLive.h"
#include "Tab5Appearance.h"
#include "Tab5UiStyle.h"
#include "Tab5DocumentText.h"
#include "Tab5ThemeSchema.h"
#include "Tab5ThemeChoices.h"
#include "Tab5TopBarLayout.h"
#include "Tab5NetDiagnostics.h"
#include "Tab5Memory.h"
#include "Tab5ResourceManager.h"
#include "Tab5TlsMemory.h"
#include "Tab5PathLabel.h"
#include "Tab5EnergyPolicy.h"
#include "Tab5BatteryGuard.h"
#include "Tab5Compatibility.h"
#include "Tab5NetworkGate.h"
#include "Tab5Writing.h"
#include "WriterUsb.h"
#include "WriterGamepadNavigation.h"
#include "Tab5NumericKeyboard.h"
#include "Tab5Mouse.h"
#include "Tab5ContextMenu.h"
#include "Tab5TextInput.h"
#include "Tab5Focus.h"
#include "Tab5FilePolicy.h"
#include "Tab5FileSearch.h"
#include "Tab5FolderPreview.h"
#include "Tab5ExplorerTree.h"
#include "Tab5FileOperation.h"
#include "Tab5FileSelection.h"
#include "Tab5Search.h"
#include <new>

// Resolve all Arduino libraries before scanning the large embedded source tree.
#include <Preferences.h>
#include "Tab5AtomicFile.h"
#include <SPIFFS.h>
#include <NetworkClientSecure.h>
#include <NetworkClient.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>

#if LVGL_VERSION_MAJOR != 9
#error "Tab5OS Arduino requires LVGL 9.x"
#endif

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#if !defined(ESP_ARDUINO_VERSION) || !defined(ESP_ARDUINO_VERSION_VAL)
#error "Arduino-ESP32 version macros are missing"
#elif ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 3, 9)
#error "Tab5OS requires M5Stack/Arduino-ESP32 3.3.9 or later: 3.3.8 has broken ESP32-P4 VO4 LDO ownership for the microSD SPI pins"
#endif
#if !defined(M5UNIFIED_VERSION_MAJOR) || !defined(M5UNIFIED_VERSION_MINOR) || \
    !defined(M5UNIFIED_VERSION_PATCH)
#error "M5Unified 0.2.23 or later is required for the Tab5 LCD reset fix"
#elif (M5UNIFIED_VERSION_MAJOR == 0) && \
      ((M5UNIFIED_VERSION_MINOR < 2) || \
       ((M5UNIFIED_VERSION_MINOR == 2) && (M5UNIFIED_VERSION_PATCH < 23)))
#error "Update M5Unified to 0.2.23 and M5GFX to 0.2.30 or later (Tab5 LCD_RST fix)"
#endif
#endif

// -----------------------------------------------------------------------------
// Tab5OS Arduino v0.5.3 Ecriture - LVGL 9.x
// Native home + files + editor + reader + EPUB + media + raster maps.
// Target: M5Stack Tab5 / ESP32-P4 / Arduino framework.
// -----------------------------------------------------------------------------

// ESP-IDF task stack units are bytes. setup and LVGL share this task.
SET_LOOP_TASK_STACK_SIZE(24 * 1024);
static Tab5BootStatus bootStatus;
static constexpr char APP_VERSION[] = "0.8.53";
static void warnSystemFile(const String& path);
const char* resetReasonName(esp_reset_reason_t reason);
static constexpr int SCREEN_W = 1280;
static constexpr int SCREEN_H = 720;
static constexpr int TOP_H = 48;
static constexpr int PAGE_H = SCREEN_H - TOP_H;
static constexpr int DOCK_W = 64;
static int WORK_W = SCREEN_W - DOCK_W;
static int MAP_VIEW_W = WORK_W - 16;
static constexpr int MAP_VIEW_H = 576;
static constexpr int KB_H = 264;
static int keyboardHeight() {
  return KB_H+(tab5ime::modeFor(Tab5Keyboards.selectedId().c_str())!=tab5ime::Mode::None?90:0);
}
static constexpr int FILE_LEFT_W = 380;
static constexpr size_t MAX_DOCUMENT_BYTES = 512 * 1024;
static constexpr size_t MAX_READER_TEXT_BYTES = 3 * 1024 * 1024;
static constexpr size_t FILE_PREVIEW_BYTES = 8 * 1024;
static constexpr size_t TEXT_SNIFF_BYTES = 8 * 1024;
static constexpr size_t IMAGE_SNIFF_BYTES = 64 * 1024;
static constexpr size_t MAX_IMAGE_FILE_BYTES = 12 * 1024 * 1024;
static constexpr size_t MAX_IMAGE_CANVAS_BYTES = 8 * 1024 * 1024;
static constexpr size_t FILE_PATH_CAPACITY = 260;
static constexpr size_t HISTORY_BUDGET = 4 * 1024 * 1024;
static constexpr int HISTORY_MAX = 18;
static constexpr char APP_DIR[] = "/System";
static constexpr char DOC_DIR[] = "/Documents";
static constexpr char SETTINGS_FILE[] = "/System/settings.ini";
static constexpr uint8_t SETTINGS_FORMAT_VERSION = 2;
static constexpr char LANGUAGE_DIR[] = "/System/languages";
static constexpr char FONT_FILE[] = "/System/font.ttf";
static constexpr char FONT_DIR[] = "/System/fonts";
static constexpr char FONT_BOLD_FILE[] = "/System/fonts/bold.ttf";
static constexpr char FONT_ITALIC_FILE[] = "/System/fonts/italic.ttf";
static constexpr char FONT_BOLDITALIC_FILE[] = "/System/fonts/bolditalic.ttf";
static constexpr char PROGRESS_DIR[] = "/System/progress";

// Official Tab5 microSD SPI pins.
static constexpr int SD_SPI_CS_PIN   = 42;
static constexpr int SD_SPI_SCK_PIN  = 43;
static constexpr int SD_SPI_MOSI_PIN = 44;
static constexpr int SD_SPI_MISO_PIN = 39;
static constexpr uint32_t SD_SPI_HZ  = 10000000;

struct AppSettings {
  Tab5Mouse::Config mouse;
  uint8_t brightness = 190;
  uint8_t screenSleepIndex = 3;  // Never / 1 / 2 / 5 / 10 minutes
  bool wifiPowerSave = true;
  bool screenUpsideDown = false;
  bool batteryInstalled = false; // Enable after a battery pack has been installed.
  char interfaceFontPath[Tab5FontDefaults::PathCapacity]{};
  char documentFontPath[Tab5FontDefaults::PathCapacity]{};
  uint8_t fontIndex = 1;         // 0=20, 1=24, 2=28, 3=32
  uint8_t themeIndex = 0;
  bool colorfulUi = false;
  uint8_t cornerRadius = 7;
  bool autosave = true;
  bool keyboardOnFocus = true;
  uint16_t autosaveDelaySec = 6;
  uint8_t audioVolume = 150;
  bool ntpEnabled = false;
  uint8_t timeZoneIndex = 0;
  String documentsRoot = "/";
};

struct FileEntryCtx {
  char path[FILE_PATH_CAPACITY];
  bool isDir;
  bool right;
  uint16_t depth;
};

enum class Page : uint8_t {
  Home, Files, Editor, Writing, Preview, Audio, Map, Settings, ImageEditor
};
enum class AudioView : uint8_t { Local, Radio, Recordings, Videos };
enum class TextDialogAction : uint8_t {
  None, SaveAs, NewFolder, Rename, SetClock,
  MapAddPlace, MapRenamePlace, MapSearch, FileSearch, AudioSearch
};
enum class KeyboardMode : uint8_t { Lower, Upper, Symbols };

static AppSettings settings;
static bool displayOrientationPending = false;
static ThemePalette appliedTheme;static char appliedThemeName[64];

// POSIX TZ rules are stored separately from the displayed country/city name.
// Zones with seasonal changes therefore keep the correct DST transition rules
// without hard-coding a one-off GMT offset.
static constexpr TimeZoneDef TIME_ZONES[] = {
  {"UTC · GMT+0", "UTC0"},
  {"France · GMT+1/+2", "CET-1CEST,M3.5.0/2,M10.5.0/3"},
  {"R.-Uni · GMT+0/+1", "GMT0BST,M3.5.0/1,M10.5.0/2"},
  {"Finlande · GMT+2/+3", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
  {"USA New York · GMT-5/-4", "EST5EDT,M3.2.0/2,M11.1.0/2"},
  {"USA Chicago · GMT-6/-5", "CST6CDT,M3.2.0/2,M11.1.0/2"},
  {"USA Denver · GMT-7/-6", "MST7MDT,M3.2.0/2,M11.1.0/2"},
  {"USA Los Angeles · GMT-8/-7", "PST8PDT,M3.2.0/2,M11.1.0/2"},
  {"Canada Toronto · GMT-5/-4", "EST5EDT,M3.2.0/2,M11.1.0/2"},
  {"Brésil São Paulo · GMT-3", "BRT3"},
  {"Afrique du Sud · GMT+2", "SAST-2"},
  {"Inde · GMT+5:30", "IST-5:30"},
  {"Chine · GMT+8", "CST-8"},
  {"Japon · GMT+9", "JST-9"},
  {"Australie Sydney · GMT+10/+11", "AEST-10AEDT,M10.1.0/2,M4.1.0/3"},
  {"N.-Zélande · GMT+12/+13", "NZST-12NZDT,M9.5.0/2,M4.1.0/3"},
};

static constexpr size_t TIME_ZONE_COUNT =
    sizeof(TIME_ZONES) / sizeof(TIME_ZONES[0]);

static const TimeZoneDef& selectedTimeZone() {
  const uint8_t index = settings.timeZoneIndex < TIME_ZONE_COUNT
      ? settings.timeZoneIndex : 0;
  return TIME_ZONES[index];
}

static ThemePalette THEMES[21] = {
  {"Graphite",      0x101214, 0x191C20, 0x24282D, 0x353B42, 0xF1F3F5, 0x9CA3AA, 0x3A424A, 0xE4E7EA, 0x30363D},
  {"Bleu ardoise", 0x0D121A, 0x151C27, 0x202C3B, 0x34445A, 0xF0F4F8, 0x9BAABD, 0x3C526D, 0xE7EEF7, 0x28374A},
  {"Ambre doux",    0x17130F, 0x211B15, 0x30271D, 0x4C4032, 0xF4EEE5, 0xB2A38F, 0x62513A, 0xF4E9D8, 0x3A3025},
  {"Sauge",         0x0F1513, 0x17201D, 0x22302B, 0x394B44, 0xEEF3F0, 0x9FADA7, 0x465E54, 0xE8F0EC, 0x2B3B35},
  {"Vert lecture",  0xCFE8CC, 0xCFE8CC, 0xC1DCBE, 0x759171, 0x19251C, 0x40563F, 0xA4C6A0, 0x254D2B, 0xADCFA9},
};
static size_t THEME_COUNT=5;
static char customThemeNames[16][64]{};
static char customThemeIds[16][24]{};
static bool loadCustomThemes(){
 File file=tab5FS.open("/System/themes.json");if(!file||file.size()>32768)return false;
 JsonDocument doc(tab5JsonAllocator());if(deserializeJson(doc,file)||!doc["themes"].is<JsonArray>())return false;
 THEME_COUNT=5;
 for(JsonObject item:doc["themes"].as<JsonArray>()){
  if(THEME_COUNT>=21)break;const char* name=item["name"]|"";if(!*name||strlen(name)>63||strchr(name,'\n')||strchr(name,'\r'))continue;
  uint32_t c[tab5theme::PaletteCount];
  if(!tab5theme::readPalette([&](unsigned i){return item[tab5theme::roleKey(i)].as<const char*>();},c))continue;
  char* saved=customThemeNames[THEME_COUNT-5];strcpy(saved,name);
  snprintf(customThemeIds[THEME_COUNT-5],24,"%s",item["id"]|"");
   THEMES[THEME_COUNT++]={saved,c[0],c[1],c[2],c[3],c[4],c[5],c[6],c[7],c[8],c[9],c[10],c[11],c[12],c[13],c[14],c[15],c[16],c[17],c[18]};
 }
 return true;
}
static void copyThemePalette(const ThemePalette& p,uint32_t* colors){
 const uint32_t values[]={p.bg,p.surface,p.raised,p.border,p.text,p.muted,p.active,p.cursor,p.selected,p.pointer,p.pointerOutline,
   p.switchOff<=0xffffff?p.switchOff:p.raised,p.switchOn<=0xffffff?p.switchOn:p.active,
   p.switchKnobOff<=0xffffff?p.switchKnobOff:p.text,p.switchKnobOn<=0xffffff?p.switchKnobOn:p.surface,
   p.fileIcon,p.folderIcon,
   p.hiddenFileIcon,p.hiddenFolderIcon};
 memcpy(colors,values,sizeof(values));
}

static const ThemePalette& currentTheme() {
  return appliedTheme;
}

static inline lv_color_t C_BG()      { return lv_color_hex(currentTheme().bg); }
static inline lv_color_t C_SURFACE() { return lv_color_hex(currentTheme().surface); }
static inline lv_color_t C_RAISED()  { return lv_color_hex(currentTheme().raised); }
static inline lv_color_t C_BORDER()  { return lv_color_hex(currentTheme().border); }
static inline lv_color_t C_TEXT()    { return lv_color_hex(currentTheme().text); }
static inline lv_color_t C_MUTED()   { return lv_color_hex(currentTheme().muted); }
static inline lv_color_t C_ACTIVE()  { return lv_color_hex(currentTheme().active); }
static inline lv_color_t C_CURSOR()  { return lv_color_hex(currentTheme().cursor); }
static inline lv_color_t C_SELECTED(){ return lv_color_hex(currentTheme().selected); }

// Les textes français du code servent de clés stables dans les packs JSON.
// Une clé absente reste affichée en français : un pack partiel ne peut donc
// jamais produire une interface vide.
static const char* T(const char* source) { return Tab5Locale.text(source); }

// Status updates must not invalidate unchanged labels (clock, battery, audio).
static void setLabelIfChanged(lv_obj_t* label, const char* text) {
  if (label && text && strcmp(lv_label_get_text(label), text) != 0)
    lv_label_set_text(label, text);
}

static String translateRuntimeText(const String& source) {
  const String exact = Tab5Locale.text(source);
  if (exact != source) return exact;
  // Translate application messages at the UI boundary; payloads such as
  // filenames, URLs and station names remain verbatim.
  const char* prefixes[] = {"Entrée EPUB introuvable: ",
      "Aucun document de lecture EPUB trouvé (manifest="};
  for (const char* prefix : prefixes)
    if (source.startsWith(prefix)) return String(T(prefix)) + source.substring(strlen(prefix));
  if (source.startsWith("AVI vidéo ") && source.endsWith(" non prise en charge ; MJPEG requis"))
    return String(T("AVI vidéo ")) + source.substring(strlen("AVI vidéo "),
        source.length() - strlen(" non prise en charge ; MJPEG requis")) +
        T(" non prise en charge ; MJPEG requis");
  const int separator = source.indexOf("; ");
  if (separator >= 0)
    return translateRuntimeText(source.substring(0, separator)) + "; " +
           translateRuntimeText(source.substring(separator + 2));
  if (source.startsWith("Connexion à "))
    return String(T("Connexion à ")) + source.substring(strlen("Connexion à "));
  if (source.startsWith("Wi-Fi connecté à "))
    return String(T("Wi-Fi connecté à ")) +
           source.substring(strlen("Wi-Fi connecté à "));
  if (source.endsWith("  •  radio Internet"))
    return source.substring(0, source.length() - strlen("radio Internet")) +
           T("radio Internet");
  if (source.endsWith("  •  hors ligne"))
    return source.substring(0, source.length() - strlen("hors ligne")) +
           T("hors ligne");
  return source;
}

static Page activePage = Page::Home;
static bool keyboardUiNavigation=false,keyboardDockFocus=false;
static lv_obj_t* keyboardHelpOverlay=nullptr;
static lv_obj_t* keyboardHelpBody=nullptr;
static lv_obj_t* keyboardHelpText=nullptr;
static lv_obj_t* keyboardHelpTitle=nullptr;
static std::vector<lv_obj_t*> keyboardHelpTabs;
static void showKeyboardHelp();
static void closeSystemInfo();
static bool wifiChangeAllowed();
static lv_obj_t* keyboardScope();
static void showContextMenu(lv_obj_t* hit,int x,int y);
static bool fileContextMenuAt(lv_obj_t* hit,int x,int y);
static HomeAction activeTool = HomeAction::Home;
static TextDialogAction dialogAction = TextDialogAction::None;

static String currentDir = "/";
static String currentPath;
static String selectedPath;
static String dialogDirectory;
static String pendingDeletePath;
static String pendingRenamePath;
static String clipboardPath;
static String editorClipboard;
static String lastReaderPath;
static String fileInfoText;
static bool clipboardCut = false;
static bool selectedIsDir = false;
static bool selectedIsText = false;
static bool selectedIsImage = false;
static bool selectedIsAudio = false;
static bool selectedIsVideo = false;
static bool documentDirty = false;
static bool loadingDocument = false;
static bool restoringHistory = false;
static lv_obj_t* homeNativeView=nullptr;
static lv_obj_t* homePythonView=nullptr;
static bool homePythonLibrary=false;
static void showPythonLibrary(bool visible);
static lv_obj_t* homeAppsGrid=nullptr;
static lv_obj_t* homeAppsStatus=nullptr;
static bool homeAppsDirty=true,homeAppsScanning=false;
static bool homeCustomize=false,homeRenderPending=false;
static lv_obj_t* homeDefaultGrid=nullptr;
static lv_obj_t* homeCatalogueTitle=nullptr;
static lv_obj_t* homeSaveButton=nullptr;
static lv_obj_t* homeModeButton=nullptr;
static lv_obj_t* homeEditButton=nullptr;
static lv_obj_t* homeRefreshButton=nullptr;
static lv_obj_t* homeResetButton=nullptr;
static lv_obj_t* homeIconBoxes[Tab5AppCatalogue::MaxApps]{};
static unsigned homeIconNext=0;
static unsigned homeAppCount=0;
static void serviceHomeApps();
static bool sdReady = false;
static bool systemMigrationFailed=false,systemMigrationSettingsPending=true,settingsReadComplete=false;
static bool migrateSystemDirectory(){
 const auto result=Tab5SystemMigration::run(SD,[](){delay(1);});
 systemMigrationFailed=!result.ok();
 if(systemMigrationFailed){
  systemMigrationSettingsPending=true;
  Tab5DebugSerial.printf("[System] migration blocked: %s\n",Tab5SystemMigration::errorName(result.error));
 }else if(result.moved)Tab5DebugSerial.printf("[System] migration: %u moves, %u preserved conflicts\n",unsigned(result.moved),unsigned(result.conflicts));
 return result.ok();
}
static bool pathStorageReady(const String& p){return Tab5UsbStorage::path(p.c_str())?Tab5UsbStorage::mounted():sdReady;}
static bool keyboardVisible = false;
static unsigned keyboardNumberMode = 0;
static KeyboardMode keyboardMode = KeyboardMode::Lower;
static lv_obj_t* keyboardTarget = nullptr;
static uint32_t lastEditMs = 0;
static uint32_t lastSaveMs = 0;
static uint32_t lastStatsMs = 0;
static bool statsPending = true;
static Tab5ScreenIdle screenIdle;
static bool screenWakeTouch = false;
static uint8_t appliedBrightness = 255;
static constexpr uint32_t SCREEN_TIMEOUTS[] = {0, 60000, 120000, 300000, 600000};

static bool settingsDirty = false;
static uint32_t lastSettingsChangeMs = 0;
static uint32_t savedTextHash = 2166136261UL;

// RTC / clock.
static bool rtcReady = false;
static uint32_t lastClockUpdateMs = 0;
static uint32_t lastBatteryUpdateMs = 0;
static bool wifiWasConnectedForRtc = false;
static bool ntpAwaitingSync = false;
// Distinct de ntpAwaitingSync : le service SNTP peut rester actif après avoir
// reçu l'heure. Ne jamais appeler esp_sntp_stop() si configTzTime() n'a pas
// auparavant créé le service et la boîte tcpip de lwIP.
static bool ntpServiceStarted = false;
#if TAB5OS_HAS_SNTP_CALLBACK
static volatile bool ntpTimeReceived = false;
#endif
static uint32_t ntpConfiguredAtMs = 0;

// Undo / redo snapshots in PSRAM.
static HistorySnapshot history[HISTORY_MAX];
static int historyCount = 0;
static int historyIndex = -1;
static size_t historyBytes = 0;
static bool historyPending = false;
static uint32_t lastHistoryEditMs = 0;

// LVGL.
static lv_display_t* lvDisplay = nullptr;
static lv_indev_t* lvTouch = nullptr;
static uint8_t* lvBuf1 = nullptr;
static uint8_t* lvBuf2 = nullptr;
static size_t lvBufBytes = 0;

// Unicode TTF loaded once into PSRAM.
static uint8_t* ttfData = nullptr;
static size_t ttfDataSize = 0;
static lv_font_t* uiTtfFont = nullptr;
static lv_font_t* docTtfFont = nullptr;
static lv_font_t* keyTtfFont = nullptr;
static lv_font_t* readerTtfFont = nullptr;
static bool smoothFontReady = false;
static Tab5FontDefaults::Sources configuredFontSources;
static lv_font_t *uiStockFont=nullptr,*docStockFont=nullptr,*keyStockFont=nullptr,*readerStockFont=nullptr;
static lv_font_t *headingTtfFont=nullptr,*headingStockFont=nullptr;
static bool configuredFontWarning=false;
static lv_obj_t* defaultFontButtons[2]{};
static lv_obj_t* defaultFontPopup=nullptr;
static void closeDefaultFontPopup();

// Rich EPUB font variants are loaded lazily. Regular reuses FONT_FILE/ttfData;
// the three optional companion files enable real bold and italic faces.
struct RichFontCacheEntry {
  uint16_t px = 0;
  uint8_t variant = 0; // 0 regular, 1 bold, 2 italic, 3 bold+italic
  lv_font_t* font = nullptr;
};
static RichFontBlob richFontBlob[4];
static RichFontCacheEntry richFontCache[28];
static lv_font_t* richStockFonts[28]{};
static uint8_t richFontCacheCount = 0;
static bool richFontSourceTried[4] = {false, false, false, false};

// Main UI.
static lv_obj_t* root = nullptr;
static lv_obj_t* topBar = nullptr;
static lv_obj_t* titleLabel = nullptr;
static lv_obj_t* dirtyLabel = nullptr;
static lv_obj_t* topAudioIndicator = nullptr;
static lv_obj_t* topPythonResume = nullptr;
static lv_obj_t* topHomeButton = nullptr;
static void shellTopLayout();
static void shellDockPositions();
static void shellRefreshDock();
static void shellCatalogueChanged();
static bool shellDockNeedsCatalogue();
static void shellServiceDockIcon();
static void shellLaunchDock(unsigned slot);
static const Tab5AppCatalogue::App* shellAppForKey(uint64_t key);
static void applyShellLayout();
static const char* homePageTitle();
static void homeOpenMenu(unsigned menu);
static lv_obj_t* dateLabel = nullptr;
static lv_obj_t* clockLabel = nullptr;
static lv_obj_t* topWifiLabel = nullptr;
static lv_obj_t* batteryLabel = nullptr;
static lv_obj_t* saveBtn = nullptr;
static lv_obj_t* keyboardBtn = nullptr;
static lv_obj_t* undoBtn = nullptr;
static lv_obj_t* redoBtn = nullptr;
static lv_obj_t* editorCutBtn = nullptr;
static lv_obj_t* editorCopyBtn = nullptr;
static lv_obj_t* editorPasteBtn = nullptr;
static lv_obj_t* dock = nullptr;
static constexpr int DOCK_BUTTON_COUNT = Tab5Shell::DockCount;
static lv_obj_t* dockButtons[DOCK_BUTTON_COUNT] = {};
static uint64_t dockLaunchedKey=0;

static lv_obj_t* pageHost = nullptr;
static lv_obj_t* homePage = nullptr;
static lv_obj_t* filesPage = nullptr;
static lv_obj_t* editorPage = nullptr;
static lv_obj_t* previewPage = nullptr;
static lv_obj_t* audioPage = nullptr;
static lv_obj_t* mapPage = nullptr;
static lv_obj_t* settingsPage = nullptr;

// OpenStreetMap raster viewer. The viewport is a single RGB565 canvas in
// PSRAM; only currently visible XYZ tiles are requested and cached.
static lv_obj_t* mapImage = nullptr;
static lv_obj_t* mapInfoLabel = nullptr;
static lv_obj_t* mapStatusLabel = nullptr;
static lv_obj_t* mapAttributionLabel = nullptr;
static lv_obj_t* mapZoomSlider = nullptr;
static lv_obj_t* mapZoomValueLabel = nullptr;
static M5Canvas* mapCanvas = nullptr;
static lv_image_dsc_t mapImageDsc = {};
static double mapLatitude = Tab5MapViewState::DefaultLatitude;
static double mapLongitude = Tab5MapViewState::DefaultLongitude;
static uint8_t mapZoom = Tab5MapViewState::DefaultZoom;
static bool mapZoomDragging = false;
static lv_obj_t* mapInfoButton = nullptr;
static double mapPointLatitude = 0, mapPointLongitude = 0;
static void mapPointInfo(int x, int y);
static double mapHomeLatitude = Tab5MapViewState::DefaultLatitude;
static double mapHomeLongitude = Tab5MapViewState::DefaultLongitude;
static uint8_t mapHomeZoom = Tab5MapViewState::DefaultZoom;
static uint32_t mapGeneration = 0;
// A canvas may contain complete or partial tiles, all belonging to this view.
// Preserve them only for network resumption with exactly the same generation.
static Tab5MapWheel::View mapRenderedView;
static size_t mapRenderedStyle = 0;
static int mapRenderedWidth = 0, mapRenderedHeight = 0;
static uint32_t mapRenderedGeneration = 0;
static uint16_t mapTilesNeeded = 0;
static uint16_t mapTilesReady = 0;
static uint16_t mapTilesPending = 0;
static uint16_t mapTilesFailed = 0;
static bool mapDragging = false;
static lv_point_t mapDragStart = {};
static lv_point_t mapDragLast = {};
static uint32_t lastMapViewChangeMs = 0;
static bool mapLastWifiConnected = false;
static bool mapLastDownloadsAllowed = true;
static bool mapNetworkSetupPending = false;
static uint32_t lastMapRefreshMs = 0;
static lv_obj_t* mapPlacesOverlay = nullptr;
static lv_obj_t* mapPlacesList = nullptr;
static lv_obj_t* mapSelectedPlaceButton = nullptr;
static lv_obj_t* mapPlacesDeleteOverlay = nullptr;
static lv_obj_t* mapStylesOverlay = nullptr;
static lv_obj_t* mapSearchOverlay = nullptr;
static int mapSelectedPlace = -1;
static size_t pendingMapPlaceIndex = static_cast<size_t>(-1);
static Tab5MapSearchItem* mapSearchItems = nullptr;
static uint8_t mapSearchCount = 0;
static uint32_t mapSearchRequestId = 0;
static bool mapSearchPending = false;
static Tab5MapWheel::Pending mapWheel;
static bool mapJpegPreparing=false, mapJpegEncoding=false;
static bool mapSearchResultsReady = false;
static bool mapSearchLocal=true,mapGazetteerAvailable=false,mapSearchLocalError=false,mapSearchTruncated=false;
static String mapSearchQuery;
static Tab5MapTileResult mapPendingStore = {};
static bool mapPendingStoreValid = false;
static unsigned mapStoreAttempts=0;
static uint32_t mapStoreLastAttempt=0;
static int mapLastHttpError = 0;
static int mapLastTlsError = 0;
struct MapRasterJob {
  uint32_t x, y;
  uint16_t distance;
};
static MapRasterJob* mapRasterJobs = nullptr;
static uint8_t mapRasterCount = 0, mapRasterIndex = 0;
static bool mapRasterRefresh = false;

static bool allocateMapUiStorage() {
  if (!mapSearchItems) {
    mapSearchItems = static_cast<Tab5MapSearchItem*>(heap_caps_calloc(
        TAB5_MAP_MAX_SEARCH_RESULTS, sizeof(Tab5MapSearchItem),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  if (!mapRasterJobs)
    mapRasterJobs = static_cast<MapRasterJob*>(heap_caps_calloc(
        32, sizeof(MapRasterJob), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  return mapSearchItems && mapRasterJobs;
}
// La première vue est initialisée par loop(), après le retour du callback
// LVGL. Cela évite une allocation PSRAM et une lecture SD imbriquées dans le
// gestionnaire de clic, ainsi que deux générations successives au premier
// passage lorsque le Wi-Fi est déjà connecté.
static bool mapOpenPending = false;
static lv_obj_t* mapDownloadOverlay = nullptr;


// Native audio branch: local MP3/WAV, MP3 web radios and WAV recording.
static AudioView audioView = AudioView::Local;
static String audioSearchQueries[4];
static lv_obj_t* audioSearchClear=nullptr;
static String audioAlbumFolder;
static String audioRadioCategory;
static std::vector<String> audioAlbumFolders;
static std::vector<String> audioRadioCategories;
static lv_obj_t* audioList = nullptr;
static lv_obj_t* audioListTitle = nullptr;
static lv_obj_t* audioNowPlaying = nullptr;
static lv_obj_t* audioStatus = nullptr;
static lv_obj_t* audioPlayBtn = nullptr;
static lv_obj_t* audioStopBtn = nullptr;
static lv_obj_t* audioRecordBtn = nullptr;
static lv_obj_t* audioRecordGraphic = nullptr;
static lv_obj_t* audioRecordControls = nullptr;
static lv_obj_t* audioProgressSlider = nullptr;
static lv_obj_t* audioTimeLabel = nullptr;
static lv_obj_t* audioVolumeSlider = nullptr;
static lv_obj_t* audioVolumeValue = nullptr;
static lv_obj_t* audioVolumeIcon = nullptr;
static lv_obj_t* audioLevelBar = nullptr;
static lv_obj_t* audioFavoritesTab=nullptr;
static lv_obj_t* audioFavoritesSelector=nullptr;
static lv_obj_t* audioSortSelector=nullptr;
static Tab5MediaList* audioRows=nullptr;
static bool audioListRebuilding=false;
static AudioView audioRenderedView=AudioView::Local;
static uint32_t audioRenderedRevision=0;
static bool audioRefreshPending=false;
static lv_obj_t* audioNewFolder=nullptr;
static bool audioFavoritesOnly[2]{};
static lv_obj_t* audioLocalTab = nullptr;
static lv_obj_t* audioRadioTab = nullptr;
static lv_obj_t* audioRecordTab = nullptr;
static lv_obj_t* audioVideoTab = nullptr;
static bool audioProgressDragging = false;
static lv_obj_t* wifiOverlay = nullptr;
static lv_obj_t* wifiNetworkList = nullptr;
static lv_obj_t* wifiSelectedLabel = nullptr;
static lv_obj_t* wifiPasswordInput = nullptr;
static lv_obj_t* wifiPopupAutoSwitch = nullptr;
static lv_obj_t* wifiScanButton = nullptr;
static lv_obj_t* wifiConnectButton = nullptr;
static lv_obj_t* wifiForgetButton = nullptr;
static String wifiSelectedSsid;
static bool wifiSelectedSecured = true;
static uint32_t shownWifiScanRevision = 0;
static uint32_t lastWifiSettingsUpdateMs = 0;

// Full-screen P4 MJPEG viewer. The engine owns three PSRAM frame buffers;
// LVGL only points at the published front buffer.
static bool videoViewerActive = false;
static lv_obj_t* videoViewerScreen = nullptr;
static lv_obj_t* videoViewerImage = nullptr;
static lv_obj_t* videoViewerTitle = nullptr;
static lv_obj_t* videoViewerStatus = nullptr;
static lv_obj_t* videoViewerPause = nullptr;
static lv_image_dsc_t videoViewerDsc = {};
static uint32_t shownVideoGeneration = 0;

// Split file browser.
static lv_obj_t* filePathLabel = nullptr;
static String fileSearchQuery,fileSearchRoot;
static Tab5FileSearch fileSearcher;
static lv_obj_t* fileSearchClear=nullptr;
static lv_obj_t* fileOpenWithBtn=nullptr;
static lv_obj_t* fileOperationOverlay=nullptr;
static lv_obj_t* fileOperationMessage=nullptr;
static lv_obj_t* fileOperationButton=nullptr;
static lv_obj_t* fileHiddenBtn=nullptr;
static File fileListDir;
static Tab5DirectoryModel fileDirectoryModel;
static bool fileDirectoryFailed=false;
static Tab5FolderPreview folderPreview;
static Tab5ExplorerTree fileTree;
static String fileContentDir;
enum class FileContentMode:uint8_t {Empty,Directory,Preview,Search};
static FileContentMode fileContentMode=FileContentMode::Empty;
static String fileDisplayedPath,fileMetadataPath;
static uint32_t fileDisplayedRevision=0,fileMetadataRevision=0;
static size_t fileSelectedBytes=0;
static bool fileTreeRequested=false;
static bool fileSessionStarted=false,fileSessionRestorePending=false,fileSearchNeedsRestart=false;
static int32_t fileSavedTreeScroll=0,fileSavedContentScroll=0;
static size_t fileSavedContentOffset=0,fileSearchOffset=0;
static String fileSavedFocusPath;
static bool fileSavedFocusRight=false;
static void ensureFileSession();
static void rememberFileSession();
static void restoreFileSessionPosition();
static void beginFileSearch(const String& directory,const String& query);
static void renderFileSearchPage(size_t offset);
static void fileClearSearch();
static void updateFileTreeHints();
static lv_obj_t* filePaneDivider=nullptr;
static void showFileSelectionContent(bool force=false);
static void restoreFileContent();
static void clearFileContent();
static bool fileActiveRight=false;
static lv_obj_t* folderPreviewHost=nullptr;
static lv_obj_t* folderPreviewRows=nullptr;
static lv_obj_t* folderPreviewPrevious=nullptr;
static lv_obj_t* folderPreviewNext=nullptr;
static lv_obj_t* folderPreviewPageLabel=nullptr;
static lv_obj_t* fileHideSelectedBtn=nullptr;
static lv_obj_t* fileMultiSelectBtn=nullptr;
static lv_obj_t* fileDeleteOverlay=nullptr;
static Tab5FileSelection fileSelection;
struct FileSelectionRow { std::string path; bool directory; lv_obj_t* object; bool right; uint16_t depth; bool checked=false,tick=false; };
static std::vector<FileSelectionRow> fileSelectionRows;
static std::vector<std::string> fileClipboardPaths,pendingDeletePaths;
static String fileSelectionScope;
static String fileListRevealPath; // keyboard focus restoration after a tree rebuild
static String fileTreeRevealPath; // explicit double-click request, independent of pane navigation
static void fileRequestTreeReveal(const String& path);
static void serviceFileTreeReveal();
static bool fileMultiSelect=false,fileSelectionFocusOnly=false,fileSelectionLongPress=false;
static File fileSelectAllDirectory;
static bool fileSelectAllSearch=false;
static String fileSelectAllRoot;
static void openFileContentDirectory(const String& directory);
static void startFileListPage(size_t offset);
struct FilePasteBatch {
  std::vector<std::string> paths;
  String directory,lastError;
  size_t next=0,done=0,failed=0,retained=0;
  bool active=false,cut=false,cancelled=false;
} filePasteBatch;
static void updateFileSelectionUi();
static void serviceFileSelection();
static bool fileSelectionKey(const writing::KeyEvent& e);
static void closeFileDeleteDialog();
static void advanceFilePasteBatch();
static lv_obj_t* fileUsbButton=nullptr;
static lv_obj_t* fileParentButton=nullptr;
static uint32_t fileKnownRevision=0;
static int32_t fileRefreshScroll=-1;
static void serviceFolderPreview();
static void cancelFolderPreview();
static void folderPreviewPageEvent(lv_event_t* event);
static void toggleSelectedHidden(lv_event_t* event);
static size_t fileListOffset=0,fileListSeen=0,fileListShown=0;
static bool fileListNeedsFocus=false;
static constexpr size_t FILE_LIST_PAGE=128;
static lv_obj_t* fileList = nullptr;
static lv_obj_t* selectedFileBtn = nullptr;
static lv_obj_t* filePreviewTitle = nullptr;
static lv_obj_t* filePreviewMeta = nullptr;
static lv_obj_t* filePreviewLabel = nullptr;
static lv_obj_t* filePreviewImage = nullptr;
static M5Canvas* filePreviewImageCanvas = nullptr;
static String filePreviewImagePath;
static lv_image_dsc_t filePreviewImageDsc = {};
static lv_obj_t* fileInfoBtn = nullptr;
static lv_obj_t* filePrintBtn = nullptr;
static lv_obj_t* fileVaultBtn = nullptr;
static lv_obj_t* fileActionBar = nullptr;
static lv_obj_t* fileGitBtn = nullptr;
static lv_obj_t* fileGitHost = nullptr;
static lv_obj_t* fileActionsMenuBtn = nullptr;
static lv_obj_t* filePreviewScroll = nullptr;
static lv_obj_t* fileOpenBtn = nullptr;
static lv_obj_t* fileDrawBtn = nullptr;
static lv_obj_t* fileReadBtn = nullptr;
static lv_obj_t* fileExecuteBtn = nullptr;
static lv_obj_t* fileCopyBtn = nullptr;
static lv_obj_t* fileCutBtn = nullptr;
static lv_obj_t* filePasteBtn = nullptr;
static lv_obj_t* fileRenameBtn = nullptr;
static lv_obj_t* deleteDocBtn = nullptr;

// Editor / preview.
static lv_obj_t* editor = nullptr;
static lv_obj_t* editorStatus = nullptr;
static lv_obj_t* previewLabel = nullptr;
static lv_obj_t* previewHint = nullptr;
static lv_obj_t* keyboardPanel = nullptr;
static lv_obj_t* accentPopup = nullptr;

// Settings.
static lv_obj_t* brightnessSlider = nullptr;
static lv_obj_t* brightnessValue = nullptr;
static lv_obj_t* autosaveSwitch = nullptr;
static lv_obj_t* keyboardFocusSwitch = nullptr;
static lv_obj_t* autosaveDelaySlider = nullptr;
static lv_obj_t* autosaveDelayValue = nullptr;
static lv_obj_t* fontValue = nullptr;
static lv_obj_t* fontPreview = nullptr;
static lv_obj_t* themeDropdown = nullptr;
static lv_obj_t* rtcValue = nullptr;
static lv_obj_t* ntpSwitch = nullptr;
static lv_obj_t* latencySwitch = nullptr;
static lv_obj_t* timeZoneDropdown = nullptr;
static lv_obj_t* timeSetButton = nullptr;
static bool systemInfoDetailsRequested = false;
static uint32_t systemInfoUpdatedAt = 0;
static lv_obj_t* fileShareStatusLabel = nullptr;
static lv_obj_t* fileShareLanButton = nullptr;
static lv_obj_t* fileShareDirectButton = nullptr;
static lv_obj_t* fileShareStopButton = nullptr;
static lv_obj_t* systemInfoOverlay = nullptr;
static lv_obj_t* systemInfoBody = nullptr;
static bool systemInfoRefreshRequested = false;
static void showSystemInfo();
static void closeSystemInfo();
static bool wifiChangeAllowed();
static lv_obj_t* wifiPowerSwitch = nullptr;
static lv_obj_t* wifiSettingsStatus = nullptr;
static lv_obj_t* languageDropdown = nullptr;
static lv_obj_t* keyboardLayoutDropdown = nullptr;
static lv_obj_t* documentsRootButton = nullptr;
static constexpr uint8_t SETTINGS_SECTION_COUNT = 4;
static lv_obj_t* settingsSectionButtons[SETTINGS_SECTION_COUNT] = {};
static lv_obj_t* settingsSectionPanels[SETTINGS_SECTION_COUNT] = {};
static uint8_t activeSettingsSection = 0;
static bool settingsStorageMounted = false;
static lv_obj_t* sidebarSettingsOverlay = nullptr;

// Dialog.
static lv_obj_t* dialogOverlay = nullptr;
static lv_obj_t* dialogBox = nullptr;
static lv_obj_t* dialogInput = nullptr;
static lv_obj_t* filePopupOverlay = nullptr;

// Landscape image viewer. The decoded RGB565 canvas lives in PSRAM; LVGL
// handles zooming while the scrollable viewport handles drag-to-pan.
static bool imageViewerActive = false;
static String imageViewerPath;
static lv_obj_t* imageViewerScreen = nullptr;
static lv_obj_t* imageViewerViewport = nullptr;
static lv_obj_t* imageViewerContent = nullptr;
static lv_obj_t* imageViewerObject = nullptr;
static lv_obj_t* imageViewerZoomLabel = nullptr;
static M5Canvas* imageViewerCanvas = nullptr;
static lv_image_dsc_t imageViewerDsc = {};
static int imageViewerBaseW = 0;
static int imageViewerBaseH = 0;
static uint16_t imageViewerScale = 256;
static uint16_t imageViewerFitScale = 256;

// Portrait reader.
static bool readerActive = false;
static bool readerIsEpub = false;
static String readerPath;
static epub_lite::Book readerBook;
static uint16_t readerChapter = 0;
static char* readerText = nullptr;
static size_t readerTextLen = 0;
static lv_obj_t* readerScreen = nullptr;
static lv_obj_t* readerTextLabel = nullptr;
static lv_obj_t* readerTitleLabel = nullptr;
static lv_obj_t* readerStatusLabel = nullptr;
static lv_obj_t* readerSeekSlider = nullptr;
static bool readerSeekDragging = false;
static void readerSeekSync();
static lv_obj_t* readerClockLabel = nullptr;
static lv_obj_t* readerRestartBtn = nullptr;
static lv_obj_t* readerContent = nullptr;
static lv_obj_t* readerTapLayer = nullptr;
static lv_obj_t* readerImageObj = nullptr;
static M5Canvas* readerImageCanvas = nullptr;
static lv_image_dsc_t readerImageDsc = {};
static constexpr uint16_t READER_PAGE_CACHE_MAX = 2048;
static Tab5PsramTable<uint32_t, READER_PAGE_CACHE_MAX + 1> readerPageStarts;
static uint16_t readerPageStartCount = 0;
static uint16_t readerPageIndex = 0;
static bool readerChapterPaginationComplete = false;
static float readerPageAspect = 2.0f / 3.0f;
static float readerDisplayedAspect = 2.0f / 3.0f;
static int readerChapterViewportW = 0;
static int readerChapterViewportH = 0;
// Reading position is persisted per document on the SD card. For EPUB we store
// chapter + byte offset of the first character on the current page, rather than
// a fragile page number. Plain text stores its vertical scroll position.
static bool readerProgressDirty = false;
static uint32_t lastReaderProgressChangeMs = 0;
static bool readerResumePending = false;
static uint16_t readerResumeChapter = 0;
static uint32_t readerResumeOffset = 0;
static int32_t readerResumeScrollY = 0;

// EPUB 0.6 rich-layout state. A page location is block + offset inside that
// block (byte offset for rich text, row index for tables).
static epub_render::Document readerRichDoc;
static Tab5PsramTable<RichLoc, READER_PAGE_CACHE_MAX + 1> readerRichPageStarts;
static uint16_t readerRichPageStartCount = 0;
static uint16_t readerResumeBlock = 0;
static bool readerResumeHasRichLoc = false;
static lv_obj_t* readerRichHost = nullptr;
static lv_obj_t* readerMeasureHost = nullptr;
static bool readerRichPaginationComplete = false;
static bool readerPageDisplayed = false;

// Forward declarations.
static void showPage(Page page);
static void showSettingsSection(uint8_t section);
static void openHomeAction(HomeAction action);
static void openImageEditor(const char* path = nullptr);
static bool imageMouseEvent(int x,int y,uint8_t buttons,int wheel);
static void updateResourceForeground();
static void refreshSystemInfo();
static void refreshFileList();
static void previewSelectedFile();
static void updateDocumentTitle();
static void updateEditorStatus(bool force = false);
static void applyEditorFont();
static void setKeyboardVisible(bool visible, lv_obj_t* target = nullptr);
static void imeSync(lv_obj_t* input);
static void imeCancel();
static void rebuildKeyboard();
static bool saveDocumentTo(const String& path, bool showToast = true);
static void newDocument();
static void openTextDialog(TextDialogAction action, const char* title, const String& initial, const String& directory);
static void closeTextDialog();
static void toast(const char* text, uint32_t ms = 1600);
static bool loadSmoothFonts();
static const char* storageSdAction(bool eject);
static const lv_font_t* uiFont();
static const lv_font_t* documentFont();
static const lv_font_t* keyFont();
static const lv_font_t* readerFont();
static void updateNavState();
static void updateClockLabels(bool force = false);
static void serviceNetworkRtc();
static void updateUndoRedoState();
static void updateEditorClipboardState();
static void resetHistory();
static void pushHistorySnapshot(bool force = false);
static void startReader(const String& path);
static void stopReader();
static void startImageViewer(const String& path);
static void stopImageViewer();
static void startVideoViewer(size_t index, uint32_t revision);
static void startVideoViewerSource(const String& source,
                                   const String& name = String(),
                                   float fps = 25.0f,
                                   const String& format = String());
static void stopVideoViewer();
static void updateVideoViewer(bool force = false);
static void createMapPage();
static void renderMapViewport(bool refreshTiles = false, bool preserveCanvas = false);
static void markMapViewDirty();
static void mapDownloadEvent(lv_event_t* event);

static void serviceMapPage();
static void freeMapCanvas();
static void refreshMapPlacesList();
static void showMapSearchResults();
static void refreshAudioList();
static void audioRememberBrowser();
static void audioRememberScroll();
static void audioRestoreBrowser();
static uint32_t audioCatalogueRevision(AudioView view);
static bool audioModelCurrent();
static bool audioRememberItem(size_t index);
static void audioRememberPlayback(const Tab5AudioSnapshot& audioState);
static void updateAudioUi(bool force = false);
static void updateTopAudioIndicator(bool force = false);
static void updatePythonResumeState();
static void resumePythonApp();
static void refreshWifiPopupList(bool force = false);
static void refreshWifiSettingsUi(bool force = false);
static void wifiAutoStartEvent(lv_event_t* e);
static void openAudioPath(const String& path);
static void closeWifiPopup();
static void freeFilePreviewImage();
static void showFilePreviewImage();
static void readerLoadCurrent();
static void saveReaderProgressNow();
static void markReaderProgressDirty();
static String readerProgressPath(const String& path);
static void migrateReaderProgress(const String& oldPath, const String& newPath);
static bool readerRenderImage(const String& entryPath);
static const lv_font_t* richFontFor(uint16_t px, uint8_t flags);
static bool imageNaturalSize(const uint8_t* data, size_t len, int& w, int& h);
static bool isReadableTextFile(const String& path);
static bool isReadableImageFile(const String& path);
static bool loadSdImageCanvas(const String& path, int maxW, int maxH,
                              M5Canvas*& canvas, lv_image_dsc_t& dsc,
                              int& naturalW, int& naturalH, String& err);

// -----------------------------------------------------------------------------
// Utilities
// -----------------------------------------------------------------------------

static String baseNameOf(const String& path) {
  if (path.length() == 0) return T("Sans titre");
  int slash = path.lastIndexOf('/');
  return (slash >= 0) ? path.substring(slash + 1) : path;
}

static String parentDirOf(const String& path) {
  if (path.length() <= 1) return "/";
  int slash = path.lastIndexOf('/');
  if (slash <= 0) return "/";
  return path.substring(0, slash);
}

static String joinPath(const String& dir, const String& name) {
  if (dir == "/") return "/" + name;
  return dir + "/" + name;
}

static bool isEditableTextExtension(const String& name) {
  String s = name;
  s.toLowerCase();
  return Tab5WritingApp::codeFile(name.c_str()) || s.endsWith(".txt") || s.endsWith(".md") || s.endsWith(".markdown") ||
         s.endsWith(".log") || s.endsWith(".py") || s.endsWith(".json") ||
         s.endsWith(".csv") || s.endsWith(".ini") || s.endsWith(".cfg") ||
         s.endsWith(".xml") || s.endsWith(".html") || s.endsWith(".htm") ||
         s.endsWith(".css") || s.endsWith(".js") || s.endsWith(".yaml") ||
         s.endsWith(".yml");
}

static bool isImageProject(const String& name) { return Tab5ImageFormats::kind(name.c_str())==Tab5ImageFormats::Kind::Project; }
static bool isExecutableExtension(const String& name) { String lower=name;lower.toLowerCase();return lower.endsWith(".elf")||tab5ApplicationForFile(name.c_str()); }

static bool isFirmwareExtension(const String& name) { String lower=name;lower.toLowerCase();return lower.endsWith(".bin"); }

static bool isPythonExtension(const String& name) {
  String s = name;
  s.toLowerCase();
  return s.endsWith(".py");
}

static bool isShellExtension(const String& name) {
  String lower=name;lower.toLowerCase();return lower.endsWith(".sh");
}

static bool isEpubExtension(const String& name) {
  String s = name;
  s.toLowerCase();
  return s.endsWith(".epub");
}

static bool isImageExtension(const String& name) {
  return Tab5ImageFormats::supported(name.c_str());
}

static bool isAudioExtension(const String& name) {
  String s = name;
  s.toLowerCase();
  return s.endsWith(".mp3") || s.endsWith(".aac") ||
         s.endsWith(".wav") || s.endsWith(".flac");
}

static bool isVideoExtension(const String& name) {
  String s = name;
  const int query = s.indexOf('?');
  if (query >= 0) s.remove(query);
  s.toLowerCase();
  return s.endsWith(".avi") || s.endsWith(".mjpg") ||
         s.endsWith(".mjpeg");
}

static bool hasSupportedExtension(const String& name) {
  return isEpubExtension(name) || isReadableTextFile(name);
}

// Accept plain ASCII and well-formed UTF-8. NUL bytes and malformed sequences
// are strong binary-file signals. A very small amount of non-text control data
// is tolerated for logs that contain an escape or backspace character.
static bool looksLikeUtf8Text(const uint8_t* data, size_t len, bool allowTrailingPartial) {
  if (!data) return false;
  if (len == 0) return true;

  size_t i = 0;
  if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) i = 3;
  size_t controls = 0;
  while (i < len) {
    const uint8_t c = data[i];
    if (c == 0) return false;
    if (c < 0x80) {
      if (c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != '\f') ++controls;
      ++i;
      continue;
    }

    size_t extra = 0;
    uint32_t cp = 0;
    if (c >= 0xC2 && c <= 0xDF) { extra = 1; cp = c & 0x1F; }
    else if (c >= 0xE0 && c <= 0xEF) { extra = 2; cp = c & 0x0F; }
    else if (c >= 0xF0 && c <= 0xF4) { extra = 3; cp = c & 0x07; }
    else return false;
    if (i + extra >= len) {
      if (allowTrailingPartial) { i = len; continue; }
      return false;
    }
    for (size_t j = 1; j <= extra; ++j) {
      const uint8_t cc = data[i + j];
      if ((cc & 0xC0) != 0x80) return false;
      cp = (cp << 6) | (cc & 0x3F);
    }
    if ((extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000) ||
        (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) return false;
    i += extra + 1;
  }
  return controls <= 2 && controls * 100 <= len + 1;
}

static bool looksLikeTextFile(File& f) {
  if (!f || f.isDirectory()) return false;
  const size_t oldPos = static_cast<size_t>(f.position());
  if (!f.seek(0)) return false;
  const size_t want = min(static_cast<size_t>(f.size()), TEXT_SNIFF_BYTES);
  uint8_t* sample = static_cast<uint8_t*>(malloc(want ? want : 1));
  if (!sample) { f.seek(oldPos); return false; }
  const size_t got = want ? f.read(sample, want) : 0;
  const bool text = looksLikeUtf8Text(sample, got, static_cast<size_t>(f.size()) > got);
  free(sample);
  f.seek(oldPos);
  return text;
}

static bool isReadableTextFile(const String& path) {
  if (isEditableTextExtension(path)) return true;
  if (isEpubExtension(path) || isImageExtension(path) || !pathStorageReady(path)) return false;
  File f = tab5FS.open(path.c_str(), FILE_READ);
  if (!f || f.isDirectory()) { if (f) f.close(); return false; }
  const bool text = looksLikeTextFile(f);
  f.close();
  return text;
}

static String sanitizeFilename(String name, bool appendTxt) {
  name.trim();
  if (name.length() == 0) name = "document";
  const char* bad = "/\\:*?\"<>|";
  for (size_t i = 0; i < strlen(bad); ++i) name.replace(bad[i], '_');
  if(name=="."||name=="..")name="";
  if (name.length() == 0) name = "document";
  if (appendTxt && !isEditableTextExtension(name)) name += ".txt";
  return name;
}

static const lv_font_t* fontForIndex(uint8_t idx) {
  switch (idx) {
    case 0: return &lv_font_montserrat_20;
    case 1: return &lv_font_montserrat_24;
    case 2: return &lv_font_montserrat_28;
    default: return &lv_font_montserrat_32;
  }
}

static uint16_t fontSizeForIndex(uint8_t idx) {
  return Tab5DocumentText::pixels(idx);
}

static void setText(lv_obj_t* label, const String& s) {
  lv_label_set_text(label, s.c_str());
}

static const lv_font_t* uiFont() {
  return smoothFontReady && uiTtfFont ? uiTtfFont : &lv_font_montserrat_20;
}

static const lv_font_t* documentFont() {
  return smoothFontReady && docTtfFont ? docTtfFont : fontForIndex(settings.fontIndex);
}

static const lv_font_t* keyFont() {
  return smoothFontReady && keyTtfFont ? keyTtfFont : &lv_font_montserrat_32;
}

static const lv_font_t* readerFont() {
  return smoothFontReady && readerTtfFont ? readerTtfFont : fontForIndex(settings.fontIndex);
}

static lv_font_t* configuredFont(Tab5FontDefaults::Role role,uint16_t pixels,unsigned cache,lv_font_t*& fallback) {
#if LV_USE_TINY_TTF
  fallback=nullptr;
  auto* stock=lv_tiny_ttf_create_data_ex(ttfData,ttfDataSize,pixels,LV_FONT_KERNING_NORMAL,cache);
  if(!stock)return nullptr;
  Tab5Cjk.attach(stock,pixels);
  if(const auto* custom=configuredFontSources.custom(role)){
    auto* selected=lv_tiny_ttf_create_data_ex(custom->data,custom->size,pixels,LV_FONT_KERNING_NORMAL,cache);
    if(selected){fallback=stock;Tab5Cjk.attach(selected,pixels,stock);return selected;}
    configuredFontWarning=true;
  }
  return stock;
#else
  (void)role;(void)pixels;(void)cache;(void)fallback;return nullptr;
#endif
}
static void resizeConfiguredFont(lv_font_t* font,lv_font_t* fallback,uint16_t pixels){
#if LV_USE_TINY_TTF
  if(fallback){lv_tiny_ttf_set_size(fallback,pixels);Tab5Cjk.attach(fallback,pixels);}
  if(font){lv_tiny_ttf_set_size(font,pixels);Tab5Cjk.attach(font,pixels,fallback);}
#else
  (void)font;(void)fallback;(void)pixels;
#endif
}

static bool loadSmoothFonts() {
#if LV_USE_TINY_TTF
  if (!sdReady || !tab5FS.exists(FONT_FILE)) return false;
  Tab5Cjk.select(Tab5Locale.selectedId(),Tab5Keyboards.selectedId());
  File f = tab5FS.open(FONT_FILE, FILE_READ);
  if (!f) return false;

  const size_t size = static_cast<size_t>(f.size());
  if (size < 4096 || size > (6U * 1024U * 1024U)) { f.close(); return false; }

  ttfData = static_cast<uint8_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!ttfData) { f.close(); return false; }
  const size_t got = f.read(ttfData, size);
  f.close();
  if (got != size) {
    heap_caps_free(ttfData); ttfData = nullptr; return false;
  }
  ttfDataSize = size;

  // Keep startup light: only the UI/document faces are created here.
  // The larger keyboard and reader faces are created only when those modes open.
  configuredFontSources.begin(settings.interfaceFontPath,settings.documentFontPath);
  configuredFontWarning=configuredFontSources.result(Tab5FontDefaults::Role::Interface)!=Tab5FontDefaults::Result::Ok||
      configuredFontSources.result(Tab5FontDefaults::Role::Document)!=Tab5FontDefaults::Result::Ok;
  uiTtfFont=configuredFont(Tab5FontDefaults::Role::Interface,20,256,uiStockFont);
  docTtfFont=configuredFont(Tab5FontDefaults::Role::Document,fontSizeForIndex(settings.fontIndex),640,docStockFont);
  if (!uiTtfFont || !docTtfFont) {
    for(auto* font:{uiTtfFont,docTtfFont,uiStockFont,docStockFont})if(font){Tab5Cjk.detach(font);lv_tiny_ttf_destroy(font);}
    uiTtfFont=docTtfFont=uiStockFont=docStockFont=nullptr;
    heap_caps_free(ttfData); ttfData = nullptr; ttfDataSize = 0;
    return false;
  }

  Tab5Cjk.attach(uiTtfFont,20,uiStockFont);
  Tab5Cjk.attach(docTtfFont,fontSizeForIndex(settings.fontIndex),docStockFont);
  smoothFontReady = true;
  return true;
#else
  return false;
#endif
}

static bool ensureKeyFont() {
#if LV_USE_TINY_TTF
  if (keyTtfFont) return true;
  if (!smoothFontReady || !ttfData || !ttfDataSize) return false;
  keyTtfFont=configuredFont(Tab5FontDefaults::Role::Interface,34,256,keyStockFont);
  return keyTtfFont != nullptr;
#else
  return false;
#endif
}

static bool ensureReaderFont() {
#if LV_USE_TINY_TTF
  if (readerTtfFont) return true;
  if (!smoothFontReady || !ttfData || !ttfDataSize) return false;
  readerTtfFont=configuredFont(Tab5FontDefaults::Role::Document,fontSizeForIndex(settings.fontIndex),1024,readerStockFont);
  return readerTtfFont != nullptr;
#else
  return false;
#endif
}


static bool loadRichFontBlob(const char* path, RichFontBlob& dst) {
#if LV_USE_TINY_TTF
  if (!sdReady || !path || !tab5FS.exists(path)) return false;
  File f = tab5FS.open(path, FILE_READ);
  if (!f) return false;
  const size_t n = static_cast<size_t>(f.size());
  if (n < 4096 || n > 6U * 1024U * 1024U) { f.close(); return false; }
  uint8_t* data = static_cast<uint8_t*>(heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!data) { f.close(); return false; }
  const size_t got = f.read(data, n); f.close();
  if (got != n) { heap_caps_free(data); return false; }
  dst.data = data; dst.size = n;
  return true;
#else
  (void)path; (void)dst; return false;
#endif
}

static void ensureRichFontSource(uint8_t variant) {
  if (variant > 3) variant = 0;
  if (richFontSourceTried[variant]) return;
  richFontSourceTried[variant] = true;

  if (variant == 0) {
    richFontBlob[0].data = ttfData;
    richFontBlob[0].size = ttfDataSize;
    return;
  }

  const char* path = nullptr;
  if (variant == 1) path = FONT_BOLD_FILE;
  else if (variant == 2) path = FONT_ITALIC_FILE;
  else if (variant == 3) path = FONT_BOLDITALIC_FILE;

  if (path) loadRichFontBlob(path, richFontBlob[variant]);
}

static const lv_font_t* richFontFor(uint16_t px, uint8_t flags) {
#if LV_USE_TINY_TTF
  if (px < 14) px = 14; if (px > 58) px = 58;
  uint8_t variant = 0;
  if (flags & epub_render::TXT_BOLD) variant |= 1;
  if (flags & epub_render::TXT_ITALIC) variant |= 2;
  for (uint8_t i = 0; i < richFontCacheCount; ++i) {
    if (richFontCache[i].px == px && richFontCache[i].variant == variant && richFontCache[i].font)
      return richFontCache[i].font;
  }
  ensureRichFontSource(0);
  ensureRichFontSource(variant);
  RichFontBlob src = richFontBlob[variant];
  if (!src.data || !src.size) src = richFontBlob[0];
  if (!src.data || !src.size) return readerFont();

  // Italic faces may omit Arabic and other scripts. Keep a regular face of
  // the same size in their fallback chain, reserving it before the cache slot.
  auto* regularFallback=variant?const_cast<lv_font_t*>(richFontFor(px,0)):nullptr;

  // Reuse a small fixed cache. EPUB pages are static, so a modest glyph cache
  // per face/size is enough and avoids allocating dozens of large font objects.
  if (richFontCacheCount >= 28) {
    // Never destroy a font that might still be referenced by a span on screen.
    // When the cache is full, use the closest already-created face.
    int best = -1; int bestD = 1000;
    for (uint8_t i = 0; i < richFontCacheCount; ++i) {
      if (richFontCache[i].variant != variant || !richFontCache[i].font) continue;
      int d = abs(static_cast<int>(richFontCache[i].px) - static_cast<int>(px));
      if (d < bestD) { bestD = d; best = i; }
    }
    if (best >= 0) return richFontCache[best].font;
    return readerFont();
  }
  const uint8_t slot = richFontCacheCount++;
  lv_font_t* f=variant?lv_tiny_ttf_create_data_ex(src.data,src.size,px,LV_FONT_KERNING_NORMAL,192):configuredFont(Tab5FontDefaults::Role::Document,px,192,richStockFonts[slot]);
  if (!f) { --richFontCacheCount; return readerFont(); }
  Tab5Cjk.attach(f,px,regularFallback?regularFallback:richStockFonts[slot]);
  richFontCache[slot].px = px;
  richFontCache[slot].variant = variant;
  richFontCache[slot].font = f;
  return f;
#else
  (void)px; (void)flags; return readerFont();
#endif
}

static const lv_font_t* uiHeadingFont() {
  if(smoothFontReady&&!headingTtfFont)headingTtfFont=configuredFont(Tab5FontDefaults::Role::Interface,24,192,headingStockFont);
  return headingTtfFont?headingTtfFont:uiFont();
}

// -----------------------------------------------------------------------------
// Settings on SD
// -----------------------------------------------------------------------------

static bool isExistingDirectory(const String& path) {
  if (!pathStorageReady(path) || !path.length()) return false;
  File directory = tab5FS.open(path.c_str());
  const bool valid = directory && directory.isDirectory();
  if (directory) directory.close();
  return valid;
}

static void ensureDirectories() {
  if (!tab5FS.exists(APP_DIR)) tab5FS.mkdir(APP_DIR);
  if (!tab5FS.exists(DOC_DIR)) tab5FS.mkdir(DOC_DIR);
  if (!tab5FS.exists(PROGRESS_DIR)) tab5FS.mkdir(PROGRESS_DIR);
  if (!tab5FS.exists(FONT_DIR)) tab5FS.mkdir(FONT_DIR);
  if (!tab5FS.exists(LANGUAGE_DIR)) tab5FS.mkdir(LANGUAGE_DIR);
}

static bool saveSettings() {
  if (!sdReady || systemMigrationSettingsPending) return false;
  const String temporary = String(SETTINGS_FILE) + ".tmp";
  const String backup = String(SETTINGS_FILE) + ".bak";
  tab5FS.remove(temporary.c_str());
  errno=0;File f = tab5FS.open(temporary.c_str(), FILE_WRITE);
  if (!f){tab5StorageFault("réglages",temporary.c_str());return false;}
  size_t expected = 0;
  bool complete = true;
  auto append = [&](const char* format, auto value) {
    const int length = snprintf(nullptr, 0, format, value);
    if (length < 0) { complete = false; return; }
    expected += static_cast<size_t>(length);
    if (f.printf(format, value) != static_cast<size_t>(length)) complete = false;
  };
  append("settings_version=%u\n", SETTINGS_FORMAT_VERSION);
  append("brightness=%u\n", settings.brightness);
  append("screen_upside_down=%u\n", settings.screenUpsideDown ? 1 : 0);
  append("screen_sleep=%u\n", settings.screenSleepIndex);
  append("wifi_power_save=%u\n", settings.wifiPowerSave ? 1 : 0);
  append("font=%u\n", settings.fontIndex);
  append("interface_font=%s\n",settings.interfaceFontPath);
  append("document_font=%s\n",settings.documentFontPath);
  append("theme=%u\n", settings.themeIndex);
  append("colorful_ui=%u\n", settings.colorfulUi ? 1 : 0);
  append("corner_radius=%u\n", settings.cornerRadius);
  append("batteryInstalled=%u\n", settings.batteryInstalled);
  append("autosave=%u\n", settings.autosave ? 1 : 0);
  append("keyboard_on_focus=%u\n", settings.keyboardOnFocus ? 1 : 0);
  append("autosave_delay=%u\n", settings.autosaveDelaySec);
  append("show_hidden=%u\n",Tab5Files::showHidden?1:0);
  append("audio_volume=%u\n", settings.audioVolume);
  append("mouse_speed=%u\n", settings.mouse.speed);
  append("mouse_scroll=%u\n", settings.mouse.scroll);
  append("mouse_left=%u\n", settings.mouse.leftHanded);
  append("mouse_natural=%u\n", settings.mouse.natural);

  append("ntp_enabled=%u\n", settings.ntpEnabled ? 1 : 0);
  append("time_zone=%u\n", settings.timeZoneIndex);
  append("documents_root=%s\n", settings.documentsRoot.c_str());
  append("last_reader=%s\n", lastReaderPath.c_str());
  errno=0;f.flush();const bool flushed=!errno;
  bool written = flushed && complete && f.size() == expected && expected > 0 && !f.getWriteError();
  errno=0;f.close();written=written&&!errno;
  if (!written){tab5StorageFault("réglages écriture",temporary.c_str());return false;}
  return tab5AtomicReplace(temporary.c_str(),SETTINGS_FILE,backup.c_str());
}

static void loadSettings() {
  settingsReadComplete=false;
  systemMigrationSettingsPending=true;
  if (!sdReady) return;
  const String backup = String(SETTINGS_FILE) + ".bak";
  auto probe = [](const char* path, bool& present) {
    errno=0;present=tab5FS.exists(path);
    return present || errno==ENOENT;
  };
  bool primaryExists=false,backupExists=false;
  if(!probe(SETTINGS_FILE,primaryExists))return;
  if(!primaryExists&&!probe(backup.c_str(),backupExists))return;
  if(!primaryExists&&!backupExists){
    settingsReadComplete=true;systemMigrationSettingsPending=false;return;
  }
  errno=0;File f=tab5FS.open(primaryExists?SETTINGS_FILE:backup.c_str(),FILE_READ);
  if(!f)return;
  errno=0;const size_t expected=f.size();
  if(f.isDirectory()||errno||expected>64U*1024U){f.close();return;}
  // Read into a transaction: an interrupted read cannot partly replace the
  // active settings or later overwrite the user's original with defaults.
  AppSettings loadedSettings=settings;
  String loadedReaderPath=lastReaderPath;
  bool loadedShowHidden=Tab5Files::showHidden;
  if(loadedSettings.documentsRoot.length()!=settings.documentsRoot.length()||
      loadedReaderPath.length()!=lastReaderPath.length()){f.close();return;}
  uint8_t loadedSettingsVersion=0;
  bool complete=true;
  size_t readPosition=0;
  while(complete&&readPosition<expected){
    char buffer[512];size_t used=0;
    while(readPosition<expected){
      const int c=f.read();
      if(c<0){complete=false;break;}
      ++readPosition;
      if(c=='\n')break;
      if(used==sizeof(buffer)-1){complete=false;break;}
      buffer[used++]=static_cast<char>(c);
    }
    if(!complete)break;
    buffer[used]=0;
    String line(buffer);
    if(line.length()!=used){complete=false;break;}
    line.trim();
    const int eq=line.indexOf('=');
    if(eq<1)continue;
    String key=line.substring(0,eq),value=line.substring(eq+1);
    if(key.length()!=static_cast<size_t>(eq)||value.length()!=line.length()-eq-1){complete=false;break;}
    value.trim();
    if (key == "settings_version") loadedSettingsVersion = constrain(value.toInt(), 0, 255);
    else if (key == "screen_upside_down") loadedSettings.screenUpsideDown = value.toInt() == 1;
    else if (key == "brightness") loadedSettings.brightness = constrain(value.toInt(), 20, 255);
    else if (key == "screen_sleep") loadedSettings.screenSleepIndex = constrain(value.toInt(), 0, 4);
    else if (key == "wifi_power_save") loadedSettings.wifiPowerSave = value.toInt() != 0;
    else if (key == "interface_font" && (!value.length()||Tab5FontDefaults::validPath(value.c_str()))) snprintf(loadedSettings.interfaceFontPath,sizeof(loadedSettings.interfaceFontPath),"%s",value.c_str());
    else if (key == "document_font" && (!value.length()||Tab5FontDefaults::validPath(value.c_str()))) snprintf(loadedSettings.documentFontPath,sizeof(loadedSettings.documentFontPath),"%s",value.c_str());
    else if (key == "font") loadedSettings.fontIndex = constrain(value.toInt(), 0, 3);
    else if (key == "theme") loadedSettings.themeIndex = constrain(value.toInt(), 0, THEME_COUNT - 1);
    else if (key == "colorful_ui") loadedSettings.colorfulUi = value.toInt() != 0;
    else if (key == "corner_radius") loadedSettings.cornerRadius = constrain(value.toInt(), 0, 24);
    else if (key == "batteryInstalled") loadedSettings.batteryInstalled = value.toInt() != 0;
    else if (key == "autosave") loadedSettings.autosave = value.toInt() != 0;
    else if (key == "keyboard_on_focus") loadedSettings.keyboardOnFocus = value.toInt() != 0;
    else if (key == "show_hidden") loadedShowHidden=value.toInt()!=0;
    else if (key == "autosave_delay") loadedSettings.autosaveDelaySec = constrain(value.toInt(), 2, 30);
    else if (key == "mouse_speed") loadedSettings.mouse.speed=constrain(value.toInt(),25,300);
    else if (key == "mouse_scroll") loadedSettings.mouse.scroll=constrain(value.toInt(),1,12);
    else if (key == "mouse_left") loadedSettings.mouse.leftHanded=value.toInt()!=0;
    else if (key == "mouse_natural") loadedSettings.mouse.natural=value.toInt()!=0;
    else if (key == "audio_volume") loadedSettings.audioVolume = constrain(value.toInt(), 0, 255);
    else if (key == "ntp_enabled") loadedSettings.ntpEnabled = value.toInt() != 0;
    // Migration transparente depuis la v0.3.4 : seul son mode NTP active NTP.
    else if (key == "time_source") loadedSettings.ntpEnabled = value.toInt() == 2;
    else if (key == "time_zone") loadedSettings.timeZoneIndex = constrain(value.toInt(), 0, static_cast<int>(TIME_ZONE_COUNT - 1));
    else if (key == "documents_root") loadedSettings.documentsRoot = std::move(value);
    else if (key == "last_reader") loadedReaderPath = std::move(value);
  }
  errno=0;
  settingsReadComplete=complete&&readPosition==expected&&f.position()==expected&&f.size()==expected&&!errno;
  f.close();
  if(!settingsReadComplete)return;
  displayOrientationPending|=settings.screenUpsideDown!=loadedSettings.screenUpsideDown;
  settings=std::move(loadedSettings);
  lastReaderPath=std::move(loadedReaderPath);
  Tab5Files::showHidden=loadedShowHidden;
  systemMigrationSettingsPending=false;
  // A saved zero remains an intentional mute. Only the format marker and
  // an empty documents directory need migration after a complete read.
  if(loadedSettingsVersion<SETTINGS_FORMAT_VERSION){
    if(!settings.documentsRoot.length())settings.documentsRoot="/";
    settingsDirty=true;lastSettingsChangeMs=millis();
  }
}


static bool loadSettingsSources() {
  settingsReadComplete=false;systemMigrationSettingsPending=true;
  if(!loadCustomThemes()){
    errno=0;const bool present=tab5FS.exists("/System/themes.json");
    if(present||errno!=ENOENT)return false;
  }
  loadSettings();
  return settingsReadComplete;
}

static bool reloadSettingsAfterMigration() {
  if(!systemMigrationSettingsPending)return true;
  if(!loadSettingsSources())return false;
  systemMigrationSettingsPending=false;settingsDirty=false;
  Tab5MediaSession::seedLegacyVolume(settings.audioVolume,millis());
  settings.audioVolume=Tab5MediaSession::volume();
  Tab5Audio.setVolume(settings.audioVolume);
  return true;
}

static void markSettingsDirty() {
  settingsDirty = true;
  lastSettingsChangeMs = millis();
}

// -----------------------------------------------------------------------------
// RTC + filesystem time
// -----------------------------------------------------------------------------

static bool parseCompileDate(m5::rtc_datetime_t& dt) {
  static const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  char mon[4] = {__DATE__[0], __DATE__[1], __DATE__[2], 0};
  const char* m = strstr(months, mon);
  if (!m) return false;
  dt.date.year = atoi(__DATE__ + 7);
  dt.date.month = static_cast<uint8_t>((m - months) / 3 + 1);
  dt.date.date = atoi(__DATE__ + 4);
  dt.time.hours = atoi(__TIME__);
  dt.time.minutes = atoi(__TIME__ + 3);
  dt.time.seconds = atoi(__TIME__ + 6);
  return true;
}

static bool validRtc(const m5::rtc_datetime_t& dt) {
  return dt.date.year >= 2024 && dt.date.year <= 2099 &&
         dt.date.month >= 1 && dt.date.month <= 12 && dt.date.date >= 1 && dt.date.date <= 31 &&
         dt.time.hours < 24 && dt.time.minutes < 60 && dt.time.seconds < 60;
}

static void applySelectedTimeZone() {
  setenv("TZ", selectedTimeZone().posix, 1);
  tzset();
}

static bool systemTimeValid() {
  return time(nullptr) >= 1704067200;
}

static void setSystemTimeFromDateTime(const m5::rtc_datetime_t& dt) {
  applySelectedTimeZone();
  struct tm t = {};
  t.tm_year = dt.date.year - 1900;
  t.tm_mon = dt.date.month - 1;
  t.tm_mday = dt.date.date;
  t.tm_hour = dt.time.hours;
  t.tm_min = dt.time.minutes;
  t.tm_sec = dt.time.seconds;
  t.tm_isdst = -1;
  const time_t epoch = mktime(&t);
  if (epoch > 0) {
    timeval tv{epoch, 0};
    settimeofday(&tv, nullptr);
  }
}

static bool getSystemDateTime(m5::rtc_datetime_t& dt) {
  const time_t now = time(nullptr);
  if (now < 1704067200) return false;
  struct tm local = {};
  if (!localtime_r(&now, &local)) return false;
  dt.date.year = local.tm_year + 1900;
  dt.date.month = local.tm_mon + 1;
  dt.date.date = local.tm_mday;
  dt.time.hours = local.tm_hour;
  dt.time.minutes = local.tm_min;
  dt.time.seconds = local.tm_sec;
  return validRtc(dt);
}

#if TAB5OS_HAS_SNTP_CALLBACK
static void networkTimeReceived(struct timeval*) {
  ntpTimeReceived = true;
}
#endif

static void stopNetworkTime() {
#if TAB5OS_HAS_SNTP_CALLBACK
  if (ntpServiceStarted) esp_sntp_stop();
  ntpTimeReceived = false;
#endif
  ntpServiceStarted = false;
  ntpAwaitingSync = false;
}

static void requestNetworkTime() {
  if (!settings.ntpEnabled) return;
  if (!Tab5Audio.wifiEnabled() || !Tab5Audio.wifiConnected()) {
    Tab5DebugSerial.println("[NTP] en attente du Wi-Fi");
    return;
  }
  stopNetworkTime();
  applySelectedTimeZone();
#if TAB5OS_HAS_SNTP_CALLBACK
  sntp_set_time_sync_notification_cb(networkTimeReceived);
  ntpTimeReceived = false;
#endif
  configTzTime(selectedTimeZone().posix, "pool.ntp.org",
               "time.cloudflare.com");
  ntpServiceStarted = true;
  ntpAwaitingSync = true;
  ntpConfiguredAtMs = millis();
  Tab5DebugSerial.println("[NTP] service démarré");
}

static bool getRtcDateTime(m5::rtc_datetime_t& dt) {
  return rtcReady && M5.Rtc.getDateTime(&dt);
}

static bool loadSystemTimeFromRtc() {
  m5::rtc_datetime_t dt = {};
  if (!getRtcDateTime(dt) || !validRtc(dt)) return false;
  setSystemTimeFromDateTime(dt);
  return true;
}

static bool writeRtcFromSystemTime() {
  if (!rtcReady) return false;
  m5::rtc_datetime_t dt = {};
  if (!getSystemDateTime(dt)) return false;
  M5.Rtc.setDateTime(dt);
  return true;
}

static void applyClockPolicy() {
  // Au démarrage ntpServiceStarted est faux. Cette fonction remet seulement
  // notre état local à zéro et n'appelle donc pas lwIP si le Wi-Fi est coupé.
  stopNetworkTime();
  applySelectedTimeZone();

  // Le RTC matériel est la base de l'horloge du Tab5, sans mode concurrent.
  // La date de compilation ne sert qu'à initialiser un RTC encore vide.
  if (!loadSystemTimeFromRtc()) {
    m5::rtc_datetime_t build = {};
    if (parseCompileDate(build)) {
      setSystemTimeFromDateTime(build);
      if (rtcReady) M5.Rtc.setDateTime(build);
    }
  }

  const bool connected = Tab5Audio.wifiConnected();
  wifiWasConnectedForRtc = connected;
  if (settings.ntpEnabled && connected) requestNetworkTime();
  else if (settings.ntpEnabled)
    Tab5DebugSerial.println("[NTP] activé, attente d'une connexion Wi-Fi");
  lastClockUpdateMs = 0;
}

static void serviceNetworkRtc() {
  static uint32_t lastCheckMs = 0;
  const uint32_t now = millis();
  if (now - lastCheckMs < 250U) return;
  lastCheckMs = now;

  const bool connected = Tab5Audio.wifiConnected();
  if (!settings.ntpEnabled) {
    if (ntpServiceStarted) stopNetworkTime();
    wifiWasConnectedForRtc = connected;
    return;
  }

  if (connected && !wifiWasConnectedForRtc) requestNetworkTime();
  wifiWasConnectedForRtc = connected;
  if (!connected) {
    // Une perte de point d'accès laisse normalement la pile lwIP active. On
    // arrête alors SNTP tant que cette pile existe encore ; l'interrupteur
    // Wi-Fi l'arrête explicitement avant de désinitialiser le pilote.
    if (ntpServiceStarted && Tab5Audio.wifiEnabled()) stopNetworkTime();
    return;
  }
  if (!ntpAwaitingSync) return;

#if TAB5OS_HAS_SNTP_CALLBACK
  if (ntpTimeReceived) {
    ntpTimeReceived = false;
    if (systemTimeValid()) {
      writeRtcFromSystemTime();
      stopNetworkTime();
      lastClockUpdateMs = 0;
      Tab5DebugSerial.println("[NTP] RTC synchronisé");
    }
  }
#else
  if (now - ntpConfiguredAtMs >= 10000U && systemTimeValid()) {
    writeRtcFromSystemTime();
    stopNetworkTime();
    lastClockUpdateMs = 0;
    Tab5DebugSerial.println("[NTP] RTC synchronisé");
  }
#endif
}

static void initRtc() {
  rtcReady = M5.Rtc.isEnabled();
  applyClockPolicy();
}

static String clockString(bool withDate = false) {
  m5::rtc_datetime_t dt = {};
  if (!getSystemDateTime(dt))
    return withDate ? String(T("Heure non réglée")) : String("--:--");
  char b[40];
  if (withDate)
    snprintf(b, sizeof(b), "%02u/%02u/%04u  %02u:%02u",
             dt.date.date, dt.date.month, dt.date.year,
             dt.time.hours, dt.time.minutes);
  else
    snprintf(b, sizeof(b), "%02u:%02u", dt.time.hours, dt.time.minutes);
  return String(b);
}

static String topBarDateString() {
  m5::rtc_datetime_t dt = {};
  if (!getSystemDateTime(dt)) return "--/--/----";
  char b[16];
  snprintf(b, sizeof(b), "%02u/%02u/%04u",
           dt.date.date, dt.date.month, dt.date.year);
  return String(b);
}

static const char* batterySymbolFor(int32_t level, bool charging) {
  if (charging) return LV_SYMBOL_CHARGE;
  if (level <= 10) return LV_SYMBOL_BATTERY_EMPTY;
  if (level <= 35) return LV_SYMBOL_BATTERY_1;
  if (level <= 60) return LV_SYMBOL_BATTERY_2;
  if (level <= 85) return LV_SYMBOL_BATTERY_3;
  return LV_SYMBOL_BATTERY_FULL;
}

static void updateBatteryLabel(bool force) {
  if (!batteryLabel) return;
  const uint32_t now = millis();
  if (!force && now - lastBatteryUpdateMs < 5000) return;
  lastBatteryUpdateMs = now;
  if (!settings.batteryInstalled) {
    setLabelIfChanged(batteryLabel, LV_SYMBOL_USB " USB");
    if (activePage == Page::Settings) refreshSystemInfo();
    return;
  }
  int32_t level = M5.Power.getBatteryLevel();
  if (level < 0 || level > 100) {
    setLabelIfChanged(batteryLabel, LV_SYMBOL_BATTERY_EMPTY " --");
    return;
  }
  const bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  char b[28];
  snprintf(b, sizeof(b), "%s %ld%%", batterySymbolFor(level, charging),
           static_cast<long>(level));
  setLabelIfChanged(batteryLabel, b);
  if(activePage==Page::Settings)refreshSystemInfo();
}

static bool setClockFromString(String s) {
  s.trim();
  int y=0, mo=0, d=0, h=0, mi=0;
  if (sscanf(s.c_str(), "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) != 5) return false;
  if (y < 2024 || y > 2099 || mo < 1 || mo > 12 || d < 1 || d > 31 || h < 0 || h > 23 || mi < 0 || mi > 59) return false;
  m5::rtc_datetime_t dt = {};
  dt.date.year = y; dt.date.month = mo; dt.date.date = d;
  dt.time.hours = h; dt.time.minutes = mi; dt.time.seconds = 0;
  setSystemTimeFromDateTime(dt);
  if (rtcReady) M5.Rtc.setDateTime(dt);
  return true;
}

static void updateClockLabels(bool force) {
  uint32_t now = millis();
  if (!force && now - lastClockUpdateMs < 1000) return;
  lastClockUpdateMs = now;
  String t = clockString(false);
  String date = topBarDateString();
  if (dateLabel&&(force||strcmp(lv_label_get_text(dateLabel),date.c_str()))){
    setLabelIfChanged(dateLabel,date.c_str());Tab5TopBarLayout::date(dateLabel,topAudioIndicator,SCREEN_W);
  }
  if (clockLabel) setLabelIfChanged(clockLabel, t.c_str());
  if (rtcValue) {
    const String current = clockString(true);
    setLabelIfChanged(rtcValue, current.c_str());
  }
  if (topWifiLabel) {
    bool enabled=false,connected=false;
    if (Tab5Audio.wifiSnapshot(enabled,connected)) {
    static int previousWifiState = -1;
    const int nextWifiState = connected ? 2 : enabled ? 1 : 0;
    if (force || previousWifiState != nextWifiState) {
      previousWifiState = nextWifiState;
      setLabelIfChanged(topWifiLabel, LV_SYMBOL_WIFI);
      lv_obj_set_style_text_color(topWifiLabel, connected ? C_TEXT() : C_MUTED(), 0);
      lv_obj_set_style_opa(topWifiLabel,
          connected ? LV_OPA_COVER : (enabled ? LV_OPA_60 : LV_OPA_30), 0);
    }
    }
  }
  if (readerClockLabel) setLabelIfChanged(readerClockLabel, t.c_str());
  updateBatteryLabel(force);
}

static String formatFileTime(time_t ts) {
  if (ts <= 0) return T("date inconnue");
  struct tm tmv;
  localtime_r(&ts, &tmv);
  char b[32];
  snprintf(b, sizeof(b), "%02d/%02d/%04d %02d:%02d",
           tmv.tm_mday, tmv.tm_mon + 1, tmv.tm_year + 1900, tmv.tm_hour, tmv.tm_min);
  return String(b);
}

// -----------------------------------------------------------------------------
// Undo / redo history
// -----------------------------------------------------------------------------

static uint32_t hashText(const char* s) {
  uint32_t h = 2166136261UL;
  if (!s) return h;
  while (*s) { h ^= static_cast<uint8_t>(*s++); h *= 16777619UL; }
  return h;
}

static void freeHistorySnapshot(HistorySnapshot& h) {
  if (h.text) {
    historyBytes -= h.len + 1;
    heap_caps_free(h.text);
  }
  h.text = nullptr; h.len = 0; h.cursor = 0;
}

static void clearHistory() {
  for (int i = 0; i < historyCount; ++i) freeHistorySnapshot(history[i]);
  historyCount = 0; historyIndex = -1; historyBytes = 0; historyPending = false;
  updateUndoRedoState();
}

static void dropOldestHistory() {
  if (historyCount <= 0) return;
  freeHistorySnapshot(history[0]);
  for (int i = 1; i < historyCount; ++i) history[i - 1] = history[i];
  history[historyCount - 1] = {};
  --historyCount;
  --historyIndex;
  if (historyIndex < 0 && historyCount) historyIndex = 0;
}

static void pushHistorySnapshot(bool force) {
  if (!editor || restoringHistory || loadingDocument) return;
  const char* text = lv_textarea_get_text(editor);
  const size_t len = strlen(text);

  if (!force && historyIndex >= 0 && history[historyIndex].len == len &&
      history[historyIndex].text && memcmp(history[historyIndex].text, text, len) == 0) {
    historyPending = false;
    return;
  }
  if (historyIndex >= 0 && history[historyIndex].len == len && history[historyIndex].text &&
      memcmp(history[historyIndex].text, text, len) == 0) {
    historyPending = false;
    return;
  }

  // Any edit after an Undo invalidates the Redo branch.
  while (historyCount - 1 > historyIndex) {
    freeHistorySnapshot(history[historyCount - 1]);
    --historyCount;
  }

  while (historyCount >= HISTORY_MAX || (historyCount > 1 && historyBytes + len + 1 > HISTORY_BUDGET)) {
    dropOldestHistory();
  }

  char* copy = static_cast<char*>(heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!copy) { historyPending = false; return; }
  memcpy(copy, text, len + 1);

  if (historyCount >= HISTORY_MAX) dropOldestHistory();
  history[historyCount].text = copy;
  history[historyCount].len = len;
  history[historyCount].cursor = lv_textarea_get_cursor_pos(editor);
  historyBytes += len + 1;
  ++historyCount;
  historyIndex = historyCount - 1;
  historyPending = false;
  updateUndoRedoState();
}

static void resetHistory() {
  clearHistory();
  pushHistorySnapshot(true);
}

static void restoreHistoryAt(int idx) {
  if (!editor || idx < 0 || idx >= historyCount || !history[idx].text) return;
  restoringHistory = true;
  loadingDocument = true;
  if (previewLabel) lv_label_set_text(previewLabel, "");
  lv_textarea_set_text(editor, history[idx].text);
  lv_textarea_set_cursor_pos(editor, history[idx].cursor);
  loadingDocument = false;
  restoringHistory = false;
  historyIndex = idx;
  documentDirty = hashText(lv_textarea_get_text(editor)) != savedTextHash;
  statsPending = true;
  updateDocumentTitle();
  updateEditorStatus(true);
  updateUndoRedoState();
}

static void updateUndoRedoState() {
  if (undoBtn) {
    if (historyIndex > 0 || historyPending) lv_obj_remove_state(undoBtn, LV_STATE_DISABLED);
    else lv_obj_add_state(undoBtn, LV_STATE_DISABLED);
  }
  if (redoBtn) {
    if (!historyPending && historyIndex >= 0 && historyIndex < historyCount - 1) lv_obj_remove_state(redoBtn, LV_STATE_DISABLED);
    else lv_obj_add_state(redoBtn, LV_STATE_DISABLED);
  }
}

// -----------------------------------------------------------------------------
// SD / document handling
// -----------------------------------------------------------------------------

static bool atomicWriteText(const String& path, const char* text, size_t len) {
  if (!Tab5Files::userWritable(path.c_str())) { toast("Chemin invalide"); return false; }
  String tmp = path + ".tmp";
  String bak = path + ".bak";

  tab5FS.remove(tmp.c_str());
  File out = tab5FS.open(tmp.c_str(), FILE_WRITE);
  if (!out) return false;

  const uint8_t* ptr = reinterpret_cast<const uint8_t*>(text);
  size_t written = 0;
  while (written < len) {
    size_t remaining = len - written;
    size_t chunk = (remaining < 8192) ? remaining : 8192;
    size_t n = out.write(ptr + written, chunk);
    if (n != chunk) {
      out.close();
      tab5FS.remove(tmp.c_str());
      return false;
    }
    written += n;
    delay(0);
  }
  errno=0;out.flush();const bool flushed=!errno;
  bool complete=flushed&&out.size()==len&&!out.getWriteError();
  errno=0;out.close();complete=complete&&!errno;
  if(!complete||!tab5AtomicReplace(tmp.c_str(),path.c_str(),bak.c_str()))return false;
  return true;
}

static bool saveDocumentTo(const String& path, bool showToast) {
  if (!pathStorageReady(path) || path.length() == 0 || editor == nullptr) return false;
  const char* text = lv_textarea_get_text(editor);
  size_t len = strlen(text);
  if (len > MAX_DOCUMENT_BYTES) {
    if (showToast) toast("Document trop volumineux");
    return false;
  }

  if (!atomicWriteText(path, text, len)) {
    if (showToast) toast("Échec de sauvegarde");
    return false;
  }

  currentPath = path;
  currentDir = parentDirOf(path);
  savedTextHash = hashText(text);
  documentDirty = false;
  lastSaveMs = millis();
  if (historyPending) pushHistorySnapshot(true);
  updateDocumentTitle();
  updateEditorStatus(true);
  if (showToast) toast("Sauvegarde OK");
  return true;
}

static bool commitCurrentBeforeLeave() {
  if (!documentDirty) return true;

  if (currentPath.length() == 0) {
    toast("Enregistrez d'abord le document");
    showPage(Page::Editor);
    return false;
  }

  if (!saveDocumentTo(currentPath, false)) {
    toast("Sauvegarde impossible");
    showPage(Page::Editor);
    return false;
  }
  return true;
}

static bool loadDocumentFrom(const String& path,bool explicitAttempt=false) {
  if (!Tab5Files::userWritable(path.c_str())) { toast("Chemin invalide"); return false; }
  if (!pathStorageReady(path)) return false;
  if (!explicitAttempt && !isReadableTextFile(path)) {
    toast("Ce fichier n’est pas du texte UTF-8");
    return false;
  }
  if (!commitCurrentBeforeLeave()) return false;
  setKeyboardVisible(false);
  if (Tab5WritingApp::codeFile(path.c_str())) warnSystemFile(path);
  if (!Tab5Writing.open(path.c_str())) return false;
  currentDir = parentDirOf(path);
  showPage(Page::Writing);
  return true;
}

static void newDocument() {
  setKeyboardVisible(false);
  loadingDocument = true;
  if (previewLabel) lv_label_set_text(previewLabel, "");
  lv_textarea_set_text(editor, "");
  loadingDocument = false;
  currentPath = "";
  savedTextHash = hashText("");
  documentDirty = false;
  statsPending = true;
  resetHistory();
  updateDocumentTitle();
  updateEditorStatus(true);
  updateClockLabels(true);
  showPage(Page::Editor);
  lv_obj_add_state(editor, LV_STATE_FOCUSED);
  setKeyboardVisible(!writing::usbConnected(), editor);
}

// -----------------------------------------------------------------------------
// LVGL hardware port
// -----------------------------------------------------------------------------

static void displayFlush(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
  static bool frameTransaction = false;
  static bool frameDrawable = false;
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  if (!frameTransaction) {
    frameDrawable = tab5DisplayCacheBeginFrame();
    if (frameDrawable) M5.Display.startWrite();
    frameTransaction = true;
  }
  if (frameDrawable) {
    tab5DisplayCacheDirtyRect(area->x1, area->y1, w, h);
    const lv_draw_buf_t* source = lv_display_get_buf_active(disp);
    const size_t stride = source ? source->header.stride : size_t(w) * sizeof(uint16_t);
    const size_t bytes = (size_t(h) - 1) * stride + size_t(w) * sizeof(uint16_t);
    if (!tab5DisplayPresentWritePixels(area->x1, area->y1, w, h, pxMap,
        bytes, stride)) {
      if (stride == size_t(w) * sizeof(uint16_t))
        M5.Display.pushImage(area->x1, area->y1, w, h, reinterpret_cast<uint16_t*>(pxMap));
      else for (int32_t row = 0; row < h; ++row)
        M5.Display.pushImage(area->x1, area->y1 + row, w, 1,
            reinterpret_cast<uint16_t*>(pxMap + size_t(row) * stride));
    }
    Tab5DisplayActivity::flushed(w, h, lv_display_flush_is_last(disp), millis());
  }
  if (lv_display_flush_is_last(disp)) {
    if (frameDrawable) {
      M5.Display.endWrite();
      tab5DisplayCacheEndFrame();
    }
    frameTransaction = false;
  }
  // Transfers complete synchronously; the refresh gate retains invalidations
  // while the DSI framebuffer is still owned by the panel.
  lv_display_flush_ready(disp);
}

static void touchRead(lv_indev_t* indev, lv_indev_data_t* data) {
  (void)indev;
  static int16_t lastX = 0, lastY = 0;
  const int w = lv_display_get_horizontal_resolution(lvDisplay);
  const int h = lv_display_get_vertical_resolution(lvDisplay);

  if (M5.Touch.getCount() > 0) {
    auto t = M5.Touch.getDetail(0);
    if (t.isPressed() && !screenWakeTouch) {
      int16_t x = constrain(t.x, 0, w - 1);
      int16_t y = constrain(t.y, 0, h - 1);
      lastX = x; lastY = y;
      data->point.x = x; data->point.y = y;
      data->state = LV_INDEV_STATE_PRESSED;
      return;
    }
  }
  data->point.x = lastX; data->point.y = lastY;
  data->state = LV_INDEV_STATE_RELEASED;
}

// M5Unified converts raw touch coordinates using the active M5GFX rotation.
// Defer until the triggering gesture ends; no extra framebuffer or LVGL rotation.
static void serviceDisplayOrientation() {
  if (!displayOrientationPending || !lvDisplay) return;
  for (size_t i=0;i<M5.Touch.getCount();++i) if(M5.Touch.getDetail(i).isPressed()) return;
  for (auto* input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input))
    if(lv_indev_get_type(input)==LV_INDEV_TYPE_POINTER&&lv_indev_get_state(input)==LV_INDEV_STATE_PRESSED) return;
  displayOrientationPending=false;
  const bool portrait=readerActive||Tab5PythonReader::visible();
  M5.Display.setRotation(Tab5DisplayOrientation::rotation(portrait,settings.screenUpsideDown));
  for(auto* input=lv_indev_get_next(nullptr);input;input=lv_indev_get_next(input))lv_indev_reset(input,nullptr);
  if(auto* screen=lv_screen_active())lv_obj_invalidate(screen);
  lv_obj_invalidate(lv_layer_top());lv_obj_invalidate(lv_layer_sys());
}

static bool initLvgl() {
  lv_init();
  // Input and timers use the same monotonic clock, even during long callbacks.
  lv_tick_set_cb([]()->uint32_t{return millis();});

  lvDisplay = lv_display_create(SCREEN_W, SCREEN_H);
  if (!lvDisplay) return false;
  // M5GFX/Tab5 expects the RGB565 byte order opposite to LVGL's native buffer.
  // Without this, neutral greys turn purple/green and antialiased text gets
  // coloured fringes. Let LVGL produce the correctly swapped buffer natively.
  lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565_SWAPPED);
  lv_display_set_flush_cb(lvDisplay, displayFlush);
  Tab5DisplayActivity::begin(lvDisplay);

  // pushImage() is synchronous in this port. A second LVGL draw buffer
  // cannot overlap any transfer and only doubles the resident allocation.
  // 32-line slices bound each PSRAM copy. M5GFX owns the panel framebuffers.
  lvBufBytes = SCREEN_W * 32U * sizeof(uint16_t);
  lvBuf1 = static_cast<uint8_t*>(heap_caps_malloc(lvBufBytes,
      MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  lvBuf2 = nullptr;
  if (!lvBuf1) return false;
  lv_display_set_buffers(lvDisplay, lvBuf1, nullptr, lvBufBytes,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  tab5LogMemory("LVGL: tampon unique 80 Kio PSRAM");

  lvTouch = lv_indev_create();
  if (!lvTouch) return false;
  lv_indev_set_type(lvTouch, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lvTouch, touchRead);
  lv_indev_set_mode(lvTouch,LV_INDEV_MODE_EVENT);
  lv_indev_set_display(lvTouch, lvDisplay);
  Tab5UiStyle::touchInput(lvTouch);
  Tab5Mouse::configure(settings.mouse);
  Tab5Mouse::begin(lvDisplay);
  Tab5Mouse::contextMenu(showContextMenu);
  Tab5Mouse::pointerEvents(imageMouseEvent);
  Tab5Mouse::relativeEvents(Tab5NativeView::mouse);
  Tab5ContextMenu::configure(uiFont,[](const char* text){return T(text);});
  return true;
}

// -----------------------------------------------------------------------------
// UI helpers
// -----------------------------------------------------------------------------

static lv_obj_t* makeButton(lv_obj_t* parent, const char* text, int w, int h) {
  lv_obj_t* btn = lv_button_create(parent);
  lv_obj_set_size(btn, w, h);
  lv_obj_set_style_radius(btn, settings.cornerRadius, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(btn, C_RAISED(), 0);
  lv_obj_set_style_bg_color(btn, C_ACTIVE(), LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(btn, C_ACTIVE(), LV_STATE_CHECKED);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, C_BORDER(), 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_outline_width(btn, 0, 0);
  lv_obj_set_style_text_color(btn, C_TEXT(), 0);
  lv_obj_set_style_text_color(btn, C_TEXT(), LV_STATE_CHECKED);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, T(text));
  lv_obj_set_style_text_color(label, C_TEXT(), 0);
  lv_obj_set_style_text_font(label, uiFont(), 0);
#if LVGL_VERSION_MAJOR > 9 || (LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 4)
  lv_obj_set_style_text_outline_stroke_width(label, 0, 0);
#endif
  lv_obj_center(label);
  const char* hint=text;
  if(!strcmp(text,LV_SYMBOL_SAVE))hint="Enregistrer";
  else if(!strcmp(text,LV_SYMBOL_DIRECTORY))hint="Dossiers";
  else if(!strcmp(text,LV_SYMBOL_EDIT))hint="Modifier le texte";
  else if(!strcmp(text,LV_SYMBOL_EYE_OPEN))hint="Lire";
  else if(!strcmp(text,LV_SYMBOL_PLAY))hint="Lire / lancer";
  else if(!strcmp(text,LV_SYMBOL_PAUSE))hint="Pause / reprendre";
  else if(!strcmp(text,LV_SYMBOL_STOP))hint="Arrêter";
  else if(!strcmp(text,LV_SYMBOL_LEFT))hint="Revenir / précédent";
  else if(!strcmp(text,LV_SYMBOL_RIGHT))hint="Suivant";
  else if(!strcmp(text,LV_SYMBOL_PLUS))hint="Ajouter";
  else if(!strcmp(text,LV_SYMBOL_CLOSE))hint="Fermer · Échap";
  else if(!strcmp(text,LV_SYMBOL_PASTE))hint="Coller";
  else if(!strcmp(text,LV_SYMBOL_COPY))hint="Copier";
  else if(!strcmp(text,LV_SYMBOL_CUT))hint="Couper";
  else if(!strcmp(text,LV_SYMBOL_SETTINGS))hint="Réglages";
  else if(!strcmp(text,LV_SYMBOL_HOME))hint="Accueil";
  else if(!strcmp(text,LV_SYMBOL_AUDIO))hint="Audio";
  else if(!strcmp(text,LV_SYMBOL_DOWNLOAD))hint="Télécharger";
  else if(!strcmp(text,LV_SYMBOL_FILE))hint="Document";
  else if(!strcmp(text,LV_SYMBOL_GPS))hint="Position sur la carte";
  else if(!strcmp(text,LV_SYMBOL_IMAGE))hint="Image";
  else if(!strcmp(text,LV_SYMBOL_KEYBOARD))hint="Clavier tactile";
  else if(!strcmp(text,LV_SYMBOL_LIST))hint="Liste / plan";
  else if(!strcmp(text,LV_SYMBOL_MINUS))hint="Réduire";
  else if(!strcmp(text,LV_SYMBOL_NEXT))hint="Suivant";
  else if(!strcmp(text,LV_SYMBOL_PREV))hint="Précédent";
  else if(!strcmp(text,LV_SYMBOL_OK))hint="Valider";
  else if(!strcmp(text,LV_SYMBOL_REFRESH))hint="Actualiser";
  else if(!strcmp(text,LV_SYMBOL_TRASH))hint="Supprimer";
  else if(!strcmp(text,LV_SYMBOL_UP))hint="Remonter";
  else if(!strcmp(text,LV_SYMBOL_VOLUME_MAX))hint="Volume";
  else if(!strcmp(text,LV_SYMBOL_WIFI))hint="Wi-Fi";
  Tab5Focus.describe(btn,T(hint));
  Tab5UiStyle::selection(btn);
  return btn;
}

static lv_obj_t* makeIconButton(lv_obj_t* parent, const char* symbol, int w, int h) {
  lv_obj_t* btn = makeButton(parent, symbol, w, h);
  lv_obj_t* label = lv_obj_get_child(btn, 0);
  if (!strcmp(symbol,TAB5_SYMBOL_PRINT)) {
    if(label)lv_obj_add_flag(label,LV_OBJ_FLAG_HIDDEN);
    tab5PrinterIcon(btn,C_TEXT(),C_RAISED());
    Tab5Focus.describe(btn,T("Imprimer"));
  } else if (!strcmp(symbol,TAB5_SYMBOL_LOCK)) {
    if(label)lv_obj_add_flag(label,LV_OBJ_FLAG_HIDDEN);
    tab5LockIcon(btn,C_TEXT(),C_RAISED());
  } else if (label) lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
  return btn;
}

// Microphone drawn from LVGL primitives: it does not depend on an emoji or a
// private font glyph, so it renders identically with every installed TTF.
static lv_obj_t* makeMicrophoneButton(lv_obj_t* parent, int w, int h,
                                      lv_obj_t** graphicOut) {
  lv_obj_t* btn = makeButton(parent, "", w, h);
  lv_obj_t* label = lv_obj_get_child(btn, 0);
  if (label) {
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
  }

  lv_obj_t* graphic = lv_obj_create(btn);
  lv_obj_remove_style_all(graphic);
  lv_obj_set_size(graphic, 34, 40);
  lv_obj_center(graphic);
  lv_obj_remove_flag(graphic, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(graphic, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* capsule = lv_obj_create(graphic);
  lv_obj_set_size(capsule, 15, 23);
  lv_obj_align(capsule, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_bg_opa(capsule, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(capsule, 3, 0);
  lv_obj_set_style_border_color(capsule, C_TEXT(), 0);
  lv_obj_set_style_radius(capsule, LV_RADIUS_CIRCLE, 0);
  lv_obj_remove_flag(capsule, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* stem = lv_obj_create(graphic);
  lv_obj_remove_style_all(stem);
  lv_obj_set_size(stem, 3, 10);
  lv_obj_align(stem, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_set_style_bg_color(stem, C_TEXT(), 0);
  lv_obj_set_style_bg_opa(stem, LV_OPA_COVER, 0);

  lv_obj_t* base = lv_obj_create(graphic);
  lv_obj_remove_style_all(base);
  lv_obj_set_size(base, 20, 3);
  lv_obj_align(base, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(base, C_TEXT(), 0);
  lv_obj_set_style_bg_opa(base, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(base, LV_RADIUS_CIRCLE, 0);

  if (graphicOut) *graphicOut = graphic;
  return btn;
}


static void toastDeleteCb(lv_timer_t* timer) {
  lv_obj_t* obj = static_cast<lv_obj_t*>(lv_timer_get_user_data(timer));
  if (obj && lv_obj_is_valid(obj)) lv_obj_delete(obj);
}

static void toast(const char* text, uint32_t ms) {
  if (!root) return;
  lv_obj_t* box = lv_obj_create(root);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_min_width(box, 260, 0);
  lv_obj_set_style_max_width(box, SCREEN_W - 40, 0);
  lv_obj_set_style_pad_all(box, 10, 0);
  lv_obj_set_style_bg_color(box, C_RAISED(), 0);
  lv_obj_set_style_text_color(box, C_TEXT(), 0);
  lv_obj_set_style_border_color(box, C_BORDER(), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_90, 0);
  lv_obj_set_style_radius(box, 12, 0);
  lv_obj_t* l = lv_label_create(box);
  const String message = translateRuntimeText(String(text));
  lv_obj_set_style_max_width(l, SCREEN_W - 64, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_label_set_text(l, message.c_str());
  lv_obj_center(l);
  lv_obj_align(box, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_move_foreground(box);
  lv_timer_t* t = lv_timer_create(toastDeleteCb, ms, box);
  lv_timer_set_repeat_count(t, 1);
}

static void updateDocumentTitle() {
  if (!titleLabel) return;
  String name = baseNameOf(currentPath);
  if (currentPath.length() == 0) name = T("Sans titre");
  if (activePage == Page::Editor) {
    lv_label_set_text(titleLabel, name.c_str());
    lv_label_set_text(dirtyLabel, documentDirty ? "*" : "");
  } else {
    lv_label_set_text(dirtyLabel, "");
  }
  if (deleteDocBtn) {
    if (currentPath.length()) lv_obj_remove_state(deleteDocBtn, LV_STATE_DISABLED);
    else lv_obj_add_state(deleteDocBtn, LV_STATE_DISABLED);
  }
}

static TextStats computeTextStatsBuffer(const char* s) {
  TextStats st;
  if (!s) return st;
  st.bytes = strlen(s);
  bool inWord = false;
  for (size_t i = 0; i < st.bytes; ++i) {
    const uint8_t c = static_cast<uint8_t>(s[i]);
    if ((c & 0xC0) != 0x80) ++st.chars;
    const bool ws = (c == ' ' || c == '\n' || c == '\r' || c == '\t');
    if (ws) inWord = false;
    else if (!inWord) { ++st.words; inWord = true; }
  }
  return st;
}

static TextStats computeTextStatsFile(File& f) {
  TextStats st;
  if (!f || f.isDirectory()) return st;
  const size_t oldPos = static_cast<size_t>(f.position());
  if (!f.seek(0)) return st;
  uint8_t buf[2048];
  bool inWord = false;
  while (true) {
    const size_t got = f.read(buf, sizeof(buf));
    if (!got) break;
    st.bytes += got;
    for (size_t i = 0; i < got; ++i) {
      const uint8_t c = buf[i];
      if ((c & 0xC0) != 0x80) ++st.chars;
      const bool ws = (c == ' ' || c == '\n' || c == '\r' || c == '\t');
      if (ws) inWord = false;
      else if (!inWord) { ++st.words; inWord = true; }
    }
  }
  f.seek(oldPos);
  return st;
}

static String formatTextStats(const TextStats& st, bool includeModified = false) {
  String out = String(static_cast<unsigned>(st.words)) + " " + T("mots") +
               "  •  " + String(static_cast<unsigned>(st.chars)) + " " +
               T("caractères") + "  •  " +
               String(static_cast<unsigned>(st.bytes)) + " " + T("octets");
  if (includeModified && documentDirty)
    out += String("  •  ") + T("modifié");
  return out;
}

static void updateEditorStatus(bool force) {
  if (!editor) return;
  const uint32_t now = millis();
  if (!force && now - lastStatsMs < 650) return;
  lastStatsMs = now;

  const TextStats st = computeTextStatsBuffer(lv_textarea_get_text(editor));
  if (previewHint) {
    String line = String(T("Aperçu texte")) + "  •  " +
                  formatTextStats(st, true) + "  •  " +
                  T("Lecture en portrait disponible");
    lv_label_set_text(previewHint, line.c_str());
  }
  statsPending = false;
}

static void applyEditorFont() {
  const auto pixels=fontSizeForIndex(settings.fontIndex);
#if LV_USE_TINY_TTF
  if (smoothFontReady && docTtfFont) { resizeConfiguredFont(docTtfFont,docStockFont,pixels); }
#endif
  // The EPUB reader applies this setting and rebuilds pagination when opened.
  // Its shared font can also belong to a Python reader: never mutate it here.
  const lv_font_t* f = documentFont();
  for(auto* object:{editor,previewLabel,filePreviewLabel,fontPreview})Tab5DocumentText::apply(object,f);
  Tab5Writing.setTextSize(pixels);
  if (fontValue) {
    char b[24];
    snprintf(b, sizeof(b), "%u px",pixels);
    lv_label_set_text(fontValue, b);
  }
}

static void setKeyboardVisible(bool visible, lv_obj_t* target) {
  if (!keyboardPanel) return;
  keyboardVisible = visible;
  const unsigned nextNumberMode=Tab5NumericKeyboard::mode(target?target:keyboardTarget);
  const bool numberModeChanged=keyboardNumberMode!=nextNumberMode;
  keyboardNumberMode=nextNumberMode;
  if (activePage == Page::Writing) Tab5Writing.resize(visible ? PAGE_H - keyboardHeight() : PAGE_H);
  if (activePage == Page::ImageEditor) Tab5ImageEditor::resize(WORK_W,visible ? PAGE_H - keyboardHeight() : PAGE_H);
  if (target) {
    keyboardTarget = target;
    if (lv_obj_check_type(target, &lv_textarea_class)) {
      keyboardUiNavigation = false; keyboardDockFocus = false;
      // A touch/mouse click supersedes the last Tab destination for every
      // dispatcher, including IME and application-owned modal forms.
      Tab5Focus.editObject(target);
      // Remember a target only while it exists. Rebuilding a dialog cannot leave a stale pointer.
      static auto deleted = +[](lv_event_t* e) {
        if (keyboardTarget == lv_event_get_target_obj(e)) { keyboardTarget = nullptr; setKeyboardVisible(false); }
      };
      bool watched = false;
      for (uint32_t i=0;i<lv_obj_get_event_count(target);++i)
        if (lv_event_dsc_get_cb(lv_obj_get_event_dsc(target,i))==deleted) watched=true;
      if (!watched) lv_obj_add_event_cb(target,deleted,LV_EVENT_DELETE,nullptr);
    }
  }
  Tab5Tools.resize(visible ? PAGE_H - keyboardHeight() : PAGE_H);
  Tab5GitView::resize(visible ? SCREEN_H - keyboardHeight() : SCREEN_H);
  Tab5Meshtastic::resize(visible ? SCREEN_H - keyboardHeight() : SCREEN_H);
  if (visible) {
    if(numberModeChanged)rebuildKeyboard();
    // The large TTF keyboard face is intentionally lazy to shorten boot.
    if (smoothFontReady && !keyTtfFont && ensureKeyFont()) rebuildKeyboard();
    lv_obj_remove_flag(keyboardPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keyboardPanel);
    if (editor && activePage == Page::Editor && keyboardTarget == editor) {
      lv_obj_set_height(editor, PAGE_H - keyboardHeight());
    }
  } else {
    if (accentPopup && lv_obj_is_valid(accentPopup)) lv_obj_delete(accentPopup);
    accentPopup = nullptr;
    lv_obj_add_flag(keyboardPanel, LV_OBJ_FLAG_HIDDEN);
    if (editor) lv_obj_set_height(editor, PAGE_H);
  }
}

static void setTopActionVisible(lv_obj_t* obj, bool visible) {
  if (!obj) return;
  if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void updateTopBarForPage() {
  shellTopLayout();
  const bool editorTools = activePage == Page::Editor;
  setTopActionVisible(undoBtn, editorTools);
  setTopActionVisible(redoBtn, editorTools);
  setTopActionVisible(editorCutBtn, editorTools);
  setTopActionVisible(editorCopyBtn, editorTools);
  setTopActionVisible(editorPasteBtn, editorTools);
  setTopActionVisible(keyboardBtn, editorTools);
  setTopActionVisible(saveBtn, editorTools);
  setTopActionVisible(topAudioIndicator,
                      !editorTools && Tab5Audio.mode() != Tab5AudioMode::Idle);
  if (batteryLabel)
    lv_obj_align(batteryLabel, LV_ALIGN_RIGHT_MID,
                 editorTools ? -140 : -4, 0);
  if (topWifiLabel)
    lv_obj_align(topWifiLabel, LV_ALIGN_RIGHT_MID, editorTools ? -240 : -104, 0);
  if (clockLabel)
    lv_obj_align(clockLabel, LV_ALIGN_RIGHT_MID, editorTools ? -274 : -138, 0);
  if (dateLabel) {
    setTopActionVisible(dateLabel, !editorTools);
    lv_obj_align(dateLabel, LV_ALIGN_RIGHT_MID, -214, 0);
  }

  switch (activePage) {
    case Page::Home:     lv_label_set_text(titleLabel, T(homePageTitle())); break;
    case Page::Files:    lv_label_set_text(titleLabel, T("Explorateur de fichiers")); break;
    case Page::Editor:   updateDocumentTitle(); break;
    case Page::Writing:  lv_label_set_text(titleLabel, T("Éditeur")); break;
    case Page::Preview:  lv_label_set_text(titleLabel, T("Aperçu du document")); break;
    case Page::Audio:    lv_label_set_text(titleLabel, T("Média")); break;
    case Page::Map:      lv_label_set_text(titleLabel, T("Cartes OpenStreetMap")); break;
    case Page::Settings: lv_label_set_text(titleLabel, T("Réglages")); break;
    case Page::ImageEditor: lv_label_set_text(titleLabel, T("Dessin")); break;
  }
  if (editorTools) updateEditorClipboardState();
  updateTopAudioIndicator(true);
  if (!editorTools) lv_label_set_text(dirtyLabel, "");
}

static void topAudioIndicatorEvent(lv_event_t* e) {
  (void)e;
  showPage(Page::Audio);
}

static void updateTopAudioIndicator(bool force) {
  if (!topAudioIndicator) return;
  static uint32_t seen=0;
  static bool forcePending=false;
  forcePending=forcePending||force;
  static Tab5AudioSnapshot audioState;
  if (!Tab5Audio.indicatorSnapshot(audioState,seen,forcePending)) return;
  forcePending=false;
  audioRememberPlayback(audioState);
  const Tab5AudioMode mode = audioState.mode;
  const bool visible = activePage != Page::Editor && mode != Tab5AudioMode::Idle;
  setTopActionVisible(topAudioIndicator, visible);
  if (!visible) return;

  String text;
  if (mode == Tab5AudioMode::Recording) text = "REC  ";
  else if (audioState.paused) text = String(LV_SYMBOL_PAUSE) + "  ";
  else if (mode == Tab5AudioMode::Radio) text = String(LV_SYMBOL_WIFI) + "  ";
  else text = String(LV_SYMBOL_AUDIO) + "  ";
  String oneLine=audioState.nowPlaying;
  oneLine.replace("\r", " "); oneLine.replace("\n", " ");
  text += oneLine;
  setLabelIfChanged(topAudioIndicator, text.c_str());
}

static void resumePythonApp() {
  if(!Tab5Python::running())return;
  if(activePage==Page::Writing)Tab5Writing.leaveEditing();
  setKeyboardVisible(false);keyboardDockFocus=false;keyboardUiNavigation=true;
  if(Tab5Tools.resumePythonApp()&&!Tab5PythonReader::visible())Tab5Focus.focusInitial(Tab5Tools.scope());
}

static void updatePythonResumeState() {
  if(!topPythonResume)return;
  static uint32_t lastUpdate=0;
  const uint32_t now=millis();
  if(now-lastUpdate<250)return;
  lastUpdate=now;
  const bool running=Tab5Python::running();setTopActionVisible(topPythonResume,running);
  if(!running)return;
  uint32_t generation;char title[192]{};
  if(!Tab5PythonUI::opened(generation,title)||!*title){
    const char* path=Tab5Python::runningPath();const char* file=strrchr(path,'/');
    snprintf(title,sizeof(title),"%s",file?file+1:path);
  }
  static String previous;
  String description=String(T("Reprendre"))+" : "+title+" · Ctrl+Alt+P";
  if(description!=previous){previous=description;Tab5Focus.describe(topPythonResume,description.c_str());}
}

static void updateNavState() {
  for (int i = 0; i < DOCK_BUTTON_COUNT; ++i) {
    if (!dockButtons[i]) continue;
    const uint64_t key=i<Tab5Shell::choices.count?Tab5Shell::choices.keys[i]:0;
    const auto* app=shellAppForKey(key);
    const bool active=dockLaunchedKey&&Tab5Tools.scope()?key==dockLaunchedKey:
      (key==Tab5Shell::homeKey()?activeTool==HomeAction::Home:
       app&&app->kind==Tab5AppCatalogue::Kind::Builtin&&app->action==uint8_t(activeTool));
    lv_obj_set_style_bg_color(dockButtons[i], active ? C_ACTIVE() : C_RAISED(), 0);
    lv_obj_set_style_border_color(dockButtons[i], active ? C_MUTED() : C_BORDER(), 0);
  }
}

static void restoreMapOpeningView() {
  // Defaults and explicit home are independent from the last visited location.
  mapHomeLatitude = Tab5MapViewState::DefaultLatitude;
  mapHomeLongitude = Tab5MapViewState::DefaultLongitude;
  mapHomeZoom = Tab5MapViewState::DefaultZoom;
  Tab5Map.loadHomeView(mapHomeLatitude, mapHomeLongitude, mapHomeZoom);
  mapLatitude = mapHomeLatitude;
  mapLongitude = mapHomeLongitude;
  mapZoom = mapHomeZoom;
  Tab5Map.loadView(mapLatitude, mapLongitude, mapZoom);
}

static void saveMapLeavingView() {
  // At most one small NVS commit when the view changed. No SD access or timer.
  if (!Tab5Map.saveView(mapLatitude, mapLongitude, mapZoom))
    toast("Dernière vue non enregistrée");
}

static void updateResourceForeground() {
  Tab5Resources::Owner foreground=Tab5Resources::Owner::System;
  if(Tab5Tools.scope())foreground=Tab5Resources::Owner::Tools;
  else if(videoViewerActive)foreground=Tab5Resources::Owner::Video;
  else if(readerActive)foreground=Tab5Resources::Owner::Reader;
  else switch(activePage){
    case Page::Files:foreground=Tab5Resources::Owner::Files;break;
    case Page::ImageEditor:foreground=Tab5Resources::Owner::Image;break;
    case Page::Map:foreground=Tab5Resources::Owner::Map;break;
    case Page::Audio:foreground=Tab5Resources::Owner::Audio;break;
    case Page::Editor:case Page::Writing:foreground=Tab5Resources::Owner::Writing;break;
    default:break;
  }
  Tab5MemoryManager.foreground(foreground);
}

static size_t releaseHiddenMediaList() {
  if(activePage==Page::Audio||!audioList)return 0;
  const auto before=tab5MemoryTotals();
  delete audioRows;audioRows=nullptr;
  std::vector<String>().swap(audioAlbumFolders);
  std::vector<String>().swap(audioRadioCategories);
  const auto after=tab5MemoryTotals();
  return (after.psram>before.psram?after.psram-before.psram:0)+
         (after.internal>before.internal?after.internal-before.internal:0);
}

static void showPage(Page page) {
  if(defaultFontPopup)closeDefaultFontPopup();
  if(Tab5Meshtastic::visible())Tab5Meshtastic::close();
  if(activePage==Page::Files&&page!=Page::Files)rememberFileSession();
  Tab5GitView::close();
  Tab5Tools.suspend();
  Tab5Focus.remember(keyboardDockFocus?dock:keyboardScope());
  Tab5Focus.clear();keyboardDockFocus=false;keyboardUiNavigation=false;
  if (page == Page::Map && (!sdReady || !Tab5Map.ready())) {
    Tab5DebugSerial.printf("[MAP][OPEN] refusée : microSD=%s, moteur=%s\n",
                  sdReady ? "OK" : "absente",
                  Tab5Map.ready() ? "OK" : "indisponible");
    toast(sdReady ? "Moteur cartographique indisponible"
                  : "Carte SD indisponible — Cartes non ouverte",
          3200);
    return;
  }
  if(page!=Page::Home){
    Tab5AppCatalogue::endScan();homeAppsScanning=false;
    if(homeCustomize){homeCustomize=false;Tab5AppCatalogue::loadChoices();}
    if(homeAppsGrid)lv_obj_clean(homeAppsGrid);
    homeAppCount=0;homeAppsDirty=true;
  }
  const Page previousPage = activePage;
  if (previousPage == Page::Audio && page != Page::Audio) {
    audioRememberScroll(); audioRememberBrowser();
    Tab5MediaSession::flush(millis());
  }
  if (previousPage == Page::Files && page != Page::Files) {
    fileTreeRevealPath="";fileListRevealPath="";
    fileTree.cancel();fileListDir.close();
    if(fileSearcher.busy()){fileSearchNeedsRestart=true;fileSearcher.cancel();}
    if(!folderPreview.done)cancelFolderPreview();
    freeFilePreviewImage();
  }
  if (previousPage == Page::Map && page != Page::Map) {
    saveMapLeavingView();
    mapOpenPending = false;
    ++mapGeneration;
    if (!mapGeneration) ++mapGeneration;
    Tab5Map.setActive(false, mapGeneration);
    mapWheel.cancel();
    mapSearchPending=mapSearchResultsReady=false;++mapSearchRequestId;if(!mapSearchRequestId)++mapSearchRequestId;
    freeMapCanvas();
  }
  if (previousPage == Page::Writing && page != Page::Writing) Tab5Writing.show(false);
  activePage = page;
  updateResourceForeground();
  if(page!=Page::ImageEditor)Tab5ImageEditor::show(false);
  if (Tab5Writing.object()) lv_obj_add_flag(Tab5Writing.object(), LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(homePage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(filesPage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(editorPage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(previewPage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(audioPage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(mapPage, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(settingsPage, LV_OBJ_FLAG_HIDDEN);
  Tab5Map.setActive(page == Page::Map, mapGeneration);
  if (previousPage == Page::Audio && page != Page::Audio) {
    // Views are hidden and their browsing state is remembered. Only derived
    // rows/catalogues are trimmed; active playback and pause remain owned by
    // the engine. Opening Media already rebuilds these lists from the source.
    releaseHiddenMediaList();
    Tab5Audio.releaseIdleCatalogues();
    Tab5Video.reclaimIdle();
  }

  lv_obj_t* activeObject = nullptr;
  switch (page) {
    case Page::Home:
      activeTool = HomeAction::Home;
      setKeyboardVisible(false);
      lv_obj_remove_flag(homePage, LV_OBJ_FLAG_HIDDEN);
      activeObject = homePage;
      break;
    case Page::Files:
      if (activeTool != HomeAction::Reader) activeTool = HomeAction::Files;
      setKeyboardVisible(false);
      lv_obj_remove_flag(filesPage, LV_OBJ_FLAG_HIDDEN);
      ensureFileSession();
      activeObject = filesPage;
      break;
    case Page::Editor:
      activeTool = HomeAction::Editor;
      lv_label_set_text(previewLabel, "");
      lv_obj_remove_flag(editorPage, LV_OBJ_FLAG_HIDDEN);
      activeObject = editorPage;
      break;
    case Page::Writing:
      activeTool = HomeAction::Editor;
      setKeyboardVisible(false);
      Tab5Writing.show(true);
      activeObject = Tab5Writing.object();
      break;
    case Page::Preview: {
      activeTool = HomeAction::Reader;
      setKeyboardVisible(false);
      const char* text = lv_textarea_get_text(editor);
      lv_label_set_text_static(previewLabel, text);
      updateEditorStatus(true);
      lv_obj_remove_flag(previewPage, LV_OBJ_FLAG_HIDDEN);
      activeObject = previewPage;
      break;
    }
    case Page::Audio:
      activeTool = HomeAction::Audio;
      setKeyboardVisible(false);
      refreshAudioList();
      updateAudioUi(true);
      lv_obj_remove_flag(audioPage, LV_OBJ_FLAG_HIDDEN);
      activeObject = audioPage;
      break;
    case Page::Map:
      if (previousPage != Page::Map) restoreMapOpeningView();
      activeTool = HomeAction::Map;
      setKeyboardVisible(false);
      Tab5Map.setActive(true, mapGeneration);
      lv_obj_remove_flag(mapPage, LV_OBJ_FLAG_HIDDEN);
      if (mapStatusLabel) setLabelIfChanged(mapStatusLabel, T("Ouverture…"));
      mapOpenPending = true;
      Tab5DebugSerial.println("[MAP][OPEN] page visible, initialisation différée");
      activeObject = mapPage;
      break;
    case Page::ImageEditor:
      activeTool = HomeAction::ImageEditor;
      setKeyboardVisible(false);
      Tab5ImageEditor::show(true);
      activeObject = Tab5ImageEditor::scope();
      break;
    case Page::Settings:
      // SD catalogues are discovered only when their selectors are opened.
      activeTool = HomeAction::Settings;
      setKeyboardVisible(false);
      refreshSystemInfo();
      lv_obj_remove_flag(settingsPage, LV_OBJ_FLAG_HIDDEN);
      activeObject = settingsPage;
      break;
  }
  if (activeObject) lv_obj_move_foreground(activeObject);
  if (pageHost) lv_obj_invalidate(pageHost);
  updateTopBarForPage();
  updateNavState();
}

static bool imageMouseEvent(int x,int y,uint8_t buttons,int wheel) {
  if(activePage==Page::Map&&wheel&&buttons==0&&mapImage&&mapCanvas&&keyboardScope()==mapPage){
    lv_area_t area;lv_obj_get_coords(mapImage,&area);
    return mapWheel.push(wheel,x-area.x1,y-area.y1,lv_obj_get_width(mapImage),lv_obj_get_height(mapImage),
        {mapLatitude,mapLongitude,mapZoom},millis());
  }
  if(activePage!=Page::ImageEditor||!Tab5ImageEditor::visible())return false;
  auto* currentScope=keyboardScope();
  auto* imageScope=Tab5ImageEditor::scope();
  auto* imageModal=Tab5ImageEditor::modalScope();
  if(!currentScope||(currentScope!=imageScope&&currentScope!=imageModal)){
    Tab5ImageEditor::cancelPointer();
    return false;
  }
  return Tab5ImageEditor::mouse(x,y,buttons,wheel);
}

static void openImageEditor(const char* path) {
  const String requested=path?path:"";
  setKeyboardVisible(false);
  showPage(Page::ImageEditor);
  Tab5ImageEditor::resize(WORK_W,PAGE_H);
  if(!Tab5ImageEditor::open(requested.length()?requested.c_str():nullptr)&&!Tab5ImageEditor::scope()){
    showPage(Page::Files);toast("Mémoire insuffisante",3200);return;
  }
  if(auto* object=Tab5ImageEditor::scope())lv_obj_move_foreground(object);
  updateResourceForeground();
}

// -----------------------------------------------------------------------------
// File browser: selection on the left, live preview on the right
// -----------------------------------------------------------------------------

static void fileEntryDeleteEvent(lv_event_t* e) {
  auto* ctx = static_cast<FileEntryCtx*>(lv_event_get_user_data(e));
  auto* object=lv_event_get_target_obj(e);
  if(selectedFileBtn==object)selectedFileBtn=nullptr;
  fileSelectionRows.erase(std::remove_if(fileSelectionRows.begin(),fileSelectionRows.end(),
      [object](const FileSelectionRow& row){return row.object==object;}),fileSelectionRows.end());
  if (ctx) heap_caps_free(ctx);
}

static void setButtonEnabled(lv_obj_t* button, bool enabled) {
  if (!button) return;
  if (enabled) lv_obj_remove_state(button, LV_STATE_DISABLED);
  else lv_obj_add_state(button, LV_STATE_DISABLED);
}

static void setIconButtonSymbol(lv_obj_t* button, const char* symbol) {
  if (!button) return;
  lv_obj_t* label = lv_obj_get_child(button, 0);
  if (label) setLabelIfChanged(label, symbol);
}

static size_t utf8ByteOffset(const char* text, uint32_t charIndex) {
  if (!text) return 0;
  size_t offset = 0;
  uint32_t current = 0;
  while (text[offset] && current < charIndex) {
    ++offset;
    while (text[offset] && (static_cast<uint8_t>(text[offset]) & 0xC0) == 0x80) ++offset;
    ++current;
  }
  return offset;
}

static uint32_t utf8CharCount(const char* text) {
  uint32_t count = 0;
  if (!text) return count;
  for (size_t i = 0; text[i]; ++i) {
    if ((static_cast<uint8_t>(text[i]) & 0xC0) != 0x80) ++count;
  }
  return count;
}

static bool getEditorSelection(EditorSelection& selection) {
  if (!editor || !lv_textarea_text_is_selected(editor)) return false;
  lv_obj_t* label = lv_textarea_get_label(editor);
  if (!label) return false;
  uint32_t start = lv_label_get_text_selection_start(label);
  uint32_t end = lv_label_get_text_selection_end(label);
  if (start == end) return false;
  if (end < start) {
    const uint32_t swap = start;
    start = end;
    end = swap;
  }
  const char* text = lv_textarea_get_text(editor);
  selection.startChar = start;
  selection.endChar = end;
  selection.startByte = utf8ByteOffset(text, start);
  selection.endByte = utf8ByteOffset(text, end);
  return selection.endByte > selection.startByte;
}

static bool copyEditorSelectionToClipboard() {
  EditorSelection selection;
  if (!getEditorSelection(selection)) return false;
  const char* text = lv_textarea_get_text(editor);
  const size_t length = selection.endByte - selection.startByte;
  char* copy = static_cast<char*>(
      heap_caps_malloc(length + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!copy) copy = static_cast<char*>(malloc(length + 1));
  if (!copy) return false;
  memcpy(copy, text + selection.startByte, length);
  copy[length] = 0;
  editorClipboard = copy;
  free(copy);
  return true;
}

static bool replaceEditorSelection(const char* insertion) {
  if (!editor) return false;
  EditorSelection selection;
  if (!getEditorSelection(selection)) {
    if (insertion && *insertion) lv_textarea_add_text(editor, insertion);
    return insertion && *insertion;
  }

  const char* oldText = lv_textarea_get_text(editor);
  const size_t oldLength = strlen(oldText);
  const size_t insertionLength = insertion ? strlen(insertion) : 0;
  const size_t removedLength = selection.endByte - selection.startByte;
  const size_t newLength = oldLength - removedLength + insertionLength;
  char* replacement = static_cast<char*>(
      heap_caps_malloc(newLength + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!replacement) replacement = static_cast<char*>(malloc(newLength + 1));
  if (!replacement) return false;

  memcpy(replacement, oldText, selection.startByte);
  if (insertionLength) memcpy(replacement + selection.startByte, insertion, insertionLength);
  memcpy(replacement + selection.startByte + insertionLength,
         oldText + selection.endByte, oldLength - selection.endByte);
  replacement[newLength] = 0;
  lv_textarea_set_text(editor, replacement);
  lv_textarea_set_cursor_pos(editor,
      static_cast<int32_t>(selection.startChar + utf8CharCount(insertion)));
  lv_textarea_clear_selection(editor);
  free(replacement);
  return true;
}

static void updateEditorClipboardState() {
  const bool selected = editor && lv_textarea_text_is_selected(editor);
  setButtonEnabled(editorCutBtn, selected);
  setButtonEnabled(editorCopyBtn, selected);
  setButtonEnabled(editorPasteBtn, editorClipboard.length() > 0);
}

static bool isProtectedSystemPath(const String& path) {
  return Tab5Files::protectedPath(path.c_str());
}
static void warnSystemFile(const String&) {
  // System files are identified by their row colour, without interrupting use.
}

static String fileContextDirectory() {
  if(selectedPath.length()&&fileSelection.paths.size()<=1)
    return selectedIsDir?selectedPath:parentDirOf(selectedPath);
  return fileContentDir.length()?fileContentDir:currentDir;
}
static String fileGitDirectory() {
  if(selectedPath.length()&&fileSelection.paths.size()<=1)
    return selectedIsDir?selectedPath:parentDirOf(selectedPath);
  return fileContextDirectory();
}
static bool selectedFolderIsGit() {
  // Git applies to the current folder, selected folder, or selected file's
  // parent. Ascend within its own volume so nested folders stay contextual.
  const String directory=fileGitDirectory();
  static String checkedPath;static uint32_t checkedRevision=0;static bool repository=false;
  const uint32_t revision=tab5FsRevision();
  if(checkedPath!=directory||checkedRevision!=revision){
    checkedPath=directory;checkedRevision=revision;repository=false;
    String probe=directory;
    while(probe.length()){
      const String metadata=joinPath(probe,".git");File entry=tab5FS.open(metadata.c_str(),FILE_READ);
      if(entry){repository=entry.isDirectory()?tab5FS.exists(joinPath(metadata,"HEAD").c_str())&&tab5FS.exists(joinPath(metadata,"config").c_str()):entry.size()>0;entry.close();}
      if(repository||probe=="/"||probe=="/USB")break;
      const String parent=parentDirOf(probe);if(parent==probe)break;probe=parent;
    }
  }
  return repository;
}
static bool fileSelectionHasVolume(){
  return selectedPath=="/"||selectedPath=="/USB"||std::any_of(fileSelection.paths.begin(),fileSelection.paths.end(),[](const std::string& path){return path=="/"||path=="/USB";});
}
static bool fileShowGitAction(){
  return selectedPath.length()&&selectedIsDir&&fileSelection.paths.size()<=1&&
      (!fileMultiSelect||!fileSelection.paths.empty())&&selectedFolderIsGit();
}
static void updateFileActionState() {
  const bool anySelected=!fileSelection.paths.empty()||(!fileMultiSelect&&selectedPath.length()>0);
  const bool selected = anySelected&&selectedPath.length()>0&&fileSelection.paths.size()<=1;
  const bool fileBusy=Tab5GitView::busy()||Tab5Git::busy()||Tab5FileOperation::busy()||filePasteBatch.active||bool(fileSelectAllDirectory)||fileSelectAllSearch;
  const bool canEdit = selected && !selectedIsDir && selectedIsText;
  const bool canViewImage = selected && !selectedIsDir && (selectedIsImage||isImageProject(selectedPath));
  const bool canPlayAudio = selected && !selectedIsDir && selectedIsAudio;
  const bool canPlayVideo = selected && !selectedIsDir && selectedIsVideo;
  const bool canRead = selected && !selectedIsDir &&
                       (selectedIsText || isEpubExtension(selectedPath));
  const bool canExecute = selected && !selectedIsDir && (isPythonExtension(selectedPath)||isShellExtension(selectedPath)||isFirmwareExtension(selectedPath)||isExecutableExtension(selectedPath));

  setTopActionVisible(fileOpenBtn,
                      selected && !selectedIsDir && (canEdit || canViewImage ||
                                   canPlayAudio || canPlayVideo));
  setTopActionVisible(fileReadBtn, canRead);
  setTopActionVisible(fileDrawBtn, canViewImage);
  setTopActionVisible(fileExecuteBtn, canExecute);
  setTopActionVisible(fileGitBtn,fileShowGitAction());
  setButtonEnabled(fileGitBtn,!fileBusy&&pathStorageReady(fileGitDirectory())&&fileSelection.paths.size()<=1);
  if(Tab5GitView::scope())Tab5GitView::context(fileGitDirectory().c_str(),selected&&!selectedIsDir?selectedPath.c_str():nullptr);
  setTopActionVisible(fileActionsMenuBtn,true);
  if(fileExecuteBtn){setLabelIfChanged(lv_obj_get_child(fileExecuteBtn,0),isFirmwareExtension(selectedPath)?LV_SYMBOL_UPLOAD:LV_SYMBOL_PLAY);Tab5Focus.describe(fileExecuteBtn,T(tab5DosProgramFile(selectedPath.c_str())?"Ouvrir avec DOS":isShellExtension(selectedPath)?"Exécuter le script":isFirmwareExtension(selectedPath)?"Installer un firmware":isExecutableExtension(selectedPath)?"Ouvrir l'application":"Exécuter Python"));}
  // Clipboard actions keep fixed positions even when no row is selected.
  setTopActionVisible(fileOpenWithBtn, selected && !selectedIsDir);
  setTopActionVisible(fileRenameBtn, selected);
  setTopActionVisible(deleteDocBtn, anySelected);
  setTopActionVisible(fileInfoBtn, selected);
  setTopActionVisible(filePrintBtn, selected && !selectedIsDir);
  setTopActionVisible(fileVaultBtn, selected && !selectedIsDir);
  setIconButtonSymbol(fileOpenBtn, selectedIsDir ? LV_SYMBOL_DIRECTORY :
                      (canViewImage ? LV_SYMBOL_EYE_OPEN :
                      (canPlayVideo ? LV_SYMBOL_IMAGE :
                      (canPlayAudio ? LV_SYMBOL_AUDIO : LV_SYMBOL_EDIT))));
  setButtonEnabled(fileOpenBtn,
                   selected && (!canViewImage||!fileBusy) && (selectedIsDir || canEdit || canViewImage ||
                                canPlayAudio || canPlayVideo));
  setButtonEnabled(fileReadBtn, canRead);
  setButtonEnabled(fileDrawBtn, canViewImage&&!fileBusy);
  Tab5Focus.describe(fileOpenBtn,T(canViewImage?"Ouvrir avec le visualisateur":"Ouvrir"));
  setButtonEnabled(fileExecuteBtn, canExecute);
  setButtonEnabled(fileCopyBtn, anySelected&&!fileBusy&&!fileSelectionHasVolume());
  setButtonEnabled(fileCutBtn, anySelected&&!fileBusy&&!fileSelectionHasVolume());
  setButtonEnabled(fileRenameBtn, selected&&!fileBusy&&!fileSelectionHasVolume());
  setButtonEnabled(deleteDocBtn, anySelected&&!fileBusy&&!fileSelectionHasVolume());
  setButtonEnabled(filePasteBtn, (!fileClipboardPaths.empty()||clipboardPath.length()>0)&&pathStorageReady(fileContextDirectory())&&!fileBusy);
  if(filePasteBtn){String tip=String(T("Coller dans"))+" : "+fileContextDirectory();Tab5Focus.describe(filePasteBtn,tip.c_str());}
  if(fileHiddenBtn){if(Tab5Files::showHidden)lv_obj_add_state(fileHiddenBtn,LV_STATE_CHECKED);else lv_obj_remove_state(fileHiddenBtn,LV_STATE_CHECKED);}
  setTopActionVisible(fileUsbButton,true);
  setButtonEnabled(fileParentButton,fileContentMode==FileContentMode::Search||fileContentMode==FileContentMode::Preview||(fileContentMode==FileContentMode::Directory&&fileDisplayedPath!="/"&&fileDisplayedPath!="/USB"));
  setTopActionVisible(fileHideSelectedBtn,anySelected&&selectedPath!="/"&&selectedPath!="/USB");
  setButtonEnabled(fileHideSelectedBtn,anySelected&&!fileBusy);
  if(fileHideSelectedBtn&&anySelected){
    bool allHidden=!fileSelection.paths.empty();
    for(const auto& path:fileSelection.paths)if(!Tab5HiddenFiles::hidden(path.c_str())){allHidden=false;break;}
    if(fileSelection.paths.empty())allHidden=Tab5HiddenFiles::hidden(selectedPath.c_str());
    lv_label_set_text(lv_obj_get_child(fileHideSelectedBtn,0),T(allHidden?"Ne plus masquer":"Masquer"));
    Tab5Focus.describe(fileHideSelectedBtn,T("Masquer ou afficher la sélection · Ctrl+Maj+H"));
  }
}

static String uniqueDestination(const String& directory, const String& source,
                                bool sourceIsDir) {
  const String original = baseNameOf(source);
  String direct = joinPath(directory, original);
  if (direct != source && !tab5FS.exists(direct.c_str())) return direct;

  int dot = sourceIsDir ? original.length() : original.lastIndexOf('.');
  if (dot <= 0) dot = original.length();
  const String stem = original.substring(0, dot);
  const String ext = original.substring(dot);
  for (int i = 1; i <= 99; ++i) {
    String suffix = " - copy";
    if (i > 1) {
      suffix += " ";
      suffix += String(i);
    }
    String candidate = joinPath(directory, stem + suffix + ext);
    if (!tab5FS.exists(candidate.c_str())) return candidate;
  }
  return "";
}

static bool deletePathRecursive(const String& path) {
  if (!Tab5Files::userWritable(path.c_str())) return false;
  File target = tab5FS.open(path.c_str(), FILE_READ);
  if (!target) return false;
  if (!target.isDirectory()) {
    target.close();
    return tab5FS.remove(path.c_str());
  }

  std::vector<String> children;
  children.reserve(24);
  File entry = target.openNextFile();
  while (entry) {
    const String name = baseNameOf(String(entry.name()));
    if (name.length()) children.push_back(name);
    entry.close();
    entry = target.openNextFile();
  }
  target.close();
  for (const String& name : children) {
    if (!deletePathRecursive(joinPath(path, name))) return false;
  }
  return tab5FS.rmdir(path.c_str());
}

#include "Tab5ExplorerRowStyle.inc"

static void clearFilePreview() {
  fileTreeRevealPath="";fileListRevealPath="";
  fileSelection.clear();fileSelectAllDirectory.close();fileSelectAllSearch=false;
  fileMetadataPath="";
  selectedPath="";selectedIsDir=selectedIsText=selectedIsImage=selectedIsAudio=selectedIsVideo=false;
  fileInfoText="";selectedFileBtn=nullptr;
  updateFileSelectionUi();
}

// Command metadata follows selection; the visible content changes only for an
// explicit browsing gesture. Context menus and range selection do not navigate.
static void previewSelectedFile() {
  if(fileMetadataPath==selectedPath&&fileMetadataRevision==tab5FsRevision())return;
  fileMetadataPath=selectedPath;fileMetadataRevision=tab5FsRevision();
  selectedIsText=selectedIsImage=selectedIsAudio=selectedIsVideo=false;
  if(!selectedPath.length()){fileInfoText="";return;}
  File f=tab5FS.open(selectedPath.c_str(),FILE_READ);
  selectedIsDir=f&&f.isDirectory();fileSelectedBytes=f?f.size():0;
  const time_t modified=f?f.getLastWrite():0;
  if(f&&!selectedIsDir){
    selectedIsVideo=isVideoExtension(selectedPath);
    selectedIsAudio=!selectedIsVideo&&isAudioExtension(selectedPath);
    selectedIsImage=!selectedIsVideo&&!selectedIsAudio&&!isEpubExtension(selectedPath)&&
        (isImageProject(selectedPath)||isReadableImageFile(selectedPath));
    selectedIsText=!selectedIsVideo&&!selectedIsAudio&&!selectedIsImage&&!isEpubExtension(selectedPath)&&
        (isEditableTextExtension(selectedPath)||looksLikeTextFile(f));
  }
  const String type=!f?String(T("Élément inaccessible")):selectedIsDir?String(T("Dossier")):selectedIsImage?String(T("Image")):selectedIsText?String(T("Texte")):String(T("Fichier"));
  f.close();
  fileInfoText=String(T("Nom"))+" : "+baseNameOf(selectedPath)+"\n"+T("Type")+" : "+type+"\n"+
      T("Taille exacte")+" : "+String(static_cast<unsigned long>(fileSelectedBytes))+" "+T("octets")+"\n"+
      T("Modifié")+" : "+formatFileTime(modified)+"\n"+T("Chemin")+" : "+selectedPath;
}
static void clearFileContent(){
  fileSearcher.cancel();fileSearchQuery="";fileSearchNeedsRestart=false;fileDirectoryModel.clear();fileSavedContentOffset=0;
  fileContentMode=FileContentMode::Empty;
  fileDisplayedPath="";fileContentDir="";cancelFolderPreview();freeFilePreviewImage();
  if(filePreviewTitle)setLabelIfChanged(filePreviewTitle,"");
  if(filePreviewMeta)setLabelIfChanged(filePreviewMeta,"");
  if(filePreviewLabel)setLabelIfChanged(filePreviewLabel,"");
  if(filePreviewScroll)lv_obj_add_flag(filePreviewScroll,LV_OBJ_FLAG_HIDDEN);
}
static void showFileSelectionContent(bool force){
  if(!selectedPath.length()||Tab5GitView::scope())return;
  const String path=selectedPath;
  if(selectedIsDir){
    if(force&&fileContentMode==FileContentMode::Directory&&fileDisplayedPath==path)fileDisplayedRevision=UINT32_MAX;
    openFileContentDirectory(path);return;
  }
  if(!force&&fileContentMode==FileContentMode::Preview&&fileDisplayedPath==path&&fileDisplayedRevision==tab5FsRevision())return;
  cancelFolderPreview();freeFilePreviewImage();
  fileContentMode=FileContentMode::Preview;fileDisplayedPath=path;fileContentDir=parentDirOf(path);fileDisplayedRevision=tab5FsRevision();
  updateFileActionState();
  setLabelIfChanged(filePreviewTitle,path.c_str());
  String meta=String(static_cast<unsigned long>(fileSelectedBytes))+" "+T("octets");
  setLabelIfChanged(filePreviewMeta,meta.c_str());
  lv_obj_remove_flag(filePreviewScroll,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(filePreviewLabel,LV_OBJ_FLAG_HIDDEN);
  lv_obj_scroll_to_y(filePreviewScroll,0,LV_ANIM_OFF);
  if(selectedIsImage){setLabelIfChanged(filePreviewLabel,"");showFilePreviewImage();return;}
  if(selectedIsText){
    File f=tab5FS.open(path.c_str(),FILE_READ);
    if(!f){setLabelIfChanged(filePreviewLabel,T("Élément inaccessible"));return;}
    const size_t size=f.size(),want=std::min(size,size_t(FILE_PREVIEW_BYTES));
    char* buf=static_cast<char*>(heap_caps_malloc(want+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!buf){f.close();setLabelIfChanged(filePreviewLabel,T("Mémoire insuffisante pour l’aperçu."));return;}
    const size_t got=f.read(reinterpret_cast<uint8_t*>(buf),want);f.close();size_t valid=got;
    if(got<size&&valid){size_t start=valid-1;while(start&&(uint8_t(buf[start])&0xc0)==0x80)--start;
      const uint8_t lead=uint8_t(buf[start]);const size_t need=lead>=0xf0&&lead<=0xf4?4:lead>=0xe0&&lead<=0xef?3:lead>=0xc2&&lead<=0xdf?2:1;
      if(need>valid-start)valid=start;}
    buf[valid]=0;setLabelIfChanged(filePreviewLabel,buf);heap_caps_free(buf);
  }else setLabelIfChanged(filePreviewLabel,fileInfoText.c_str());
}
static void restoreFileContent(){
  if(fileContentMode==FileContentMode::Empty){clearFileContent();return;}
  if(fileContentMode==FileContentMode::Search){
    const String directory=fileSearchRoot,query=fileSearchQuery,selection=selectedPath;
    const auto selected=fileSelection;const bool right=fileActiveRight,multiple=fileMultiSelect;
    const size_t offset=fileSearchOffset;
    beginFileSearch(directory,query);fileSearchOffset=offset;
    fileSelection=selected;selectedPath=selection;fileActiveRight=right;fileMultiSelect=multiple;
    fileMetadataPath="";previewSelectedFile();updateFileSelectionUi();return;
  }
  if(fileContentMode==FileContentMode::Directory){
    const String path=fileDisplayedPath;
    const size_t offset=folderPreview.path==path?folderPreview.offset:fileSavedContentOffset;
    const int32_t scroll=folderPreview.path==path&&folderPreviewRows?lv_obj_get_scroll_y(folderPreviewRows):fileSavedContentScroll;
    fileDisplayedRevision=UINT32_MAX;openFileContentDirectory(path);
    if(offset){folderPreview.begin(path,offset);lv_obj_clean(folderPreviewRows);serviceFolderPreview();}
    fileSavedContentOffset=offset;fileSavedContentScroll=scroll;fileSessionRestorePending=true;
    return;
  }
  const String selection=selectedPath;const bool directory=selectedIsDir;
  const String visible=fileDisplayedPath;
  selectedPath=visible;fileMetadataPath="";previewSelectedFile();showFileSelectionContent(true);
  selectedPath=selection;selectedIsDir=directory;fileMetadataPath="";previewSelectedFile();updateFileActionState();
}
static lv_color_t fileRowColor(const String& path){
  return Tab5HiddenFiles::hidden(path.c_str())?C_MUTED():(isProtectedSystemPath(path)?lv_color_mix(lv_color_hex(0xE3A642),C_TEXT(),150):C_TEXT());
}
// Restyle the existing model rows, never rebuild the browser or change its
// selection/scroll/focus. Hidden and protected paths have semantic/derived colours
// that cannot be recovered reliably by matching old RGB values.
static lv_color_t fileIconColor(const String& path,bool directory){
  const auto& palette=currentTheme();const bool hidden=Tab5HiddenFiles::hidden(path.c_str());
  const uint32_t color=hidden?(directory?palette.hiddenFolderIcon:palette.hiddenFileIcon):
      (directory?palette.folderIcon:palette.fileIcon);
  return color<=0xffffff?lv_color_hex(color):fileRowColor(path);
}
static void fileRefreshAppearance(){
  filePrepareRowStyles();
  for(auto& row:fileSelectionRows){
    fileStyleRow(row.object,row.checked,fileRowColor(row.path.c_str()));
    if(auto* icon=lv_obj_get_child(row.object,0))lv_obj_set_style_text_color(icon,fileIconColor(row.path.c_str(),row.directory),0);
    if(row.directory&&lv_obj_get_child_count(row.object)>2){
      auto* toggle=lv_obj_get_child(row.object,2);
      lv_obj_set_style_bg_color(toggle,C_ACTIVE(),LV_STATE_PRESSED);
      if(auto* glyph=lv_obj_get_child(toggle,0))lv_obj_set_style_text_color(glyph,C_MUTED(),0);
    }
  }
  for(auto* label:{filePathLabel,filePreviewMeta,folderPreviewPageLabel})
    if(label)lv_obj_set_style_text_color(label,C_MUTED(),0);
  for(auto* label:{filePreviewTitle,filePreviewLabel})
    if(label)lv_obj_set_style_text_color(label,C_TEXT(),0);
}
static std::vector<std::string> fileVisiblePaths(){
  std::vector<std::string> paths;paths.reserve(fileSelectionRows.size());
  for(const auto& row:fileSelectionRows)if(row.right==fileActiveRight)paths.push_back(row.path);
  return paths;
}
static void updateFileSelectionUi(){
  if(!fileSelection.paths.empty()&&!fileSelection.contains(selectedPath.c_str())){
    selectedPath=fileSelection.paths.back().c_str();selectedFileBtn=nullptr;
    fileMetadataPath="";previewSelectedFile();
  }
  for(auto& row:fileSelectionRows){
    const bool checked=row.right==fileActiveRight&&fileSelection.contains(row.path),tick=checked&&fileMultiSelect;
    if(row.checked!=checked){fileStyleRow(row.object,checked,fileRowColor(row.path.c_str()));row.checked=checked;}
    if(row.tick!=tick){auto* icon=lv_obj_get_child(row.object,0);
      if(icon&&lv_obj_check_type(icon,&lv_label_class))setLabelIfChanged(icon,tick?LV_SYMBOL_OK:row.directory?LV_SYMBOL_DIRECTORY:LV_SYMBOL_FILE);
      row.tick=tick;}
  }
  if(fileMultiSelectBtn){if(fileMultiSelect)lv_obj_add_state(fileMultiSelectBtn,LV_STATE_CHECKED);else lv_obj_remove_state(fileMultiSelectBtn,LV_STATE_CHECKED);}
  if(fileSelection.paths.empty()){selectedPath="";selectedFileBtn=nullptr;selectedIsDir=selectedIsText=selectedIsImage=selectedIsAudio=selectedIsVideo=false;}
  updateFileActionState();
}
static void fileSelectEntry(const String& path,bool directory,lv_obj_t* object,bool right,bool toggle=false,bool range=false,bool additive=false){
  fileSessionRestorePending=false;fileSavedFocusPath="";
  fileTreeRevealPath="";fileListRevealPath="";
  if(fileActiveRight!=right){fileSelection.clear();fileMultiSelect=false;fileSelectAllDirectory.close();fileSelectAllSearch=false;}
  fileActiveRight=right;
  fileSelection.choose(path.c_str(),range?fileVisiblePaths():std::vector<std::string>{},toggle,range,additive);
  // Ctrl/Shift ranges do not turn on the sticky touch selection mode.
  selectedPath=path;selectedIsDir=directory;selectedFileBtn=object;
  if(!fileSelection.contains(path.c_str())&&!fileSelection.paths.empty()){
    selectedPath=fileSelection.paths.back().c_str();selectedFileBtn=nullptr;
  }
  if(fileSelection.paths.empty())selectedPath="";
  previewSelectedFile();updateFileSelectionUi();
}
static void fileEntryActivateEvent(lv_event_t* e);
static void fileEntryPressEvent(lv_event_t*){fileSelectionLongPress=false;}
static void fileEntryLongPressEvent(lv_event_t* e){
  auto* ctx=static_cast<FileEntryCtx*>(lv_event_get_user_data(e));if(!ctx)return;
  fileSelectionLongPress=true;
  fileSelectEntry(ctx->path,ctx->isDir,lv_event_get_current_target_obj(e),ctx->right,true);fileMultiSelect=true;updateFileSelectionUi();
}
static void fileEntryClickEvent(lv_event_t* e){
  auto* ctx=static_cast<FileEntryCtx*>(lv_event_get_user_data(e));if(!ctx)return;
  if(fileSelectionLongPress){fileSelectionLongPress=false;return;}
  const uint8_t modifiers=writing::usbModifiers();
  const bool control=modifiers&0x99,shift=modifiers&0x22,browse=!control&&!shift&&!fileMultiSelect;
  const String path=ctx->path;const bool directory=ctx->isDir,right=ctx->right;
  auto* object=lv_event_get_current_target_obj(e);
  fileSelectEntry(path,directory,object,right,control||(fileMultiSelect&&!shift),shift,control);
  Tab5Focus.focusObject(object);
  if(browse&&!right)showFileSelectionContent();
}
static void fileEntryDoubleClickEvent(lv_event_t* e){
  if(fileMultiSelect||(writing::usbModifiers()&0xbb))return;
  fileEntryActivateEvent(e);
}


static void openSelectedShellScript() {
  if(Tab5Terminal::busy()){toast("Le terminal est occupé.");return;}
  const String path=selectedPath;
  setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Terminal);
  if(!Tab5Terminal::runScript(path.c_str()))toast("Impossible de lancer le script.");
}

static void fileEntryActivateEvent(lv_event_t* e) {
  auto* ctx=static_cast<FileEntryCtx*>(lv_event_get_user_data(e));if(!ctx)return;
  const String path=ctx->path;const bool directory=ctx->isDir,right=ctx->right;
  fileSelectEntry(path,directory,lv_event_get_current_target_obj(e),right);
  selectedOpenEvent(nullptr);
  if(directory&&right)fileRequestTreeReveal(path);
}
static void fileTreeChevronEvent(lv_event_t* e){
  auto* ctx=static_cast<FileEntryCtx*>(lv_event_get_user_data(e));if(!ctx||!ctx->isDir)return;
  // Expansion never changes selection, active pane, or the content directory.
  fileSessionRestorePending=false;fileSavedFocusPath="";fileTreeRevealPath="";
  fileTree.toggle(ctx->path);
}
static bool fileBindRow(lv_obj_t* object,const String& path,bool directory,bool right,uint16_t depth,lv_obj_t* chevron=nullptr){
  if(!object||path.length()>=FILE_PATH_CAPACITY)return false;
  auto* ctx=static_cast<FileEntryCtx*>(heap_caps_malloc(sizeof(FileEntryCtx),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if(!ctx){lv_obj_delete(object);return false;}
  path.toCharArray(ctx->path,sizeof(ctx->path));ctx->isDir=directory;ctx->right=right;ctx->depth=depth;
  fileSelectionRows.push_back({path.c_str(),directory,object,right,depth});
  if(auto* icon=lv_obj_get_child(object,0))lv_obj_set_style_text_color(icon,fileIconColor(path,directory),0);
  lv_obj_remove_flag(object,LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_add_event_cb(object,fileEntryPressEvent,LV_EVENT_PRESSED,ctx);
  lv_obj_add_event_cb(object,fileEntryLongPressEvent,LV_EVENT_LONG_PRESSED,ctx);
  lv_obj_add_event_cb(object,fileEntryClickEvent,LV_EVENT_CLICKED,ctx);
  lv_obj_add_event_cb(object,fileEntryDoubleClickEvent,LV_EVENT_DOUBLE_CLICKED,ctx);
  Tab5Focus.activation(object,fileEntryActivateEvent,ctx);
  lv_obj_add_event_cb(object,fileEntryDeleteEvent,LV_EVENT_DELETE,ctx);
  if(chevron){lv_obj_remove_flag(chevron,LV_OBJ_FLAG_EVENT_BUBBLE);lv_obj_remove_flag(chevron,LV_OBJ_FLAG_CLICK_FOCUSABLE);lv_obj_add_event_cb(chevron,fileTreeChevronEvent,LV_EVENT_CLICKED,ctx);Tab5Focus.activation(chevron,fileTreeChevronEvent,ctx);}
  if(right==fileActiveRight&&path==selectedPath)selectedFileBtn=object;
  return true;
}
static bool addFileRow(const String& full,const String& name,bool isDir,bool searchResult=false,uint16_t depth=0){
  if(!isDir)return true;
  const String label=full=="/"?String("SD"):full=="/USB"?String("USB"):name;
  lv_obj_t* chevron=nullptr;
  auto* object=fileCreateTreeRow(fileList,label.c_str(),depth,true,fileTree.expanded(full),fileRowColor(full),&chevron);
  if(!fileBindRow(object,full,true,false,depth,chevron))return false;
  fileTree.requestChildProbe(full);
  if(chevron&&!fileTree.hasSubdirectories(full))lv_obj_add_flag(chevron,LV_OBJ_FLAG_HIDDEN);
  return true;
}
static void updateFileSearchCaption(){
  if(fileContentMode!=FileContentMode::Search)return;
  String label=fileSearchQuery;
  if(fileSearcher.busy())label+=" …";
  else if(fileSearcher.incomplete())label+=" · "+String(T("Recherche partielle : "))+String(fileSearcher.count());
  else label+=" · "+String(fileSearcher.count());
  setLabelIfChanged(filePreviewMeta,label.c_str());
}
static void renderFileSearchPage(size_t offset){
  if(fileContentMode!=FileContentMode::Search||!folderPreviewRows)return;
  const size_t count=fileDirectoryModel.size();
  if(offset>=count&&offset)offset=count?((count-1)/Tab5FolderPreview::PageSize)*Tab5FolderPreview::PageSize:0;
  fileSearchOffset=offset;
  if(Tab5Focus.contains(folderPreviewRows,Tab5Focus.focused()))Tab5Focus.clear();
  lv_obj_clean(folderPreviewRows);
  const size_t end=std::min(count,offset+Tab5FolderPreview::PageSize);
  for(size_t i=offset;i<end;++i){const auto& entry=fileDirectoryModel[i];
    String label=baseNameOf(entry.name)+"\n"+parentDirOf(entry.name);
    auto* object=fileCreateContentRow(folderPreviewRows,label.c_str(),entry.directory,fileRowColor(entry.name));
    const int height=2*lv_font_get_line_height(uiFont());lv_obj_set_height(lv_obj_get_child(object,1),height);lv_obj_set_height(object,height+12);
    if(!fileBindRow(object,entry.name,entry.directory,true,0))break;
  }
  if(!count){auto* label=lv_label_create(folderPreviewRows);lv_label_set_text(label,T(fileSearcher.busy()?"Chargement.":"Aucun résultat"));}
  const String page=count?String(static_cast<unsigned long>(offset+1))+"–"+String(static_cast<unsigned long>(end))+" / "+String(static_cast<unsigned long>(count)):String("0 / 0");
  setLabelIfChanged(folderPreviewPageLabel,page.c_str());
  setTopActionVisible(folderPreviewPrevious,offset>0);
  setTopActionVisible(folderPreviewNext,end<count);
  updateFileSearchCaption();updateFileSelectionUi();
}
static void beginFileSearch(const String& directory,const String& text){
  const String path=directory,query=text;
  if(!query.length()){clearFilePreview();fileActiveRight=true;openFileContentDirectory(path);return;}
  clearFilePreview();fileActiveRight=true;cancelFolderPreview();freeFilePreviewImage();
  fileSearchRoot=path;fileSearchQuery=query;fileSearchOffset=0;fileSearchNeedsRestart=false;
  fileContentMode=FileContentMode::Search;fileDisplayedPath=path;fileContentDir=path;fileDisplayedRevision=tab5FsRevision();
  fileDirectoryModel.clear();fileSearcher.begin(path,query);
  setLabelIfChanged(filePreviewTitle,path.c_str());
  lv_obj_add_flag(filePreviewScroll,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(folderPreviewHost,LV_OBJ_FLAG_HIDDEN);
  renderFileSearchPage(0);lv_obj_scroll_to_y(folderPreviewRows,0,LV_ANIM_OFF);updateFileActionState();
}
static void fileClearSearch(){
  const String directory=fileSearchRoot.length()?fileSearchRoot:fileContentDir;
  clearFilePreview();fileActiveRight=true;openFileContentDirectory(directory);
}
static void serviceFileSearch(){
  if(!fileSearcher.busy())return;
  if(activePage!=Page::Files||!pathStorageReady(fileSearchRoot)){fileSearchNeedsRestart=true;fileSearcher.cancel();return;}
  fileSearcher.step([](const String& path,const String&,bool folder){File entry=tab5FS.open(path.c_str());const uint64_t bytes=entry?entry.size():0;const int64_t modified=entry?entry.getLastWrite():0;entry.close();if(fileSelectAllSearch)fileSelection.add(path.c_str());return fileDirectoryModel.add(path.c_str(),folder,bytes,modified);});
  static uint32_t last=0;
  if(!fileSearcher.busy()||millis()-last>=200){last=millis();updateFileSearchCaption();}
  if(!fileSearcher.busy()){
    fileDirectoryModel.sort(Tab5FileSort::selected);renderFileSearchPage(fileSearchOffset);
  }
}
// A file selected in a folder preview owns the same selection as a left row.
// Scanning and pagination are asynchronous, so reveal its row only when ready.
static void focusRevealedFile(){
  if(fileTreeRevealPath.length()||!fileListRevealPath.length()||keyboardScope()!=filesPage)return;
  for(const auto& row:fileSelectionRows)if(!row.right&&row.path==fileListRevealPath.c_str()){
    Tab5Focus.focusObject(row.object);lv_obj_scroll_to_view(row.object,LV_ANIM_OFF);fileListRevealPath="";return;
  }
}
static void selectFolderPreviewEntry(const String& path,bool directory){
  lv_obj_t* object=nullptr;
  for(const auto& row:fileSelectionRows)if(row.right&&row.path==path.c_str()){object=row.object;break;}
  fileSelectEntry(path,directory,object,true);
}
static void startFileListPage(size_t offset){
  if(!fileList)return;
  const int32_t scroll=lv_obj_get_scroll_y(fileList);
  if(!fileTreeRevealPath.length())for(const auto& row:fileSelectionRows)if(!row.right&&row.object==Tab5Focus.focused()){fileListRevealPath=row.path.c_str();break;}
  if(Tab5Focus.contains(fileList,Tab5Focus.focused()))Tab5Focus.clear();
  lv_obj_clean(fileList);fileListOffset=offset;fileListShown=0;
  const size_t count=fileTree.rowCount();
  if(offset>=count&&offset)fileListOffset=offset=count?((count-1)/FILE_LIST_PAGE)*FILE_LIST_PAGE:0;
  if(offset){auto* prev=lv_list_add_button(fileList,LV_SYMBOL_LEFT,T("Page précédente"));lv_obj_add_event_cb(prev,[](lv_event_t*){startFileListPage(fileListOffset>=FILE_LIST_PAGE?fileListOffset-FILE_LIST_PAGE:0);},LV_EVENT_CLICKED,nullptr);}
  for(size_t i=offset;i<count&&fileListShown<FILE_LIST_PAGE;++i){auto row=fileTree.row(i);if(!addFileRow(row.path.c_str(),row.name.c_str(),row.directory,false,row.depth)){fileDirectoryFailed=true;break;}++fileListShown;}
  if(offset+fileListShown<count){auto* next=lv_list_add_button(fileList,LV_SYMBOL_RIGHT,T("Page suivante"));lv_obj_add_event_cb(next,[](lv_event_t*){startFileListPage(fileListOffset+FILE_LIST_PAGE);},LV_EVENT_CLICKED,nullptr);}
  if(fileTree.failed()||fileDirectoryFailed)lv_list_add_text(fileList,T("Mémoire insuffisante — utilisez la recherche"));
  else if(!count)lv_list_add_text(fileList,T(!sdReady&&!Tab5UsbStorage::mounted()?"Carte SD absente":fileTree.busy()?"Chargement.":"Dossier vide"));
  lv_obj_update_layout(fileList);lv_obj_scroll_to_y(fileList,scroll,LV_ANIM_OFF);
  fileTree.clearDirty();focusRevealedFile();updateFileSelectionUi();
}
static void updateFileTreeHints(){
  if(!fileTree.hintsDirty())return;
  for(const auto& row:fileSelectionRows)if(!row.right&&lv_obj_get_child_count(row.object)>2){
    auto* toggle=lv_obj_get_child(row.object,2);
    if(fileTree.hasSubdirectories(row.path.c_str()))lv_obj_remove_flag(toggle,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(toggle,LV_OBJ_FLAG_HIDDEN);
  }
  fileTree.clearHintsDirty();
}
static void serviceFileList(){
  if(!fileTreeRequested||activePage!=Page::Files)return;
  fileTree.syncVolumes(sdReady);fileTree.service();
  if(fileTree.dirty()&&!fileTree.busy())startFileListPage(fileListOffset);
  updateFileTreeHints();
}
static void rememberFileSession(){
  if(!fileSessionStarted||Tab5GitView::scope())return;
  fileSavedTreeScroll=fileList?lv_obj_get_scroll_y(fileList):0;
  fileSavedContentOffset=fileContentMode==FileContentMode::Search?fileSearchOffset:folderPreview.offset;
  auto* content=fileContentMode==FileContentMode::Preview?filePreviewScroll:folderPreviewRows;
  fileSavedContentScroll=content?lv_obj_get_scroll_y(content):0;
  fileSavedFocusPath="";
  for(const auto& row:fileSelectionRows)if(Tab5Focus.contains(row.object,Tab5Focus.focused())){fileSavedFocusPath=row.path.c_str();fileSavedFocusRight=row.right;break;}
  fileSessionRestorePending=true;
}
static void ensureFileSession(){
  if(!fileSessionStarted){
    fileSessionStarted=true;fileTreeRequested=true;fileKnownRevision=tab5FsRevision();
    fileTree.beginVolumes(sdReady);startFileListPage(0);clearFilePreview();
    const String path=sdReady?String("/"):Tab5UsbStorage::mounted()?String("/USB"):String();
    if(path.length()){fileActiveRight=false;selectedPath=path;selectedIsDir=true;fileSelection.add(path.c_str());previewSelectedFile();openFileContentDirectory(path);}
    else clearFileContent();
    updateFileSelectionUi();return;
  }
  fileTree.syncVolumes(sdReady);
  if(fileKnownRevision!=tab5FsRevision()||(fileDisplayedPath.length()&&!pathStorageReady(fileDisplayedPath))){refreshFileList();}
  else if(fileContentMode==FileContentMode::Search){
    if(fileSearchNeedsRestart){const size_t offset=fileSavedContentOffset;restoreFileContent();fileSearchOffset=offset;}
    else {lv_obj_remove_flag(folderPreviewHost,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(filePreviewScroll,LV_OBJ_FLAG_HIDDEN);}
  }else if(fileContentMode==FileContentMode::Directory&&folderPreview.path!=fileDisplayedPath){
    const int32_t scroll=fileSavedContentScroll;const size_t offset=fileSavedContentOffset;restoreFileContent();fileSavedContentScroll=scroll;fileSavedContentOffset=offset;
  }else if(fileContentMode==FileContentMode::Preview){restoreFileContent();}
  if(!fileTree.busy()&&(fileTree.dirty()||!lv_obj_get_child_count(fileList)))startFileListPage(fileListOffset);
  updateFileSelectionUi();
}
static void restoreFileSessionPosition(){
  if(!fileSessionRestorePending||activePage!=Page::Files||keyboardScope()!=filesPage||fileTree.busy()||fileSearcher.busy()||!folderPreview.done)return;
  if(fileSavedFocusPath.length())for(const auto& row:fileSelectionRows)if(row.right==fileSavedFocusRight&&row.path==fileSavedFocusPath.c_str()){Tab5Focus.focusObject(row.object);break;}
  if(fileList){lv_obj_update_layout(fileList);lv_obj_scroll_to_y(fileList,fileSavedTreeScroll,LV_ANIM_OFF);}
  auto* content=fileContentMode==FileContentMode::Preview?filePreviewScroll:folderPreviewRows;
  if(content){lv_obj_update_layout(content);lv_obj_scroll_to_y(content,fileSavedContentScroll,LV_ANIM_OFF);}
  fileSessionRestorePending=false;fileSavedFocusPath="";
}
static void refreshFileList(){
  if(!fileList||!fileSessionStarted)return;
  if(activePage!=Page::Files){fileKnownRevision=UINT32_MAX;return;}
  if(!fileSessionRestorePending)rememberFileSession();fileKnownRevision=tab5FsRevision();
  fileTree.syncVolumes(sdReady);fileTree.refresh();fileDirectoryFailed=false;
  if(!fileTree.busy())startFileListPage(fileListOffset);
  setLabelIfChanged(filePathLabel,"");
  fileSelection.paths.erase(std::remove_if(fileSelection.paths.begin(),fileSelection.paths.end(),[](const std::string& path){return !pathStorageReady(path.c_str())||!tab5FS.exists(path.c_str());}),fileSelection.paths.end());
  if(selectedPath.length()&&(!pathStorageReady(selectedPath)||!tab5FS.exists(selectedPath.c_str())))clearFilePreview();
  if(fileDisplayedPath.length()&&(!pathStorageReady(fileDisplayedPath)||!tab5FS.exists(fileDisplayedPath.c_str())))clearFileContent();
  if(!Tab5GitView::scope()&&fileContentMode!=FileContentMode::Empty)restoreFileContent();
  if(selectedPath.length()){fileMetadataPath="";previewSelectedFile();}updateFileSelectionUi();
}

static void cancelFolderPreview(){
  folderPreview.cancel();
  if(filePreviewScroll&&lv_obj_is_valid(filePreviewScroll))lv_obj_remove_flag(filePreviewScroll,LV_OBJ_FLAG_HIDDEN);
  if(folderPreviewHost&&lv_obj_is_valid(folderPreviewHost))lv_obj_add_flag(folderPreviewHost,LV_OBJ_FLAG_HIDDEN);
  if(folderPreviewRows&&lv_obj_is_valid(folderPreviewRows))lv_obj_clean(folderPreviewRows);
}
static void openFileContentDirectory(const String& directory){
  const String path=directory;
  if(fileContentMode==FileContentMode::Directory&&fileDisplayedPath==path&&fileDisplayedRevision==tab5FsRevision()&&folderPreview.path==path)return;
  fileSearcher.cancel();fileSearchQuery="";fileSearchNeedsRestart=false;fileDirectoryModel.clear();
  fileSavedContentOffset=0;fileSavedContentScroll=0;
  fileContentDir=path;fileDisplayedPath=path;fileContentMode=FileContentMode::Directory;fileDisplayedRevision=tab5FsRevision();
  cancelFolderPreview();freeFilePreviewImage();
  folderPreview.begin(path);
  if(filePreviewTitle)setLabelIfChanged(filePreviewTitle,path.c_str());
  if(filePreviewMeta)setLabelIfChanged(filePreviewMeta,"");
  if(filePreviewLabel)setLabelIfChanged(filePreviewLabel,"");
  if(folderPreviewPageLabel)setLabelIfChanged(folderPreviewPageLabel,"");
  if(folderPreviewHost)lv_obj_remove_flag(folderPreviewHost,LV_OBJ_FLAG_HIDDEN);
  if(filePreviewScroll)lv_obj_add_flag(filePreviewScroll,LV_OBJ_FLAG_HIDDEN);
  updateFileActionState();serviceFolderPreview();
}
static void folderPreviewPageEvent(lv_event_t* event){
  const intptr_t direction=reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
  if(fileContentMode==FileContentMode::Search){
    if(fileSearcher.busy())return;
    renderFileSearchPage(direction>0?fileSearchOffset+Tab5FolderPreview::PageSize:(fileSearchOffset>=Tab5FolderPreview::PageSize?fileSearchOffset-Tab5FolderPreview::PageSize:0));
    lv_obj_scroll_to_y(folderPreviewRows,0,LV_ANIM_OFF);return;
  }
  if(folderPreview.path!=fileContentDir)return;
  const size_t offset=direction>0?folderPreview.offset+Tab5FolderPreview::PageSize:
      (folderPreview.offset>=Tab5FolderPreview::PageSize?folderPreview.offset-Tab5FolderPreview::PageSize:0);
  if(direction>0&&offset>=folderPreview.count)return;
  folderPreview.begin(fileContentDir,offset);lv_obj_clean(folderPreviewRows);lv_obj_scroll_to_y(folderPreviewRows,0,LV_ANIM_OFF);serviceFolderPreview();
}
static void serviceFolderPreview(){
  if(!folderPreview.path.length())return;
  if(activePage!=Page::Files)return;
  if(!pathStorageReady(fileContentDir)){clearFileContent();return;}
  folderPreview.service();
  if(!folderPreview.dirty)return;
  folderPreview.dirty=false;
  if(!folderPreviewHost||!folderPreviewRows)return;
  if(folderPreview.failed){setLabelIfChanged(folderPreviewPageLabel,T("Dossier inaccessible"));return;}
  String page=folderPreview.entries.empty()?String("0"):String(static_cast<unsigned long>(folderPreview.offset+1))+"–"+String(static_cast<unsigned long>(folderPreview.offset+folderPreview.entries.size()));
  page+=" / ";page+=folderPreview.done?String(static_cast<unsigned long>(folderPreview.count)):String("…");
  setLabelIfChanged(folderPreviewPageLabel,page.c_str());
  setTopActionVisible(folderPreviewPrevious,folderPreview.offset>0);
  setTopActionVisible(folderPreviewNext,folderPreview.done&&folderPreview.count>folderPreview.offset+folderPreview.entries.size());
  if(!folderPreview.done)return;
  size_t rendered=lv_obj_get_child_count(folderPreviewRows);
  if(folderPreview.entries.empty()){
    if(!rendered){auto* label=lv_label_create(folderPreviewRows);lv_label_set_text(label,T("Dossier vide"));}
    return;
  }
  if(rendered>folderPreview.entries.size()){lv_obj_clean(folderPreviewRows);rendered=0;}
  for(size_t i=rendered;i<folderPreview.entries.size();++i){const auto& entry=folderPreview.entries[i];
    auto* object=fileCreateContentRow(folderPreviewRows,entry.name.c_str(),entry.directory,fileRowColor(entry.path));
    if(!fileBindRow(object,entry.path,entry.directory,true,0))break;
  }
  updateFileSelectionUi();
}
static void toggleSelectedHidden(lv_event_t*){
  if(fileSelectionHasVolume())return;
  if(Tab5GitView::busy()||Tab5Git::busy()){toast("Une opération Git est déjà en cours.");return;}
  if(Tab5FileOperation::busy()||filePasteBatch.active||fileSelectAllDirectory||fileSelectAllSearch)return;
  auto paths=fileSelection.paths;if(paths.empty()&&selectedPath.length())paths.push_back(selectedPath.c_str());
  if(paths.empty())return;
  bool allHidden=true;for(const auto& path:paths)if(!Tab5HiddenFiles::hidden(path.c_str())){allHidden=false;break;}
  size_t failures=0;String firstFailure;
  for(const auto& path:paths)if(!Tab5HiddenFiles::set(path.c_str(),!allHidden)){++failures;if(!firstFailure.length())firstFailure=path.c_str();}
  if(failures){String message=String(failures)+" / "+String(paths.size())+" "+T("éléments non modifiés")+"\n"+firstFailure;toast(message.c_str(),5000);}
  if(!allHidden&&!Tab5Files::showHidden)clearFilePreview();
  refreshFileList();updateFileSelectionUi();
}
static void selectAllFiles(){
  fileSelection.clear();fileSelectAllDirectory.close();fileSelectAllSearch=false;
  if(fileActiveRight&&fileContentMode==FileContentMode::Search){
    for(size_t i=0;i<fileDirectoryModel.size();++i)fileSelection.add(fileDirectoryModel[i].name);
    fileSelectAllSearch=fileSearcher.busy();
  }else if(fileActiveRight){
    fileSelectAllRoot=fileContentDir;fileSelectAllDirectory=tab5FS.open(fileSelectAllRoot.c_str(),FILE_READ);
    if(!fileSelectAllDirectory||!fileSelectAllDirectory.isDirectory()){fileSelectAllDirectory.close();toast("Dossier inaccessible");}
  }else for(size_t i=0;i<fileTree.rowCount();++i){auto row=fileTree.row(i);if(row.path!="/"&&row.path!="/USB")fileSelection.add(row.path);}
  updateFileSelectionUi();
}
static void serviceFileSelection(){
  serviceFileTreeReveal();focusRevealedFile();restoreFileSessionPosition();
  if(activePage!=Page::Files){fileSelectAllDirectory.close();fileSelectAllSearch=false;return;}
  if(fileSelectAllSearch&&!fileSearcher.busy()){fileSelectAllSearch=false;updateFileSelectionUi();}
  if(!fileSelectAllDirectory)return;
  const uint32_t began=millis();
  for(unsigned i=0;i<8&&millis()-began<5;++i){
    File entry=fileSelectAllDirectory.openNextFile();
    if(!entry){fileSelectAllDirectory.close();break;}
    String name=baseNameOf(String(entry.name()));entry.close();if(!name.length()||name=="."||name=="..")continue;
    String path=joinPath(fileSelectAllRoot,name);
    if(Tab5Files::showHidden||!Tab5HiddenFiles::hidden(path.c_str()))fileSelection.add(path.c_str());
  }
  updateFileSelectionUi();
}
static bool fileFocusTreePath(const String& path,bool select){
  size_t index=0;if(!fileTree.rowIndex(path,index))return false;
  const size_t page=(index/FILE_LIST_PAGE)*FILE_LIST_PAGE;
  if(page!=fileListOffset)startFileListPage(page);
  for(const auto& row:fileSelectionRows)if(!row.right&&row.path==path.c_str()){
    if(select)fileSelectEntry(row.path.c_str(),row.directory,row.object,false);
    Tab5Focus.focusObject(row.object);lv_obj_scroll_to_view(row.object,LV_ANIM_OFF);return true;
  }
  return false;
}
static void fileRequestTreeReveal(const String& requested){
  if(!requested.length())return;
  fileTreeRequested=true;fileListRevealPath="";fileTreeRevealPath=requested;
  if(!fileTree.volumes())fileTree.beginVolumes(sdReady);
  else fileTree.syncVolumes(sdReady);
  serviceFileTreeReveal();
}
static void serviceFileTreeReveal(){
  if(!fileTreeRevealPath.length()||activePage!=Page::Files||keyboardScope()!=filesPage)return;
  const String path=fileTreeRevealPath;
  if(fileTree.busy())return;
  if(fileTree.dirty())startFileListPage(fileListOffset);
  if(fileFocusTreePath(path,false)){fileTreeRevealPath="";return;}
  const String support=Tab5UsbStorage::path(path.c_str())?String("/USB"):String("/");
  if(!pathStorageReady(path)){fileTreeRevealPath="";return;}
  if(!fileTree.expanded(support)){if(!fileTree.expand(support))fileTreeRevealPath="";return;}
  int slash=path.indexOf('/',support=="/"?1:5);
  while(slash>=0){
    const String ancestor=path.substring(0,slash);
    if(!fileTree.expanded(ancestor)){if(!fileTree.expand(ancestor))fileTreeRevealPath="";return;}
    slash=path.indexOf('/',slash+1);
  }
  fileTreeRevealPath="";
}
static void fileParentNavigation(){
  fileSessionRestorePending=false;fileSavedFocusPath="";
  if(fileContentMode==FileContentMode::Empty)return;
  const bool returnToDirectory=fileContentMode==FileContentMode::Preview||fileContentMode==FileContentMode::Search;
  const String parent=returnToDirectory?fileContentDir:parentDirOf(fileDisplayedPath);
  if(parent.length()&&(returnToDirectory||parent!=fileDisplayedPath)){
    clearFilePreview();fileActiveRight=true;openFileContentDirectory(parent);updateFileActionState();
  }
}
static void newFolderEvent(lv_event_t* e);
static void renameSelectedEvent(lv_event_t* e);
static bool fileSelectionKey(const writing::KeyEvent& e){
  using K=writing::Key;
  if(Tab5Focus.controlling()||filePasteBatch.active||Tab5FileOperation::busy())return false;
  if(e.key==K::Escape&&fileTreeRevealPath.length()){
    fileTreeRevealPath="";fileListRevealPath="";return true;
  }
  if(e.key==K::F2){renameSelectedEvent(nullptr);return true;}
  if(e.key==K::Backspace||(e.alt&&(e.key==K::Up||e.key==K::Left))){fileParentNavigation();return true;}
  if((e.ctrl||e.meta)&&e.key==K::Text){
    char key=e.text[0];if(key>='A'&&key<='Z')key+=32;
    if(key=='n'&&e.shift){newFolderEvent(nullptr);return true;}
    if(key=='a'){selectAllFiles();return true;}
    if(key=='h'&&e.shift){toggleSelectedHidden(nullptr);return true;}
  }
  auto found=std::find_if(fileSelectionRows.begin(),fileSelectionRows.end(),[](const FileSelectionRow& row){return Tab5Focus.contains(row.object,Tab5Focus.focused());});
  if(found==fileSelectionRows.end())return false;
  // A focused row reached by Tab belongs to its own pane, even when the last
  // selected context is still on the other side. Focus alone is never selection.
  const FileSelectionRow origin=*found;
  if(e.key==K::Text&&e.text[0]==' '&&!e.alt){fileSelectEntry(origin.path.c_str(),origin.directory,origin.object,origin.right,true);return true;}
  if(!origin.right&&(e.key==K::Left||e.key==K::Right)&&!e.alt){
    if(e.key==K::Left){
      if(origin.directory&&fileTree.expanded(origin.path.c_str()))fileTree.collapse(origin.path.c_str());
      else fileFocusTreePath(parentDirOf(origin.path.c_str()),true);
    }else if(origin.directory){
      if(!fileTree.expanded(origin.path.c_str()))fileTree.expand(origin.path.c_str());
      else for(size_t i=0;i<fileTree.rowCount();++i){auto row=fileTree.row(i);if(parentDirOf(row.path.c_str())==origin.path.c_str()){fileFocusTreePath(row.path.c_str(),true);break;}}
    }
    return true;
  }
  if(e.key!=K::Up&&e.key!=K::Down&&e.key!=K::Home&&e.key!=K::End)return false;
  if(e.alt)return false;
  std::vector<FileSelectionRow> rows;
  for(const auto& row:fileSelectionRows)if(row.right==origin.right)rows.push_back(row);
  size_t index=0;while(index<rows.size()&&rows[index].object!=origin.object)++index;
  if(index>=rows.size())return false;
  if(e.key==K::Home)index=0;else if(e.key==K::End)index=rows.size()-1;
  else if(e.key==K::Up){if(!index)return true;--index;}else{if(index+1>=rows.size())return true;++index;}
  const auto target=rows[index];
  if(fileSelection.anchor.empty())fileSelection.anchor=origin.path;
  if(e.shift||(!e.ctrl&&!e.meta))fileSelectEntry(target.path.c_str(),target.directory,target.object,target.right,false,e.shift,e.ctrl||e.meta);
  Tab5Focus.focusObject(target.object);lv_obj_scroll_to_view(target.object,LV_ANIM_OFF);return true;
}

static void serviceFileAutoRefresh(){
  serviceFileSelection();
  if(fileRefreshScroll>=0&&!fileTree.busy()&&!fileSearcher.busy()){
    if(activePage==Page::Files&&fileList){lv_obj_update_layout(fileList);lv_obj_scroll_to_y(fileList,fileRefreshScroll,LV_ANIM_OFF);}
    fileRefreshScroll=-1;
  }
  const uint32_t revision=tab5FsRevision();
  static uint32_t pendingRevision=0,changedAt=0;
  if(revision==fileKnownRevision)return;
  if(revision!=pendingRevision){pendingRevision=revision;changedAt=millis();return;}
  if(millis()-changedAt<350||activePage!=Page::Files||Tab5Tools.scope()||Tab5GitView::scope()||Tab5ContextMenu::scope()||fileTree.busy()||fileSearcher.busy()||
      Tab5FileOperation::busy()||filePasteBatch.active||fileSelectAllDirectory||fileSelectAllSearch||dialogOverlay||filePopupOverlay||fileDeleteOverlay)return;
  const int32_t scroll=fileList?lv_obj_get_scroll_y(fileList):0;
  const size_t offset=fileListOffset;
  if(selectedPath.length()&&!tab5FS.exists(selectedPath.c_str()))clearFilePreview();
  refreshFileList();
  if(offset&&!fileTree.busy())startFileListPage(offset);
  fileRefreshScroll=scroll;
  homeAppsDirty=true;
}

// -----------------------------------------------------------------------------
// Dialogs
// -----------------------------------------------------------------------------

static void dialogOkEvent(lv_event_t* e) {
  (void)e;
  if (!dialogInput) return;
  if((dialogAction==TextDialogAction::NewFolder||dialogAction==TextDialogAction::Rename||dialogAction==TextDialogAction::SaveAs)&&(Tab5GitView::busy()||Tab5Git::busy())){toast("Une opération Git est déjà en cours.");return;}
  String value = lv_textarea_get_text(dialogInput);

  if (dialogAction == TextDialogAction::FileSearch) {
    value.trim();const String directory=dialogDirectory;
    closeTextDialog();beginFileSearch(directory,value);
  } else if (dialogAction == TextDialogAction::AudioSearch) {
    value.trim();audioSearchQueries[unsigned(audioView)]=value;
    closeTextDialog();refreshAudioList();
  } else if (dialogAction == TextDialogAction::SaveAs) {
    value = sanitizeFilename(value, true);
    String target = joinPath(dialogDirectory, value);
    closeTextDialog();
    saveDocumentTo(target, true);
    refreshFileList();
  } else if (dialogAction == TextDialogAction::NewFolder) {
    value = sanitizeFilename(value, false);
    String target = joinPath(dialogDirectory, value);
    closeTextDialog();
    if (!Tab5Files::userWritable(target.c_str())) { toast("Chemin invalide"); return; }
    if (!tab5FS.mkdir(target.c_str())) toast("Création dossier impossible");
    else {if(activePage==Page::Files)refreshFileList();else if(activePage==Page::Audio&&audioList)refreshAudioList();}
  } else if (dialogAction == TextDialogAction::Rename) {
    value = sanitizeFilename(value, false);
    const String source = pendingRenamePath;
    const String target = joinPath(dialogDirectory, value);
    pendingRenamePath = "";
    closeTextDialog();
    if (!Tab5Files::userWritable(source.c_str()) || !Tab5Files::userWritable(target.c_str())) { toast("Chemin invalide"); return; }
    if (!source.length() || target == source) return;
    if (tab5FS.exists(target.c_str())) {
      toast("Ce nom existe déjà");
      return;
    }
    File sourceFile = tab5FS.open(source.c_str(), FILE_READ);
    const bool sourceIsDir = sourceFile && sourceFile.isDirectory();
    if (sourceFile) sourceFile.close();
    if (Tab5Audio.currentPath() == source) Tab5Audio.stop();
    if (!tab5FS.rename(source.c_str(), target.c_str())) {
      toast("Renommage impossible");
      return;
    }
    if (currentPath == source) currentPath = target;
    else if (currentPath.startsWith(source + "/"))
      currentPath = target + currentPath.substring(source.length());
    if (lastReaderPath == source || lastReaderPath.startsWith(source + "/")) {
      const String oldReaderPath = lastReaderPath;
      lastReaderPath = target + lastReaderPath.substring(source.length());
      migrateReaderProgress(oldReaderPath, lastReaderPath);
      markSettingsDirty();
    }
    if (clipboardPath == source || clipboardPath.startsWith(source + "/"))
      clipboardPath = target + clipboardPath.substring(source.length());
    if (currentDir == source || currentDir.startsWith(source + "/"))
      currentDir = target + currentDir.substring(source.length());
    if(fileContentDir==source||fileContentDir.startsWith(source+"/"))fileContentDir=target+fileContentDir.substring(source.length());
    if(fileDisplayedPath==source||fileDisplayedPath.startsWith(source+"/"))fileDisplayedPath=target+fileDisplayedPath.substring(source.length());
    fileSelection.clear();fileSelection.add(target.c_str());
    selectedPath = target;
    selectedIsDir = sourceIsDir;
    updateDocumentTitle();
    Tab5Audio.reloadLibrary();
    refreshFileList();
    toast("Élément renommé");
  } else if (dialogAction == TextDialogAction::SetClock) {
    bool ok = setClockFromString(value);
    closeTextDialog();
    if (!ok) toast("Format attendu : AAAA-MM-JJ HH:MM", 3000);
    else {
      updateClockLabels(true);
      toast(rtcReady ? "RTC réglée" : "Heure réglée — RTC indisponible");
    }
  } else if (dialogAction == TextDialogAction::MapAddPlace) {
    value.trim();
    const double latitude = mapLatitude;
    const double longitude = mapLongitude;
    const uint8_t zoom = mapZoom;
    closeTextDialog();
    if (!value.length()) {
      toast("Nom du lieu requis");
    } else if (Tab5Map.addPlace(value, latitude, longitude, zoom)) {
      toast("Lieu enregistré");
      refreshMapPlacesList();
    } else {
      toast("Enregistrement du lieu impossible");
    }
  } else if (dialogAction == TextDialogAction::MapRenamePlace) {
    value.trim();
    const size_t index = pendingMapPlaceIndex;
    closeTextDialog();
    if (!value.length()) {
      toast("Nom du lieu requis");
    } else if (index < Tab5Map.placeCount() &&
               Tab5Map.renamePlace(index, value)) {
      toast("Lieu renommé");
      refreshMapPlacesList();
    } else {
      toast("Renommage impossible");
    }
  } else if (dialogAction == TextDialogAction::MapSearch) {
    value.trim();
    closeTextDialog();
    if (!mapSearchItems) {
      toast("Mémoire PSRAM indisponible pour la recherche", 3000);
    } else if (value.length() < 2) {
      toast("Saisissez au moins deux caractères");
    } else {
      ++mapSearchRequestId;
      if (!mapSearchRequestId) ++mapSearchRequestId;
      mapSearchQuery = value;
      mapSearchPending = Tab5Map.requestSearch(value, mapSearchRequestId,mapLatitude,mapLongitude,true);
      if (mapSearchPending) {
        if (mapStatusLabel) setLabelIfChanged(mapStatusLabel,T("Recherche du lieu…"));
      } else toast("Recherche impossible");
    }
  } else {
    closeTextDialog();
  }
}

static void dialogCancelEvent(lv_event_t* e) {
  (void)e;
  closeTextDialog();
}

static void openTextDialog(TextDialogAction action, const char* title, const String& initial, const String& directory) {
  closeTextDialog();
  dialogAction = action;
  dialogDirectory = directory;

  dialogOverlay = lv_obj_create(root);
  lv_obj_set_size(dialogOverlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(dialogOverlay, 0, 0);
  lv_obj_set_style_bg_color(dialogOverlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(dialogOverlay, LV_OPA_50, 0);
  lv_obj_set_style_border_width(dialogOverlay, 0, 0);
  lv_obj_remove_flag(dialogOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(dialogOverlay);

  dialogBox = lv_obj_create(dialogOverlay);
  lv_obj_set_size(dialogBox, 650, 210);
  lv_obj_align(dialogBox, LV_ALIGN_TOP_MID, 0, 95);
  lv_obj_remove_flag(dialogBox, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(dialogBox, C_SURFACE(), 0);
  lv_obj_set_style_border_color(dialogBox, C_BORDER(), 0);

  lv_obj_t* t = lv_label_create(dialogBox);
  lv_label_set_text(t, T(title));
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 8, 4);

  dialogInput = lv_textarea_create(dialogBox);
  lv_textarea_set_one_line(dialogInput, true);
  lv_textarea_set_text(dialogInput, initial.c_str());
  lv_obj_set_size(dialogInput, 610, 58);
  lv_obj_set_style_bg_color(dialogInput, C_BG(), 0);
  lv_obj_set_style_text_color(dialogInput, C_TEXT(), 0);
  lv_obj_set_style_text_font(dialogInput, uiFont(), 0);
  lv_obj_align(dialogInput, LV_ALIGN_TOP_MID, 0, 45);
  lv_textarea_set_cursor_pos(dialogInput, LV_TEXTAREA_CURSOR_LAST);

  lv_obj_t* cancel = makeButton(dialogBox, "Annuler", 160, 50);
  lv_obj_align(cancel, LV_ALIGN_BOTTOM_LEFT, 18, -8);
  lv_obj_add_event_cb(cancel, dialogCancelEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* keys = makeIconButton(dialogBox, LV_SYMBOL_KEYBOARD, 64, 50);
  lv_obj_align(keys, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_add_event_cb(keys, [](lv_event_t*) {
    setKeyboardVisible(!keyboardVisible, dialogInput);
    Tab5Focus.editObject(dialogInput);
  }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* ok = makeButton(dialogBox, "OK", 160, 50);
  lv_obj_align(ok, LV_ALIGN_BOTTOM_RIGHT, -18, -8);
  lv_obj_add_event_cb(ok, dialogOkEvent, LV_EVENT_CLICKED, nullptr);

  if (action == TextDialogAction::SetClock) keyboardMode = KeyboardMode::Symbols;
  else keyboardMode = KeyboardMode::Lower;
  rebuildKeyboard();
  lv_obj_add_event_cb(dialogInput, [](lv_event_t* e) { setKeyboardVisible(keyboardVisible || !writing::usbConnected(),lv_event_get_target_obj(e)); }, LV_EVENT_CLICKED, nullptr);
  setKeyboardVisible(!writing::usbConnected(), dialogInput);
  Tab5Focus.focusInitial(dialogOverlay,dialogInput);
  lv_obj_add_event_cb(dialogInput,dialogOkEvent,LV_EVENT_READY,nullptr);
  if(action==TextDialogAction::MapSearch||action==TextDialogAction::FileSearch||action==TextDialogAction::AudioSearch){
    lv_textarea_set_max_length(dialogInput,96);Tab5Focus.editObject(dialogInput);

  }
}

static void closeTextDialog() {
  auto* old=dialogOverlay;
  if(old&&Tab5Focus.contains(old,Tab5Focus.focused()))Tab5Focus.clear();
  if(old&&keyboardTarget&&Tab5Focus.contains(old,keyboardTarget))keyboardTarget=nullptr;
  // Keyboard/layout deletion callbacks must never rediscover a dying dialog.
  dialogOverlay = nullptr;
  dialogBox = nullptr;
  dialogInput = nullptr;
  dialogAction = TextDialogAction::None;
  pendingRenamePath = "";
  pendingMapPlaceIndex = static_cast<size_t>(-1);
  setKeyboardVisible(false);
  if(old&&lv_obj_is_valid(old))lv_obj_delete(old);
  Tab5Focus.restore(keyboardScope());
}

static void closeFileDeleteDialog(){
  pendingDeletePath="";pendingDeletePaths.clear();
  auto* old=fileDeleteOverlay;fileDeleteOverlay=nullptr;
  if(old&&lv_obj_is_valid(old))lv_obj_delete(old);Tab5Focus.restore(keyboardScope());
}
static void confirmDeleteYes(lv_event_t*) {
  if(Tab5GitView::busy()||Tab5Git::busy()){toast("Une opération Git est déjà en cours.");return;}
  const auto paths=pendingDeletePaths;closeFileDeleteDialog();
  fileTree.cancel();fileListDir.close();fileSearcher.cancel();fileSelectAllDirectory.close();fileSelectAllSearch=false;cancelFolderPreview();
  size_t done=0;String firstFailure;
  for(const auto& selected:paths){
    String path=selected.c_str();
    if(Tab5Audio.currentPath()==path)Tab5Audio.stop();
    if(!pathStorageReady(path)||!deletePathRecursive(path)){if(!firstFailure.length())firstFailure=path;continue;}
    ++done;
    if(currentPath==path||currentPath.startsWith(path+"/"))newDocument();
    if(lastReaderPath==path||lastReaderPath.startsWith(path+"/")){tab5FS.remove(readerProgressPath(lastReaderPath).c_str());lastReaderPath="";markSettingsDirty();}
    if(clipboardPath==path||clipboardPath.startsWith(path+"/")){clipboardPath="";clipboardCut=false;}
  }
  fileClipboardPaths.erase(std::remove_if(fileClipboardPaths.begin(),fileClipboardPaths.end(),[](const std::string& path){return !tab5FS.exists(path.c_str());}),fileClipboardPaths.end());
  String message=String(done)+" / "+String(paths.size())+" "+T("éléments supprimés");
  if(firstFailure.length())message+=String("\n")+T("Suppression impossible")+" : "+firstFailure;
  toast(message.c_str(),firstFailure.length()?6000:2500);
  clearFilePreview();refreshFileList();Tab5Audio.reloadLibrary();if(audioList)refreshAudioList();
}
static void confirmDeleteNo(lv_event_t*) {closeFileDeleteDialog();}

static void askDeletePath(const String& path) {
  if (path.length() == 0 || !pathStorageReady(path)) return;
  if (!Tab5Files::userWritable(path.c_str())) { toast("Chemin invalide"); return; }
  warnSystemFile(path);
  closeFileDeleteDialog();pendingDeletePath=path;
  pendingDeletePaths=activePage==Page::Files&&!fileSelection.paths.empty()?fileSelection.roots():std::vector<std::string>{path.c_str()};
  for(const auto& item:pendingDeletePaths)if(!Tab5Files::userWritable(item.c_str())){pendingDeletePaths.clear();toast("Chemin invalide");return;}
  lv_obj_t* overlay = fileDeleteOverlay = lv_obj_create(root);
  lv_obj_set_size(overlay, SCREEN_W, SCREEN_H);
  lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_50, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(overlay);

  lv_obj_t* box = lv_obj_create(overlay);
  lv_obj_set_size(box, 600, 230);
  lv_obj_center(box);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(box, C_SURFACE(), 0);
  lv_obj_set_style_border_color(box, C_BORDER(), 0);

  lv_obj_t* l = lv_label_create(box);
  File target = tab5FS.open(path.c_str(), FILE_READ);
  const bool isDir = target && target.isDirectory();
  if (target) target.close();
  String msg = isDir ? T("Supprimer ce dossier et tout son contenu :\n") :
                       T("Supprimer définitivement :\n");
  msg += baseNameOf(path) + " ?";
  if(pendingDeletePaths.size()>1)msg=String(T("Supprimer définitivement la sélection ?"))+"\n"+String(pendingDeletePaths.size())+" "+T("éléments, dossiers et contenu inclus");
  lv_label_set_text(l, msg.c_str());
  lv_obj_set_width(l, 540);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_obj_align(l, LV_ALIGN_TOP_MID, 0, 12);

  lv_obj_t* no = makeButton(box, "Annuler", 180, 52);
  lv_obj_align(no, LV_ALIGN_BOTTOM_LEFT, 20, -12);
  lv_obj_add_event_cb(no, confirmDeleteNo, LV_EVENT_CLICKED, overlay);

  lv_obj_t* yes = makeButton(box, "Supprimer", 180, 52);
  lv_obj_align(yes, LV_ALIGN_BOTTOM_RIGHT, -20, -12);
  lv_obj_add_event_cb(yes, confirmDeleteYes, LV_EVENT_CLICKED, overlay);
  Tab5Focus.focusInitial(overlay,no);
}

// -----------------------------------------------------------------------------
// UI events
// -----------------------------------------------------------------------------

static void dockEvent(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  shellLaunchDock(unsigned(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e))));
}

static void saveEvent(lv_event_t* e) {
  (void)e;
  if (currentPath.length()) saveDocumentTo(currentPath, true);
  else openTextDialog(TextDialogAction::SaveAs, "Enregistrer sous", "document.txt", currentDir);
}

static void keyboardToggleEvent(lv_event_t* e) {
  (void)e;
  if (activePage != Page::Editor) showPage(Page::Editor);
  keyboardMode = KeyboardMode::Lower;
  rebuildKeyboard();
  setKeyboardVisible(!keyboardVisible, editor);
  if (keyboardVisible) lv_obj_add_state(editor, LV_STATE_FOCUSED);
}

static void undoEvent(lv_event_t* e) {
  (void)e;
  if (historyPending) pushHistorySnapshot(true);
  if (historyIndex > 0) restoreHistoryAt(historyIndex - 1);
}

static void redoEvent(lv_event_t* e) {
  (void)e;
  if (!historyPending && historyIndex >= 0 && historyIndex < historyCount - 1)
    restoreHistoryAt(historyIndex + 1);
}

static void editorCopyEvent(lv_event_t* e) {
  (void)e;
  if (!copyEditorSelectionToClipboard()) {
    toast("Sélectionnez du texte à copier");
    return;
  }
  updateEditorClipboardState();
  toast("Texte copié");
}

static void editorCutEvent(lv_event_t* e) {
  (void)e;
  if (!copyEditorSelectionToClipboard()) {
    toast("Sélectionnez du texte à couper");
    return;
  }
  if (!replaceEditorSelection("")) {
    toast("Coupe impossible");
    return;
  }
  updateEditorClipboardState();
  toast("Texte coupé");
}

static void editorPasteEvent(lv_event_t* e) {
  (void)e;
  if (!editorClipboard.length()) {
    toast("Presse-papiers texte vide");
    return;
  }
  if (!replaceEditorSelection(editorClipboard.c_str())) {
    toast("Collage impossible");
    return;
  }
  updateEditorClipboardState();
  toast("Texte collé");
}

static void selectedOpenEvent(lv_event_t* e) {
  (void)e;
  if (!selectedPath.length()) return;
  if (selectedIsDir) {
    openFileContentDirectory(selectedPath);
  } else if (selectedIsAudio || isAudioExtension(selectedPath)) {
    openAudioPath(selectedPath);
  } else if (selectedIsVideo || isVideoExtension(selectedPath)) {
    startVideoViewerSource(selectedPath, baseNameOf(selectedPath));
  } else if (isShellExtension(selectedPath)) {
    openSelectedShellScript();
  } else if(isImageProject(selectedPath)){
    openImageEditor(selectedPath.c_str());
  } else if (isExecutableExtension(selectedPath)) {
    setKeyboardVisible(false);Tab5Tools.launchExecutable(selectedPath.c_str());
  } else if (isFirmwareExtension(selectedPath)) {
    setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Firmware,selectedPath.c_str());
  } else if (selectedIsText || isReadableTextFile(selectedPath)) {
    loadDocumentFrom(selectedPath);
  } else if (isEpubExtension(selectedPath)) {
    startReader(selectedPath);
  } else if (selectedIsImage || isReadableImageFile(selectedPath)) {
    startImageViewer(selectedPath);
  } else {
    toast("Aucune application associée à cette extension", 2400);
  }
}

static void selectedReadEvent(lv_event_t* e) {
  (void)e;
  if (selectedPath.length() && !selectedIsDir &&
      (selectedIsText || isEpubExtension(selectedPath)))
    startReader(selectedPath);
}

static void selectedExecuteEvent(lv_event_t* e) {
  (void)e;
  if (!selectedPath.length() || selectedIsDir || (!isPythonExtension(selectedPath)&&!isShellExtension(selectedPath)&&!isFirmwareExtension(selectedPath)&&!isExecutableExtension(selectedPath))) return;
  if(isShellExtension(selectedPath)){openSelectedShellScript();return;}
  setKeyboardVisible(false);
  if(isExecutableExtension(selectedPath))Tab5Tools.launchExecutable(selectedPath.c_str());
  else if(isFirmwareExtension(selectedPath))Tab5Tools.open(Tab5Tool::Firmware,selectedPath.c_str());
  else Tab5Tools.openPythonApp(selectedPath.c_str());
}

static void copySelectedEvent(lv_event_t*) {
  if(fileSelectionHasVolume()||fileSelectAllDirectory||fileSelectAllSearch)return;
  fileClipboardPaths=fileSelection.roots();
  if(fileClipboardPaths.empty()&&selectedPath.length())fileClipboardPaths.push_back(selectedPath.c_str());
  if(fileClipboardPaths.empty())return;
  clipboardPath=fileClipboardPaths.front().c_str();clipboardCut=false;
  updateFileActionState();toast("Sélection copiée dans le presse-papiers");
}
static void cutSelectedEvent(lv_event_t*) {
  if(fileSelectionHasVolume()||fileSelectAllDirectory||fileSelectAllSearch)return;
  copySelectedEvent(nullptr);if(fileClipboardPaths.empty())return;
  clipboardCut=true;updateFileActionState();toast("Sélection prête à être déplacée");
}

static void closeFileOperationDialog(){
  if(filePasteBatch.active){filePasteBatch.cancelled=true;if(Tab5FileOperation::busy())Tab5FileOperation::cancel();return;}
  if(Tab5FileOperation::busy()){Tab5FileOperation::cancel();return;}
  Tab5Focus.clear();
  if(fileOperationOverlay)lv_obj_delete(fileOperationOverlay);
  fileOperationOverlay=nullptr;fileOperationMessage=nullptr;fileOperationButton=nullptr;
  Tab5Focus.restore(keyboardScope());
}
static void showFileOperationDialog(const String& destination){
  setKeyboardVisible(false);Tab5Focus.clear();
  fileOperationOverlay=lv_obj_create(root);lv_obj_set_size(fileOperationOverlay,SCREEN_W,SCREEN_H);
  lv_obj_set_style_bg_color(fileOperationOverlay,lv_color_black(),0);
  lv_obj_set_style_bg_opa(fileOperationOverlay,LV_OPA_50,0);
  lv_obj_remove_flag(fileOperationOverlay,LV_OBJ_FLAG_SCROLLABLE);
  auto* box=lv_obj_create(fileOperationOverlay);lv_obj_set_size(box,760,320);lv_obj_center(box);
  lv_obj_set_style_bg_color(box,C_SURFACE(),0);lv_obj_remove_flag(box,LV_OBJ_FLAG_SCROLLABLE);
  auto* heading=lv_label_create(box);lv_label_set_text(heading,T("Copie des fichiers"));lv_obj_set_pos(heading,16,12);
  fileOperationMessage=lv_label_create(box);lv_obj_set_pos(fileOperationMessage,16,58);lv_obj_set_width(fileOperationMessage,690);
  lv_label_set_long_mode(fileOperationMessage,LV_LABEL_LONG_WRAP);
  String message=String(T("Destination"))+" : "+destination;
  lv_label_set_text(fileOperationMessage,message.c_str());
  fileOperationButton=makeButton(box,T("Annuler"),180,48);lv_obj_align(fileOperationButton,LV_ALIGN_BOTTOM_RIGHT,-12,-10);
  lv_obj_add_event_cb(fileOperationButton,[](lv_event_t*){closeFileOperationDialog();},LV_EVENT_CLICKED,nullptr);
  lv_obj_move_foreground(fileOperationOverlay);Tab5Focus.focusFirst(fileOperationOverlay);
}
static void completeFilePaste(const String& source,const String& destination,bool sourceIsDir,bool moved){
  if(moved){
    if(currentPath==source||currentPath.startsWith(source+"/"))
      currentPath=destination+currentPath.substring(source.length());
    if(lastReaderPath==source||lastReaderPath.startsWith(source+"/")){
      const String oldReaderPath=lastReaderPath;
      lastReaderPath=destination+lastReaderPath.substring(source.length());
      migrateReaderProgress(oldReaderPath,lastReaderPath);markSettingsDirty();
    }
    if(clipboardPath==source){clipboardPath="";clipboardCut=false;}
    if(currentDir==source||currentDir.startsWith(source+"/"))currentDir=destination+currentDir.substring(source.length());
    if(fileContentDir==source||fileContentDir.startsWith(source+"/"))fileContentDir=destination+fileContentDir.substring(source.length());
    if(fileDisplayedPath==source||fileDisplayedPath.startsWith(source+"/"))fileDisplayedPath=destination+fileDisplayedPath.substring(source.length());
    updateDocumentTitle();
  }
  selectedPath=destination;selectedIsDir=sourceIsDir;
  fileSelection.clear();fileSelection.add(destination.c_str());
  if(activePage==Page::Files&&!filePasteBatch.active)refreshFileList();
  updateFileActionState();
}
static void finishFilePasteBatch(){
  filePasteBatch.active=false;
  String message=String(filePasteBatch.done)+" / "+String(filePasteBatch.paths.size())+" "+T("éléments traités");
  if(filePasteBatch.cancelled)message+=String("\n")+T("Opération annulée");
  if(filePasteBatch.failed)message+=String("\n")+String(filePasteBatch.failed)+" "+T("échecs")+"\n"+filePasteBatch.lastError;
  if(filePasteBatch.retained)message+=String("\n")+String(filePasteBatch.retained)+" "+T("originaux conservés après copie vérifiée");
  if(fileOperationMessage){lv_label_set_text(fileOperationMessage,message.c_str());lv_label_set_text(lv_obj_get_child(fileOperationButton,0),T("Fermer"));Tab5Focus.focusInitial(fileOperationOverlay,fileOperationButton);}
  else toast(message.c_str(),5000);
  if(filePasteBatch.cut){fileClipboardPaths.erase(std::remove_if(fileClipboardPaths.begin(),fileClipboardPaths.end(),[](const std::string& path){return !tab5FS.exists(path.c_str());}),fileClipboardPaths.end());clipboardPath=fileClipboardPaths.empty()?String():String(fileClipboardPaths.front().c_str());clipboardCut=!fileClipboardPaths.empty();}
  refreshFileList();updateFileActionState();
}
static void advanceFilePasteBatch(){
  if(!filePasteBatch.active||Tab5FileOperation::busy())return;
  if(filePasteBatch.cancelled||filePasteBatch.next>=filePasteBatch.paths.size()){finishFilePasteBatch();return;}
  const String source=filePasteBatch.paths[filePasteBatch.next++].c_str();
  File file=tab5FS.open(source.c_str(),FILE_READ);
  if(!file){++filePasteBatch.failed;filePasteBatch.lastError=String(T("Source introuvable"))+" : "+source;return;}
  const bool directory=file.isDirectory();file.close();
  if((filePasteBatch.cut&&parentDirOf(source)==filePasteBatch.directory)||
     (directory&&Tab5Files::sameOrDescendant(filePasteBatch.directory.c_str(),source.c_str()))){
    ++filePasteBatch.failed;filePasteBatch.lastError=String(T("Destination invalide"))+" : "+source;return;
  }
  const String destination=uniqueDestination(filePasteBatch.directory,source,directory);
  if(!destination.length()||!Tab5Files::userWritable(destination.c_str())){++filePasteBatch.failed;filePasteBatch.lastError=T("Destination invalide");return;}
  if(filePasteBatch.cut&&!Tab5Python::running()&&!tab5ExecutableBusy()&&!tab5FileShareBusy()&&!tab5FileShareActive()&&
     Tab5UsbStorage::path(source.c_str())==Tab5UsbStorage::path(destination.c_str())&&
     !tab5FS.exists(destination.c_str())&&tab5FS.rename(source.c_str(),destination.c_str())){
    completeFilePaste(source,destination,directory,true);++filePasteBatch.done;return;
  }
  if(!Tab5FileOperation::startCopy(source.c_str(),destination.c_str(),filePasteBatch.cut)){
    ++filePasteBatch.failed;filePasteBatch.lastError=source+" : "+T(Tab5FileOperation::status());
  }
}
static void serviceFileOperation(){
  Tab5FileOperation::Completion completed;
  if(Tab5FileOperation::takeCompletion(completed)){
    if(completed.success){
      completeFilePaste(completed.source,completed.destination,completed.sourceIsDirectory,completed.moved);
      ++filePasteBatch.done;if(completed.requestedMove&&!completed.moved)++filePasteBatch.retained;
    }else{
      ++filePasteBatch.failed;
      filePasteBatch.lastError=String(completed.source)+" : "+T(Tab5FileOperation::status());
      if(completed.destinationPresent)filePasteBatch.lastError+=String("\n")+T("Une copie partielle reste à destination.")+"\n"+completed.destination;
      if(completed.cancelled)filePasteBatch.cancelled=true;
    }
  }
  advanceFilePasteBatch();
  if(filePasteBatch.active&&fileOperationMessage){
    static uint32_t last=0;if(millis()-last<180)return;last=millis();
    Tab5FileOperation::Snapshot state;Tab5FileOperation::snapshot(state);
    String text=String(filePasteBatch.done)+" / "+String(filePasteBatch.paths.size())+" "+T("éléments traités");
    if(Tab5FileOperation::busy())text+=String("\n")+T(Tab5FileOperation::status())+"\n"+String(static_cast<unsigned long>(state.bytesDone/1024))+" / "+String(static_cast<unsigned long>(state.bytesTotal/1024))+" KiB";
    lv_label_set_text(fileOperationMessage,text.c_str());
  }
}
static void pasteClipboardEvent(lv_event_t*) {
  if(Tab5GitView::busy()||Tab5Git::busy()){toast("Une opération Git est déjà en cours.");return;}
  if(Tab5FileOperation::busy()||filePasteBatch.active){toast("Une copie est déjà en cours");return;}
  if((fileClipboardPaths.empty()&&!clipboardPath.length())||!pathStorageReady(fileContextDirectory())||!commitCurrentBeforeLeave())return;
  filePasteBatch=FilePasteBatch{};filePasteBatch.paths=fileClipboardPaths;
  if(filePasteBatch.paths.empty())filePasteBatch.paths.push_back(clipboardPath.c_str());
  filePasteBatch.directory=fileContextDirectory();filePasteBatch.cut=clipboardCut;filePasteBatch.active=true;
  fileTree.cancel();fileListDir.close();fileSearcher.cancel();fileSelectAllDirectory.close();fileSelectAllSearch=false;cancelFolderPreview();freeFilePreviewImage();
  showFileOperationDialog(filePasteBatch.directory);updateFileActionState();
}

static void previewReadEvent(lv_event_t* e) {
  (void)e;
  if (currentPath.length()) startReader(currentPath);
  else toast("Enregistrez le document avant la lecture portrait");
}

static void refreshTimeSettingsUi() {
  if (ntpSwitch) {
    if (settings.ntpEnabled) lv_obj_add_state(ntpSwitch, LV_STATE_CHECKED);
    else lv_obj_remove_state(ntpSwitch, LV_STATE_CHECKED);
  }
  if (timeSetButton) {
    if (settings.ntpEnabled) lv_obj_add_flag(timeSetButton, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(timeSetButton, LV_OBJ_FLAG_HIDDEN);
  }
  if (timeZoneDropdown) {
    lv_dropdown_set_selected(timeZoneDropdown, settings.timeZoneIndex);
    if (settings.ntpEnabled) lv_obj_remove_flag(timeZoneDropdown, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(timeZoneDropdown, LV_OBJ_FLAG_HIDDEN);
  }
  if (rtcValue) {
    const String current = clockString(true);
    setLabelIfChanged(rtcValue, current.c_str());
  }
}

static void clockSetEvent(lv_event_t* e) {
  (void)e;
  if (settings.ntpEnabled) return;
  m5::rtc_datetime_t dt = {};
  String initial = "2026-01-01 12:00";
  if (getSystemDateTime(dt)) {
    char b[32];
    snprintf(b, sizeof(b), "%04u-%02u-%02u %02u:%02u", dt.date.year,
             dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes);
    initial = b;
  }
  openTextDialog(TextDialogAction::SetClock, "Régler date et heure", initial, "");
}

static void ntpEvent(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED || !ntpSwitch) return;
  settings.ntpEnabled = lv_obj_has_state(ntpSwitch, LV_STATE_CHECKED);
  markSettingsDirty();
  refreshTimeSettingsUi();
  if (!settings.ntpEnabled) {
    stopNetworkTime();
    toast("NTP désactivé — horloge RTC");
    return;
  }
  if (Tab5Audio.wifiConnected()) {
    requestNetworkTime();
    toast("Synchronisation NTP demandée");
  } else {
    toast("NTP activé — en attente du Wi-Fi", 2600);
  }
}

static void timeZoneEvent(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED || !timeZoneDropdown)
    return;
  if (!settings.ntpEnabled) return;
  settings.timeZoneIndex = static_cast<uint8_t>(
      lv_dropdown_get_selected(timeZoneDropdown));
  stopNetworkTime();
  applySelectedTimeZone();
  loadSystemTimeFromRtc();
  if (settings.ntpEnabled && Tab5Audio.wifiConnected()) requestNetworkTime();
  markSettingsDirty();
  updateClockLabels(true);
  refreshTimeSettingsUi();
  toast("Fuseau horaire appliqué");
}

static void editorEvent(lv_event_t* e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_VALUE_CHANGED && !loadingDocument && !restoringHistory) {
    documentDirty = true;
    statsPending = true;
    lastEditMs = millis();
    lastHistoryEditMs = lastEditMs;
    historyPending = true;
    updateDocumentTitle();
    updateUndoRedoState();
    updateEditorClipboardState();
  } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESSING) {
    updateEditorClipboardState();
  } else if (code == LV_EVENT_CLICKED) {
    setKeyboardVisible(!writing::usbConnected(), editor);
  } else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
    setKeyboardVisible(false);
  }
}

static void upFolderEvent(lv_event_t*){fileParentNavigation();}

static void refreshFilesEvent(lv_event_t* e) {
  (void)e;
  refreshFileList();
}

static void newDocEvent(lv_event_t* e) {
  (void)e;
  if (!commitCurrentBeforeLeave()) return;
  setKeyboardVisible(false);
  if (Tab5Writing.create(fileContextDirectory().c_str())) showPage(Page::Writing);
}

static void saveAsEvent(lv_event_t* e) {
  (void)e;
  String dir = currentPath.length() ? parentDirOf(currentPath) : currentDir;
  String initial = currentPath.length() ? baseNameOf(currentPath) : "document.txt";
  openTextDialog(TextDialogAction::SaveAs, "Enregistrer sous", initial, dir);
}

static void newFolderEvent(lv_event_t* e) {
  if(Tab5GitView::busy()||Tab5Git::busy()){toast("Une opération Git est déjà en cours.");return;}
  (void)e;
  openTextDialog(TextDialogAction::NewFolder, "Nouveau dossier", "New folder", fileContextDirectory());
}

static void closeFilePopup() {
  auto* old=filePopupOverlay;
  filePopupOverlay = nullptr;
  if(old&&Tab5Focus.contains(old,Tab5Focus.focused()))Tab5Focus.clear();
  if(old&&lv_obj_is_valid(old))lv_obj_delete(old);
}

static void closeFilePopupEvent(lv_event_t* e) {
  (void)e;
  closeFilePopup();
}

static lv_obj_t* createFilePopupBox(int width, int height) {
  closeFilePopup();
  filePopupOverlay = lv_obj_create(root);
  lv_obj_set_size(filePopupOverlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(filePopupOverlay, 0, 0);
  lv_obj_set_style_bg_color(filePopupOverlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(filePopupOverlay, LV_OPA_50, 0);
  lv_obj_set_style_border_width(filePopupOverlay, 0, 0);
  lv_obj_set_style_pad_all(filePopupOverlay, 0, 0);
  lv_obj_remove_flag(filePopupOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(filePopupOverlay);

  lv_obj_t* box = lv_obj_create(filePopupOverlay);
  lv_obj_set_size(box, width, height);
  lv_obj_center(box);
  lv_obj_set_style_bg_color(box, C_SURFACE(), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(box, C_BORDER(), 0);
  lv_obj_set_style_radius(box, 14, 0);
  lv_obj_set_style_pad_all(box, 16, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
  return box;
}

static void createDocumentChoiceEvent(lv_event_t* e) {
  (void)e;
  closeFilePopup();
  newDocEvent(nullptr);
}

static void createFolderChoiceEvent(lv_event_t* e) {
  (void)e;
  closeFilePopup();
  newFolderEvent(nullptr);
}

static void showFileCreateMenu(lv_event_t* e);
static void showCreateMenuEvent(lv_event_t* e) {showFileCreateMenu(e);}

static void showFileInfoEvent(lv_event_t* e) {
  (void)e;
  if (!selectedPath.length()) return;
  lv_obj_t* box = createFilePopupBox(780, 500);

  lv_obj_t* title = lv_label_create(box);
  lv_label_set_text(title, baseNameOf(selectedPath).c_str());
  lv_obj_set_width(title, 680);
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(title, uiHeadingFont(), 0);
  lv_obj_set_pos(title, 6, 2);

  lv_obj_t* close = makeIconButton(box, LV_SYMBOL_CLOSE, 44, 40);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_event_cb(close, closeFilePopupEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* scroll = lv_obj_create(box);
  lv_obj_set_size(scroll, 744, 412);
  lv_obj_align(scroll, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(scroll, C_BG(), 0);
  lv_obj_set_style_bg_opa(scroll, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(scroll, 0, 0);
  lv_obj_set_style_radius(scroll, 8, 0);
  lv_obj_set_style_pad_all(scroll, 20, 0);
  lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_AUTO);

  lv_obj_t* info = lv_label_create(scroll);
  lv_label_set_text(info, fileInfoText.c_str());
  lv_obj_set_width(info, 692);
  lv_label_set_long_mode(info, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(info, C_TEXT(), 0);
  lv_obj_set_style_text_line_space(info, 7, 0);
  lv_obj_align(info, LV_ALIGN_TOP_LEFT, 0, 0);
  Tab5Focus.focusInitial(filePopupOverlay,close);
}

static void deleteDocEvent(lv_event_t* e) {
  if(activePage==Page::Files&&fileSelectionHasVolume())return;
  if(Tab5GitView::busy()||Tab5Git::busy()){toast("Une opération Git est déjà en cours.");return;}
  (void)e;
  if(fileSelectAllDirectory||fileSelectAllSearch||filePasteBatch.active||Tab5FileOperation::busy())return;
  if(!fileSelection.paths.empty())askDeletePath(fileSelection.paths.front().c_str());
  else if (selectedPath.length()) askDeletePath(selectedPath);
  else if (activePage==Page::Editor&&currentPath.length()) askDeletePath(currentPath);
}

static void renameSelectedEvent(lv_event_t* e) {
  if(Tab5GitView::busy()||Tab5Git::busy()){toast("Une opération Git est déjà en cours.");return;}
  (void)e;
  if (!selectedPath.length()||fileSelection.paths.size()>1||fileSelectionHasVolume()) return;
  const String source = selectedPath;
  openTextDialog(TextDialogAction::Rename, "Renommer",
                 baseNameOf(selectedPath), parentDirOf(selectedPath));
  pendingRenamePath = source;
}

static void brightnessEvent(lv_event_t* e) {
  (void)e;
  settings.brightness = static_cast<uint8_t>(lv_slider_get_value(brightnessSlider));
  M5.Display.setBrightness(settings.brightness);
  char b[24];
  snprintf(b, sizeof(b), "%u", settings.brightness);
  lv_label_set_text(brightnessValue, b);
  markSettingsDirty();
}

static void autosaveEvent(lv_event_t* e) {
  (void)e;
  settings.autosave = lv_obj_has_state(autosaveSwitch, LV_STATE_CHECKED);
  markSettingsDirty();
}

static void keyboardFocusEvent(lv_event_t* e) {
  (void)e;
  settings.keyboardOnFocus = lv_obj_has_state(keyboardFocusSwitch, LV_STATE_CHECKED);
  markSettingsDirty();
}

static void autosaveDelayEvent(lv_event_t* e) {
  (void)e;
  settings.autosaveDelaySec = static_cast<uint16_t>(lv_slider_get_value(autosaveDelaySlider));
  char b[32];
  snprintf(b, sizeof(b), "%u s", settings.autosaveDelaySec);
  lv_label_set_text(autosaveDelayValue, b);
  markSettingsDirty();
}

static void fontMinusEvent(lv_event_t* e) {
  (void)e;
  if (!settings.fontIndex)return;
  --settings.fontIndex;
  applyEditorFont();
  markSettingsDirty();
}

static void fontPlusEvent(lv_event_t* e) {
  (void)e;
  if (settings.fontIndex >= 3)return;
  ++settings.fontIndex;
  applyEditorFont();
  markSettingsDirty();
}

static void selectAppliedTheme() {
  if(settings.themeIndex>=THEME_COUNT)settings.themeIndex=0;
  appliedTheme=THEMES[settings.themeIndex];
  snprintf(appliedThemeName,sizeof(appliedThemeName),"%s",appliedTheme.name);
  appliedTheme.name=appliedThemeName;
  uint32_t colors[]={appliedTheme.bg,appliedTheme.surface,appliedTheme.raised,
    appliedTheme.border,appliedTheme.text,appliedTheme.muted,appliedTheme.active,
    appliedTheme.cursor,appliedTheme.selected};
  // Appearance changes geometry; the chosen/custom theme owns every colour.
  Tab5UiStyle::configure(colors,settings.cornerRadius,appliedTheme.switchOff,appliedTheme.switchOn,appliedTheme.switchKnobOff,appliedTheme.switchKnobOn);
}

static void refreshThemeChoices() {
  if(!themeDropdown)return;
  Tab5ThemeChoices::populate(themeDropdown,THEME_COUNT,[](size_t i){return THEMES[i].name;},T);
  lv_dropdown_set_selected(themeDropdown,settings.themeIndex);
}

static void reloadThemeCatalogue(){
  char selectedId[24]={};const bool custom=settings.themeIndex>=5&&settings.themeIndex<THEME_COUNT;
  if(custom)snprintf(selectedId,sizeof(selectedId),"%s",customThemeIds[settings.themeIndex-5]);
  Tab5StorageUse lease;
  if(lease&&loadCustomThemes()&&custom){
    settings.themeIndex=0;
    for(unsigned i=5;i<THEME_COUNT;++i)if(!strcmp(selectedId,customThemeIds[i-5])){settings.themeIndex=i;break;}
  }
  refreshThemeChoices();
}

static void applySelectedTheme() {
  const ThemePalette previous=appliedTheme;
  selectAppliedTheme();
  const uint32_t before[]={previous.bg,previous.surface,previous.raised,
    previous.border,previous.text,previous.muted,previous.active,previous.cursor,
    previous.selected};
  const uint32_t after[]={appliedTheme.bg,appliedTheme.surface,appliedTheme.raised,
    appliedTheme.border,appliedTheme.text,appliedTheme.muted,appliedTheme.active,
    appliedTheme.cursor,appliedTheme.selected};
#if LV_USE_THEME_MONO
  auto* base=lv_theme_mono_init(lvDisplay,appliedTheme.bg<0x808080,uiFont());
  if(base)lv_display_set_theme(lvDisplay,base);
#endif
  Tab5ThemeLive::apply(root,before,after);
  Tab5ThemeLive::apply(lv_layer_top(),before,after);
  Tab5ThemeLive::apply(lv_layer_sys(),before,after);
  Tab5UiStyle::tree(root);
  Tab5UiStyle::tree(lv_layer_top());
  Tab5Writing.refreshAppearance();
  Tab5Focus.theme(C_SELECTED(),C_TEXT(),C_SURFACE());
  fileRefreshAppearance();
  Tab5Mouse::palette(appliedTheme.pointer,appliedTheme.pointerOutline);
  markSettingsDirty();
}

static void applyUiCorners(){
  const uint32_t colors[]={appliedTheme.bg,appliedTheme.surface,appliedTheme.raised,
    appliedTheme.border,appliedTheme.text,appliedTheme.muted,appliedTheme.active,
    appliedTheme.cursor,appliedTheme.selected};
  Tab5UiStyle::configure(colors,settings.cornerRadius,appliedTheme.switchOff,appliedTheme.switchOn,appliedTheme.switchKnobOff,appliedTheme.switchKnobOn);
  Tab5UiStyle::tree(root);Tab5UiStyle::tree(lv_layer_top());
  markSettingsDirty();
}

static void themeChoiceEvent(lv_event_t* e) {
  if(lv_event_get_code(e)!=LV_EVENT_VALUE_CHANGED||!themeDropdown)return;
  const unsigned selected=lv_dropdown_get_selected(themeDropdown);
  if(selected>=THEME_COUNT||selected==settings.themeIndex)return;
  settings.themeIndex=uint8_t(selected);
  applySelectedTheme();
}

static void languageEvent(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED || !languageDropdown)
    return;
  const size_t index = lv_dropdown_get_selected(languageDropdown);
  if (!Tab5Locale.select(index)) {
    toast("Pack de langue invalide", 2400);
    return;
  }
  if(keyboardPanel)lv_obj_clean(keyboardPanel);
  Tab5Keyboards.selectForLanguage(Tab5Locale.selectedId());
  Tab5Cjk.select(Tab5Locale.selectedId(),Tab5Keyboards.selectedId());
  Tab5Locale.applyFontAvailability(smoothFontReady && Tab5Cjk.availableFor(Tab5Locale.selectedId()));
  imeSync(keyboardTarget);rebuildKeyboard();
  writing::usbFrench(Tab5Keyboards.selectedId().startsWith("fr"));
  writing::usbMapping(Tab5Keyboards.physical().data(),Tab5Keyboards.physical().size());
  if(keyboardLayoutDropdown)lv_dropdown_set_selected(keyboardLayoutDropdown,Tab5Keyboards.selectedIndex());
  Tab5Writing.refreshAppearance();
  toast("Langue enregistrée — redémarrez pour tout appliquer", 3000);
}

static void keyboardLayoutEvent(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED ||
      !keyboardLayoutDropdown) return;
  // Les boutons gardent un pointeur vers leur définition. Ils sont donc
  // détruits avant le remplacement atomique de la disposition active.
  if (keyboardPanel) lv_obj_clean(keyboardPanel);
  const size_t index = lv_dropdown_get_selected(keyboardLayoutDropdown);
  if (!Tab5Keyboards.select(index)) {
    rebuildKeyboard();
    toast("Disposition du clavier invalide", 2400);
    return;
  }
  keyboardMode = KeyboardMode::Lower;
  Tab5Cjk.select(Tab5Locale.selectedId(),Tab5Keyboards.selectedId());
  imeSync(keyboardTarget);
  rebuildKeyboard();
  writing::usbFrench(Tab5Keyboards.selectedId().startsWith("fr"));
  writing::usbMapping(Tab5Keyboards.physical().data(),Tab5Keyboards.physical().size());
  toast("Disposition du clavier appliquée");
}

static void documentsRootEvent(lv_event_t* e) {
  if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
  Tab5FilePicker::open(settings.documentsRoot.c_str(),"",true,[](const char* path,void*){
    if(!isExistingDirectory(path))return;
    settings.documentsRoot=path;currentDir=settings.documentsRoot;
    if(documentsRootButton)lv_label_set_text(lv_obj_get_child(documentsRootButton,0),settings.documentsRoot.c_str());
    if(activePage==Page::Files)refreshFileList();markSettingsDirty();toast("Dossier de départ enregistré");
  },nullptr);
}

// -----------------------------------------------------------------------------
// Clavier tactile chargé depuis /System/keyboards.json.
// -----------------------------------------------------------------------------

static const char* keyDisplay(const Tab5KeyDef& key) {
  const char* k = key.value.c_str();
  // Touches de fonction en vrais symboles LVGL, comme avant.
  // Elles sont rendues avec la Montserrat intégrée (voir createKey), pas avec
  // la TTF du document, afin que les glyphes privés LVGL soient toujours présents.
  if (!strcmp(k, "<BKSP>")) return LV_SYMBOL_BACKSPACE;
  if (!strcmp(k, "<ENTER>")) return LV_SYMBOL_NEW_LINE;
  if (!strcmp(k, "<SHIFT>")) return LV_SYMBOL_UP;
  if (!strcmp(k, "<SYM>")) return "123";
  if (!strcmp(k, "<ABC>")) return "ABC";
  if (!strcmp(k, "<LEFT>")) return LV_SYMBOL_LEFT;
  if (!strcmp(k, "<RIGHT>")) return LV_SYMBOL_RIGHT;
  if (!strcmp(k, " ")) return "";
  return key.label.c_str();
}

static void keyInsertText(const char* text) {
  if (!keyboardTarget || !lv_obj_is_valid(keyboardTarget) || !text) return;
  if (Tab5Writing.isInput(keyboardTarget)) Tab5Writing.insert(text);
  else if(Tab5Tools.isInput(keyboardTarget))Tab5Tools.insert(text);
  else Tab5TextInput::insert(keyboardTarget, text);
}

#include "Tab5ImeUi.inc"

static void accentChoiceEvent(lv_event_t* e) {
  const char* text = static_cast<const char*>(lv_event_get_user_data(e));
  if (text) keyInsertText(text);
  if (accentPopup && lv_obj_is_valid(accentPopup)) lv_obj_delete(accentPopup);
  accentPopup = nullptr;
}

static void showAccentPopup(lv_obj_t* keyObj, const char* set) {
  if (!set || !keyObj) return;
  if (accentPopup && lv_obj_is_valid(accentPopup)) lv_obj_delete(accentPopup);

  accentPopup = lv_obj_create(root);
  lv_obj_set_height(accentPopup, 64);
  lv_obj_set_style_pad_all(accentPopup, 3, 0);
  lv_obj_set_style_pad_column(accentPopup, 2, 0);
  lv_obj_set_style_bg_color(accentPopup, C_SURFACE(), 0);
  lv_obj_set_style_border_color(accentPopup, C_BORDER(), 0);
  lv_obj_set_flex_flow(accentPopup, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(accentPopup, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(accentPopup, LV_OBJ_FLAG_SCROLLABLE);

  String options(set);
  int count = 1;
  for (size_t i=0;i<options.length();++i) if (options[i]=='|') ++count;
  const int popupW = count * 62 + 10;
  lv_obj_set_width(accentPopup, popupW);

  lv_area_t a;
  lv_obj_get_coords(keyObj, &a);
  int x = (a.x1 + a.x2) / 2 - popupW / 2;
  x = constrain(x, 4, SCREEN_W - popupW - 4);
  const int32_t popupY = static_cast<int32_t>(a.y1) - 68;
  const int y = static_cast<int>(popupY > (TOP_H + 4) ? popupY : (TOP_H + 4));
  lv_obj_set_pos(accentPopup, x, y);

  int start = 0;
  while (start <= static_cast<int>(options.length())) {
    int sep = options.indexOf('|', start);
    if (sep < 0) sep = options.length();
    String opt = options.substring(start, sep);
    char* stable = static_cast<char*>(lv_malloc(opt.length()+1));
    if (stable) {
      strcpy(stable, opt.c_str());
      lv_obj_t* b = makeButton(accentPopup, stable, 56, 54);
      lv_obj_set_style_text_font(lv_obj_get_child(b,0), keyFont(), 0);
      lv_obj_add_event_cb(b, accentChoiceEvent, LV_EVENT_PRESSED, stable);
      lv_obj_add_event_cb(b, [](lv_event_t* ev){ void* p=lv_event_get_user_data(ev); if(p) lv_free(p); }, LV_EVENT_DELETE, stable);
    }
    if (sep >= static_cast<int>(options.length())) break;
    start = sep + 1;
  }
  lv_obj_move_foreground(accentPopup);
}

static void keyLongEvent(lv_event_t* e) {
  const auto* key = static_cast<const Tab5KeyDef*>(lv_event_get_user_data(e));
  if (!key || !key->accents.length() || !keyboardTarget) return;
  // The normal character was inserted on PRESS. Remove it and offer variants.
  if (Tab5Writing.isInput(keyboardTarget)) Tab5Writing.touchKey(writing::Key::Backspace);
  else if(Tab5Tools.isInput(keyboardTarget))Tab5Tools.touchKey(writing::Key::Backspace);
  else { writing::KeyEvent e{};e.key=writing::Key::Backspace;Tab5Focus.edit(keyboardTarget,e); }
  showAccentPopup(lv_event_get_target_obj(e), key->accents.c_str());
}

static void keyPressedEvent(lv_event_t* e) {
  const auto* key = static_cast<const Tab5KeyDef*>(lv_event_get_user_data(e));
  const char* k = key ? key->value.c_str() : nullptr;
  if (!k || !keyboardTarget || !lv_obj_is_valid(keyboardTarget)) return;
  if (imeTouch(k)) return;

  if (!strcmp(k, "<BKSP>")) { if(Tab5Writing.isInput(keyboardTarget))Tab5Writing.touchKey(writing::Key::Backspace);else if(Tab5Tools.isInput(keyboardTarget))Tab5Tools.touchKey(writing::Key::Backspace);else { writing::KeyEvent e{};e.key=writing::Key::Backspace;Tab5Focus.edit(keyboardTarget,e); } }
  else if (!strcmp(k, "<ENTER>")) {
    if(Tab5Writing.isInput(keyboardTarget))Tab5Writing.touchKey(writing::Key::Enter);else if(Tab5Tools.isInput(keyboardTarget))Tab5Tools.touchKey(writing::Key::Enter);
    else if (!lv_textarea_get_one_line(keyboardTarget)) lv_textarea_add_char(keyboardTarget, '\n');
    else { lv_obj_send_event(keyboardTarget, LV_EVENT_READY, nullptr); setKeyboardVisible(false); }
  } else if (!strcmp(k, "<SHIFT>")) {
    keyboardMode = (keyboardMode == KeyboardMode::Upper) ? KeyboardMode::Lower : KeyboardMode::Upper;
    rebuildKeyboard();
  } else if (!strcmp(k, "<SYM>")) {
    keyboardMode = KeyboardMode::Symbols;
    rebuildKeyboard();
  } else if (!strcmp(k, "<ABC>")) {
    keyboardMode = KeyboardMode::Lower;
    rebuildKeyboard();
  } else if (!strcmp(k, "<LEFT>")) {
    if(Tab5Writing.isInput(keyboardTarget)){Tab5Writing.touchKey(writing::Key::Left);return;}
    if(Tab5Tools.isInput(keyboardTarget)){Tab5Tools.touchKey(writing::Key::Left);return;}
    int32_t pos = lv_textarea_get_cursor_pos(keyboardTarget);
    if (pos > 0) lv_textarea_set_cursor_pos(keyboardTarget, pos - 1);
  } else if (!strcmp(k, "<RIGHT>")) {
    if(Tab5Writing.isInput(keyboardTarget)){Tab5Writing.touchKey(writing::Key::Right);return;}
    if(Tab5Tools.isInput(keyboardTarget)){Tab5Tools.touchKey(writing::Key::Right);return;}
    int32_t pos = lv_textarea_get_cursor_pos(keyboardTarget);
    lv_textarea_set_cursor_pos(keyboardTarget, pos + 1);
  } else keyInsertText(k);
}

static lv_obj_t* createKey(lv_obj_t* row, const Tab5KeyDef& kd) {
  lv_obj_t* b = lv_button_create(row);
  lv_obj_set_height(b, LV_PCT(100));
  lv_obj_set_width(b, 0);
  lv_obj_set_flex_grow(b, kd.grow);
  lv_obj_set_style_radius(b, 5, 0);
  lv_obj_set_style_bg_color(b, C_RAISED(), 0);
  lv_obj_set_style_bg_color(b, C_ACTIVE(), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_border_color(b, C_BORDER(), 0);
  lv_obj_set_ext_click_area(b, 1);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_outline_width(b, 0, 0);
  lv_obj_add_event_cb(b, keyPressedEvent, LV_EVENT_PRESSED,
                      const_cast<Tab5KeyDef*>(&kd));
  if (kd.accents.length())
    lv_obj_add_event_cb(b, keyLongEvent, LV_EVENT_LONG_PRESSED,
                        const_cast<Tab5KeyDef*>(&kd));

  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, keyDisplay(kd));
  lv_obj_set_style_text_color(l, C_TEXT(), 0);
  const bool controlKey = kd.value.startsWith("<");
  const bool shiftKey = kd.value == "<SHIFT>";
  lv_obj_set_style_text_font(l, controlKey ? &lv_font_montserrat_28 : keyFont(), 0);
  if (shiftKey && keyboardMode == KeyboardMode::Upper) {
    lv_obj_set_style_bg_color(b, C_SELECTED(), 0);
    lv_obj_set_style_border_color(b, C_MUTED(), 0);
  }
  lv_obj_center(l);
  return b;
}

static void addKeyboardRow(const std::vector<Tab5KeyDef>& definitions) {
  if (definitions.empty()) return;
  lv_obj_t* row = lv_obj_create(keyboardPanel);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_flex_grow(row, 1);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 1, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  for (const Tab5KeyDef& key : definitions) createKey(row, key);
}

static void rebuildKeyboard() {
  if (!keyboardPanel) return;
  if (accentPopup && lv_obj_is_valid(accentPopup)) lv_obj_delete(accentPopup);
  accentPopup = nullptr;
  lv_obj_clean(keyboardPanel);
  imeSync(keyboardTarget);
  lv_obj_set_height(keyboardPanel,keyboardHeight());
  if(imeComposer.mode()!=tab5ime::Mode::None)imeMount(keyboardPanel);
  auto* bar = lv_obj_create(keyboardPanel);
  lv_obj_set_size(bar, LV_PCT(100), 36);
  lv_obj_set_style_pad_all(bar, 0, 0);lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  auto* hide = makeIconButton(bar, LV_SYMBOL_DOWN, 72, 34);
  lv_obj_align(hide, LV_ALIGN_RIGHT_MID, -4, 0);
  lv_obj_add_event_cb(hide, [](lv_event_t*) { setKeyboardVisible(false); }, LV_EVENT_CLICKED, nullptr);
  const uint8_t layer = keyboardMode == KeyboardMode::Lower ? 0
      : (keyboardMode == KeyboardMode::Upper ? 1 : 2);
  for (uint8_t row = 0; row < 4; ++row)
    addKeyboardRow(keyboardNumberMode?Tab5NumericKeyboard::row(keyboardNumberMode,row):Tab5Keyboards.row(layer, row));
  if(keyboardVisible)setKeyboardVisible(true);
}

// -----------------------------------------------------------------------------
// Portrait reader: TXT / Markdown / EPUB
// EPUB is paginated: one complete screen = one page, tap left/right to turn.
// Plain text keeps continuous vertical scrolling, with a larger TinyTTF cache.
// -----------------------------------------------------------------------------

static void freeReaderImage() {
  if (readerImageObj && lv_obj_is_valid(readerImageObj)) {
    lv_image_set_src(readerImageObj, nullptr);
    lv_obj_add_flag(readerImageObj, LV_OBJ_FLAG_HIDDEN);
  }
  if (readerImageCanvas) {
    readerImageCanvas->deleteSprite();
    delete readerImageCanvas;
    readerImageCanvas = nullptr;
  }
  memset(&readerImageDsc, 0, sizeof(readerImageDsc));
}

static void freeReaderText() {
  if (readerTextLabel && lv_obj_is_valid(readerTextLabel)) lv_label_set_text(readerTextLabel, "");
  freeReaderImage();
  if (readerText) heap_caps_free(readerText);
  readerText = nullptr;
  readerTextLen = 0;
  readerPageStartCount = 0;
  readerPageIndex = 0;
  readerChapterPaginationComplete = false;
}

static bool loadPlainReaderText(const String& path, String& err) {
  File f = tab5FS.open(path.c_str(), FILE_READ);
  if (!f || f.isDirectory()) { if(f) f.close(); err=T("Document inaccessible"); return false; }
  const size_t size = f.size();
  if (size > MAX_READER_TEXT_BYTES) { f.close(); err=T("Document > 3 Mio"); return false; }
  char* buf = static_cast<char*>(heap_caps_malloc(size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!buf) { f.close(); err=T("Mémoire insuffisante"); return false; }
  const size_t got = f.read(reinterpret_cast<uint8_t*>(buf), size);
  f.close();
  buf[got] = 0;
  readerText = buf; readerTextLen = got;
  return true;
}

static size_t readerSkipPageWhitespace(size_t p) {
  while (p < readerTextLen) {
    const unsigned char c = static_cast<unsigned char>(readerText[p]);
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') ++p;
    else break;
  }
  return p;
}

static bool readerImageMarkerAt(size_t rawStart, String* pathOut, size_t* markerEndOut) {
  if (!readerText) return false;
  size_t p = readerSkipPageWhitespace(rawStart);
  if (p + 6 >= readerTextLen || static_cast<unsigned char>(readerText[p]) != 0x1E) return false;
  if (memcmp(readerText + p + 1, "IMG:", 4) != 0) return false;
  size_t e = p + 5;
  while (e < readerTextLen && static_cast<unsigned char>(readerText[e]) != 0x1F) ++e;
  if (e >= readerTextLen) return false;
  if (pathOut) *pathOut = String(readerText + p + 5).substring(0, e - (p + 5));
  if (markerEndOut) *markerEndOut = readerSkipPageWhitespace(e + 1);
  return true;
}

static size_t readerNextImageMarker(size_t start) {
  if (!readerText) return readerTextLen;
  for (size_t p = start; p < readerTextLen; ++p) {
    if (static_cast<unsigned char>(readerText[p]) == 0x1E && p + 5 < readerTextLen &&
        memcmp(readerText + p + 1, "IMG:", 4) == 0) return p;
  }
  return readerTextLen;
}

static size_t utf8BoundaryBack(size_t p, size_t floor) {
  if (!readerText) return p;
  while (p > floor && p < readerTextLen &&
         (static_cast<unsigned char>(readerText[p]) & 0xC0) == 0x80) --p;
  return p;
}

static int32_t readerTextHeight(size_t start, size_t end) {
  if (!readerText || end <= start) return 0;
  end = utf8BoundaryBack(end, start);
  char saved = readerText[end];
  readerText[end] = 0;
  lv_point_t sz = {0, 0};
  int32_t width = readerContent ? lv_obj_get_content_width(readerContent) : 0;
  if (width < 100) width = lv_display_get_horizontal_resolution(lvDisplay) - 52;
  lv_text_get_size(&sz, readerText + start, readerFont(), 0, 8, width, LV_TEXT_FLAG_NONE);
  readerText[end] = saved;
  return sz.y;
}

static inline bool readerPageSpace(char c) {
  return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

// Return the end of the next complete word, expressed as a byte offset.  The
// returned position points at the whitespace after the word (or hardEnd).
static size_t readerNextWordBoundary(size_t from, size_t hardEnd) {
  size_t p = from;
  while (p < hardEnd && readerPageSpace(readerText[p])) ++p;
  while (p < hardEnd && !readerPageSpace(readerText[p])) ++p;
  return utf8BoundaryBack(p, from);
}

// Find the nearest complete-word boundary at or before p. Unlike the old code,
// this does not blindly discard the last word after a byte-based binary search.
static size_t readerPrevWordBoundary(size_t p, size_t start) {
  if (p <= start) return start;
  p = utf8BoundaryBack(p, start);
  if (p > start && p < readerTextLen && readerPageSpace(readerText[p])) return p;
  while (p > start && !readerPageSpace(readerText[p])) --p;
  return p;
}

static size_t readerFindTextPageEnd(size_t rawStart) {
  const size_t start = readerSkipPageWhitespace(rawStart);
  if (start >= readerTextLen) return readerTextLen;

  // Images always begin a new page. Text is never measured across a marker.
  const size_t marker = readerNextImageMarker(start);
  const size_t hardEnd = marker < readerTextLen ? marker : readerTextLen;
  if (hardEnd <= start) return hardEnd;

  int32_t maxH = readerContent ? lv_obj_get_content_height(readerContent) : 0;
  if (maxH < 100) maxH = lv_display_get_vertical_resolution(lvDisplay) - 138;

  // First locate an approximate byte position close to the bottom of the page.
  size_t hi = start + 3072;
  if (hi > hardEnd) hi = hardEnd;
  hi = utf8BoundaryBack(hi, start);
  if (hi <= start) hi = hardEnd;

  size_t fit = start;
  while (true) {
    if (readerTextHeight(start, hi) <= maxH) {
      fit = hi;
      if (hi >= hardEnd) return hardEnd;
      size_t next = hi + 2048;
      if (next > hardEnd) next = hardEnd;
      next = utf8BoundaryBack(next, hi);
      if (next <= hi) next = hardEnd;
      hi = next;
    } else break;
  }

  size_t lo = fit > start ? fit : start + 1;
  size_t upper = hi;
  while (upper > lo + 8) {
    size_t mid = utf8BoundaryBack(lo + (upper - lo) / 2, start);
    if (mid <= start) mid = start + 1;
    if (readerTextHeight(start, mid) <= maxH) {
      fit = mid;
      lo = mid + 1;
    } else {
      upper = mid;
    }
  }

  if (fit >= hardEnd) return hardEnd;

  // The byte search above can stop in the middle of a word. The previous
  // version simply moved that whole word to the next page. Here we snap to a
  // complete-word boundary, then greedily test the NEXT complete word(s). This
  // keeps a word on the current page whenever the fully rendered word fits.
  size_t pageEnd = readerPrevWordBoundary(fit, start);
  if (pageEnd <= start) {
    // A single very long word: retain LVGL's natural wrapping rather than
    // creating an empty page.
    pageEnd = fit > start ? fit : readerNextWordBoundary(start, hardEnd);
  }

  // Safety: if the snapped boundary somehow does not fit, back up one word at
  // a time. This is rare but protects against font metric edge cases.
  while (pageEnd > start && readerTextHeight(start, pageEnd) > maxH) {
    size_t p = pageEnd;
    while (p > start && readerPageSpace(readerText[p - 1])) --p;
    while (p > start && !readerPageSpace(readerText[p - 1])) --p;
    if (p <= start || p == pageEnd) break;
    pageEnd = p;
  }

  // Now advance on REAL word boundaries while the complete next word still
  // fits. This is the important fix for the "one word unnecessarily moved to
  // the next page" behaviour.
  for (uint16_t guard = 0; guard < 96 && pageEnd < hardEnd; ++guard) {
    const size_t candidate = readerNextWordBoundary(pageEnd, hardEnd);
    if (candidate <= pageEnd) break;
    if (readerTextHeight(start, candidate) <= maxH) pageEnd = candidate;
    else break;
  }

  if (pageEnd <= start) pageEnd = fit > start ? fit : hardEnd;
  return pageEnd > hardEnd ? hardEnd : pageEnd;
}

static bool readerResetPagination() {
  readerPageStartCount = 0;
  if (!readerPageStarts.ensure(1)) return false;
  readerPageIndex = 0;
  readerPageStartCount = 1;
  readerPageStarts[0] = static_cast<uint32_t>(readerSkipPageWhitespace(0));
  readerChapterPaginationComplete = (readerPageStarts[0] >= readerTextLen);
  return true;
}

static bool readerEnsureNextPageStart(uint16_t page) {
  if (!readerText || page >= READER_PAGE_CACHE_MAX) return false;
  if (page + 1 < readerPageStartCount) return true;
  if (page >= readerPageStartCount) return false;

  size_t start = readerPageStarts[page];
  size_t end = start;
  String imagePath;
  if (readerImageMarkerAt(start, &imagePath, &end)) {
    // One image marker occupies one logical page.
  } else {
    end = readerFindTextPageEnd(start);
    end = readerSkipPageWhitespace(end);
  }
  if (end < start) end = start;
  if (end > readerTextLen) end = readerTextLen;
  if (!readerPageStarts.ensure(readerPageStartCount + 1U)) return false;
  readerPageStarts[readerPageStartCount++] = static_cast<uint32_t>(end);
  if (end >= readerTextLen) readerChapterPaginationComplete = true;
  return true;
}

static bool imageNaturalSize(const uint8_t* data, size_t len, int& w, int& h) {
  w = h = 0;
  if (!data || len < 24) return false;
  // PNG
  if (len >= 24 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
    w = (static_cast<int>(data[16]) << 24) | (static_cast<int>(data[17]) << 16) |
        (static_cast<int>(data[18]) << 8) | data[19];
    h = (static_cast<int>(data[20]) << 24) | (static_cast<int>(data[21]) << 16) |
        (static_cast<int>(data[22]) << 8) | data[23];
    return w > 0 && h > 0;
  }
  // BMP
  if (len >= 26 && data[0] == 'B' && data[1] == 'M') {
    w = static_cast<int>(data[18] | (data[19] << 8) | (data[20] << 16) | (data[21] << 24));
    h = static_cast<int>(data[22] | (data[23] << 8) | (data[24] << 16) | (data[25] << 24));
    if (h < 0) h = -h;
    return w > 0 && h > 0;
  }
  // JPEG SOF markers.
  if (data[0] == 0xFF && data[1] == 0xD8) {
    size_t p = 2;
    while (p + 9 < len) {
      while (p < len && data[p] != 0xFF) ++p;
      while (p < len && data[p] == 0xFF) ++p;
      if (p >= len) break;
      uint8_t marker = data[p++];
      if (marker == 0xD8 || marker == 0xD9 || (marker >= 0xD0 && marker <= 0xD7)) continue;
      if (p + 1 >= len) break;
      uint16_t seg = static_cast<uint16_t>((data[p] << 8) | data[p + 1]);
      if (seg < 2 || p + seg > len) break;
      const bool sof = (marker >= 0xC0 && marker <= 0xC3) || (marker >= 0xC5 && marker <= 0xC7) ||
                       (marker >= 0xC9 && marker <= 0xCB) || (marker >= 0xCD && marker <= 0xCF);
      if (sof && seg >= 7) {
        h = (data[p + 3] << 8) | data[p + 4];
        w = (data[p + 5] << 8) | data[p + 6];
        return w > 0 && h > 0;
      }
      p += seg;
    }
  }
  return false;
}

static bool sniffImageFile(File& f, int& w, int& h) {
  w = h = 0;
  if (!f || f.isDirectory()) return false;
  const size_t oldPos = static_cast<size_t>(f.position());
  if (!f.seek(0)) return false;
  const size_t want = min(static_cast<size_t>(f.size()), IMAGE_SNIFF_BYTES);
  uint8_t* sample = static_cast<uint8_t*>(
      heap_caps_malloc(want ? want : 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!sample) sample = static_cast<uint8_t*>(malloc(want ? want : 1));
  if (!sample) { f.seek(oldPos); return false; }
  const size_t got = want ? f.read(sample, want) : 0;
  const bool image = imageNaturalSize(sample, got, w, h);
  free(sample);
  f.seek(oldPos);
  return image;
}

static bool isReadableImageFile(const String& path) {
  if (!pathStorageReady(path) || isEpubExtension(path)) return false;
  File f = tab5FS.open(path.c_str(), FILE_READ);
  if (!f || f.isDirectory()) { if (f) f.close(); return false; }
  int w = 0, h = 0;
  uint8_t header[12]{};const size_t got=f.read(header,sizeof(header));f.seek(0);
  const auto kind=Tab5ImageFormats::sniff(header,got);
  const bool native=kind==Tab5ImageFormats::Kind::Project||kind==Tab5ImageFormats::Kind::Webp;
  const bool image = native||sniffImageFile(f, w, h);
  f.close();
  return image;
}

#include "Tab5ExplorerImages.inc"

static bool loadSdImageCanvas(const String& path, int maxW, int maxH,
                              M5Canvas*& canvas, lv_image_dsc_t& dsc,
                              int& naturalW, int& naturalH, String& err) {
  if (canvas) {
    canvas->deleteSprite();
    delete canvas;
    canvas = nullptr;
  }
  memset(&dsc, 0, sizeof(dsc));
  naturalW = naturalH = 0;
  err = T("");
  File f = tab5FS.open(path.c_str(), FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    err = T("Image inaccessible");
    return false;
  }
  uint8_t nativeHeader[12]{};const size_t headerBytes=f.read(nativeHeader,sizeof(nativeHeader));f.seek(0);
  const auto nativeKind=Tab5ImageFormats::sniff(nativeHeader,headerBytes);
  if(nativeKind==Tab5ImageFormats::Kind::Project||nativeKind==Tab5ImageFormats::Kind::Webp){
    const bool loaded=nativeKind==Tab5ImageFormats::Kind::Project?
        loadProjectImageCanvas(f,maxW,maxH,canvas,dsc,naturalW,naturalH,err):
        loadWebpImageCanvas(f,maxW,maxH,canvas,dsc,naturalW,naturalH,err);
    f.close();return loaded;
  }
  const size_t len = f.size();
  if (len == 0 || len > MAX_IMAGE_FILE_BYTES) {
    f.close();
    err = len == 0 ? "Fichier vide" : "Image > 12 Mio";
    return false;
  }

  uint8_t* data = static_cast<uint8_t*>(
      heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!data) {
    f.close();
    err = T("Mémoire insuffisante");
    return false;
  }
  const size_t got = f.read(data, len);
  f.close();
  if (got != len || !imageNaturalSize(data, len, naturalW, naturalH)) {
    heap_caps_free(data);
    err = T("JPEG, PNG ou BMP invalide");
    return false;
  }

  float scale = 1.0f;
  if (maxW > 0 && naturalW > maxW) scale = static_cast<float>(maxW) / naturalW;
  if (maxH > 0 && naturalH * scale > maxH)
    scale = static_cast<float>(maxH) / naturalH;
  const double naturalBytes = static_cast<double>(naturalW) * naturalH * 2.0;
  size_t canvasBudget = MAX_IMAGE_CANVAS_BYTES;
  const size_t largestPsram =
      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  const size_t reserve = 512U * 1024U;
  if (largestPsram <= reserve) canvasBudget = 0;
  else if (largestPsram - reserve < canvasBudget)
    canvasBudget = largestPsram - reserve;
  if (canvasBudget < 128U * 1024U) {
    heap_caps_free(data);
    err = T("PSRAM trop fragmentée pour l’image");
    return false;
  }
  if (naturalBytes * scale * scale > canvasBudget) {
    const float memoryScale = sqrtf(static_cast<float>(canvasBudget / naturalBytes));
    if (memoryScale < scale) scale = memoryScale;
  }
  if (scale <= 0.0f) scale = 0.01f;
  const int outW = max(1, static_cast<int>(naturalW * scale + 0.5f));
  const int outH = max(1, static_cast<int>(naturalH * scale + 0.5f));

  canvas = new M5Canvas(&M5.Display);
  if (!canvas) {
    heap_caps_free(data);
    err = T("Mémoire insuffisante");
    return false;
  }
  canvas->setPsram(true);
  canvas->setColorDepth(16);
  if (!canvas->createSprite(outW, outH)) {
    delete canvas;
    canvas = nullptr;
    heap_caps_free(data);
    err = T("Canvas image indisponible");
    return false;
  }
  canvas->setSwapBytes(false);
  canvas->fillSprite(TFT_BLACK);

  Tab5ImageLock imageLock(3000);
  bool supported = true;
  if (!imageLock) supported=false;
  else if (data[0] == 0xFF && len > 1 && data[1] == 0xD8) {
    supported=canvas->drawJpg(data, len, 0, 0, outW, outH, 0, 0,
                    scale, scale, datum_t::top_left);
  } else if (len > 8 && data[0] == 0x89 && data[1] == 'P' &&
             data[2] == 'N' && data[3] == 'G') {
    supported=canvas->drawPng(data, len, 0, 0, outW, outH, 0, 0,
                    scale, scale, datum_t::top_left);
  } else if (len > 2 && data[0] == 'B' && data[1] == 'M') {
    supported=canvas->drawBmp(data, len, 0, 0, outW, outH, 0, 0,
                    scale, scale, datum_t::top_left);
  } else {
    supported = false;
  }
  heap_caps_free(data);
  if (!supported) {
    canvas->deleteSprite();
    delete canvas;
    canvas = nullptr;
    err = T("Format image non pris en charge");
    return false;
  }

  memset(&dsc, 0, sizeof(dsc));
  dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  dsc.header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
  dsc.header.flags = 0;
  dsc.header.w = outW;
  dsc.header.h = outH;
  dsc.header.stride = outW * 2;
  dsc.data_size = static_cast<uint32_t>(outW * outH * 2);
  dsc.data = static_cast<const uint8_t*>(canvas->getBuffer());
  return true;
}

static void freeFilePreviewImage() {
  if (filePreviewImageCanvas && filePreviewImage && lv_obj_is_valid(filePreviewImage)) {
    lv_image_set_src(filePreviewImage, nullptr);
    lv_obj_add_flag(filePreviewImage, LV_OBJ_FLAG_HIDDEN);
  }
  lv_image_cache_drop(&filePreviewImageDsc);
  if (filePreviewImageCanvas) {
    filePreviewImageCanvas->deleteSprite();
    delete filePreviewImageCanvas;
    filePreviewImageCanvas = nullptr;
  }
  memset(&filePreviewImageDsc, 0, sizeof(filePreviewImageDsc));
  filePreviewImagePath="";
}

static void showFilePreviewImage() {
  if(!selectedPath.length()||selectedIsDir||!filePreviewImage||!filePreviewScroll||
     !lv_obj_is_valid(filePreviewImage)||!(isImageProject(selectedPath)||isReadableImageFile(selectedPath)))return;
  // Repeated taps keep the same preview and never launch the full viewer.
  if(filePreviewImageCanvas&&filePreviewImagePath==selectedPath)return;
  freeFilePreviewImage();
  cancelFolderPreview();
  lv_obj_update_layout(filePreviewScroll);
  const int width=lv_obj_get_content_width(filePreviewScroll);
  const int height=lv_obj_get_content_height(filePreviewScroll);
  if(width<=0||height<=0)return;
  String error;int naturalW=0,naturalH=0;
  if(!loadSdImageCanvas(selectedPath,width,height,filePreviewImageCanvas,
                        filePreviewImageDsc,naturalW,naturalH,error)){
    lv_obj_remove_flag(filePreviewLabel,LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(filePreviewLabel,T(error.c_str()));return;
  }
  lv_obj_add_flag(filePreviewLabel,LV_OBJ_FLAG_HIDDEN);
  filePreviewImagePath=selectedPath;
  lv_image_set_src(filePreviewImage,&filePreviewImageDsc);
  lv_obj_remove_flag(filePreviewImage,LV_OBJ_FLAG_HIDDEN);
  lv_obj_center(filePreviewImage);
  lv_obj_scroll_to_x(filePreviewScroll,0,LV_ANIM_OFF);
  lv_obj_scroll_to_y(filePreviewScroll,0,LV_ANIM_OFF);
}

static void freeImageViewerCanvas() {
  if (imageViewerObject && lv_obj_is_valid(imageViewerObject))
    lv_image_set_src(imageViewerObject, nullptr);
  lv_image_cache_drop(&imageViewerDsc);
  if (imageViewerCanvas) {
    imageViewerCanvas->deleteSprite();
    delete imageViewerCanvas;
    imageViewerCanvas = nullptr;
  }
  memset(&imageViewerDsc, 0, sizeof(imageViewerDsc));
  imageViewerBaseW = imageViewerBaseH = 0;
}

static void applyImageViewerScale(uint16_t scale) {
  if (!imageViewerViewport || !imageViewerContent || !imageViewerObject ||
      !imageViewerBaseW || !imageViewerBaseH) return;
  if (scale < 32) scale = 32;
  if (scale > 1024) scale = 1024;
  imageViewerScale = scale;

  lv_obj_update_layout(imageViewerScreen);
  const int viewW = lv_obj_get_content_width(imageViewerViewport);
  const int viewH = lv_obj_get_content_height(imageViewerViewport);
  const int scaledW = max(1, static_cast<int>((static_cast<int64_t>(imageViewerBaseW) * scale + 128) / 256));
  const int scaledH = max(1, static_cast<int>((static_cast<int64_t>(imageViewerBaseH) * scale + 128) / 256));
  const int contentW = max(viewW, scaledW);
  const int contentH = max(viewH, scaledH);

  lv_obj_set_size(imageViewerContent, contentW, contentH);
  lv_obj_set_size(imageViewerObject, imageViewerBaseW, imageViewerBaseH);
  lv_image_set_scale(imageViewerObject, scale);
  lv_obj_set_pos(imageViewerObject, (contentW - imageViewerBaseW) / 2,
                 (contentH - imageViewerBaseH) / 2);
  lv_obj_scroll_to_x(imageViewerViewport, max(0, (contentW - viewW) / 2), LV_ANIM_OFF);
  lv_obj_scroll_to_y(imageViewerViewport, max(0, (contentH - viewH) / 2), LV_ANIM_OFF);

  if (imageViewerZoomLabel) {
    char b[20];
    snprintf(b, sizeof(b), "%u%%", static_cast<unsigned>((scale * 100U + 128U) / 256U));
    lv_label_set_text(imageViewerZoomLabel, b);
  }
}

static void imageViewerBackEvent(lv_event_t* e) {
  (void)e;
  stopImageViewer();
}

static void imageViewerZoomOutEvent(lv_event_t* e) {
  (void)e;
  uint16_t next = static_cast<uint16_t>((imageViewerScale * 4U) / 5U);
  if (next == imageViewerScale && next > 32) --next;
  applyImageViewerScale(next);
}

static void imageViewerFitEvent(lv_event_t* e) {
  (void)e;
  applyImageViewerScale(imageViewerFitScale);
}

static void imageViewerZoomInEvent(lv_event_t* e) {
  (void)e;
  uint32_t next = (imageViewerScale * 5U) / 4U;
  if (next == imageViewerScale) ++next;
  if (next > 1024UL) next = 1024UL;
  applyImageViewerScale(static_cast<uint16_t>(next));
}

static void startImageViewer(const String& path) {
  if (!pathStorageReady(path) || !isReadableImageFile(path)) return;
  // Keep the bounded Explorer thumbnail so returning from the viewer preserves
  // its contents without rereading the SD card. The full canvas has its own budget.
  freeImageViewerCanvas();
  String err;
  int naturalW = 0, naturalH = 0;
  if (!loadSdImageCanvas(path, 4096, 4096, imageViewerCanvas, imageViewerDsc,
                         naturalW, naturalH, err)) {
    toast(err.c_str(), 3200);
    return;
  }
  imageViewerBaseW = imageViewerDsc.header.w;
  imageViewerBaseH = imageViewerDsc.header.h;
  imageViewerPath = path;
  imageViewerActive = true;

  imageViewerScreen = lv_obj_create(nullptr);
  lv_obj_set_size(imageViewerScreen, SCREEN_W, SCREEN_H);
  lv_obj_set_style_bg_color(imageViewerScreen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(imageViewerScreen, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(imageViewerScreen, 0, 0);
  lv_obj_remove_flag(imageViewerScreen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* header = lv_obj_create(imageViewerScreen);
  lv_obj_set_size(header, SCREEN_W, 64);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_bg_color(header, C_SURFACE(), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 6, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* back = makeIconButton(header, LV_SYMBOL_LEFT, 54, 50);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_add_event_cb(back, imageViewerBackEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* title = lv_label_create(header);
  lv_obj_set_width(title, 730);
  lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
  lv_label_set_text(title, baseNameOf(path).c_str());
  lv_obj_set_style_text_font(title, uiFont(), 0);
  lv_obj_set_style_text_color(title, C_TEXT(), 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 70, 0);

  lv_obj_t* zoomOut = makeIconButton(header, LV_SYMBOL_MINUS, 54, 50);
  lv_obj_align(zoomOut, LV_ALIGN_RIGHT_MID, -194, 0);
  lv_obj_add_event_cb(zoomOut, imageViewerZoomOutEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* fit = makeIconButton(header, LV_SYMBOL_IMAGE, 54, 50);
  lv_obj_align(fit, LV_ALIGN_RIGHT_MID, -132, 0);
  lv_obj_add_event_cb(fit, imageViewerFitEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* zoomIn = makeIconButton(header, LV_SYMBOL_PLUS, 54, 50);
  lv_obj_align(zoomIn, LV_ALIGN_RIGHT_MID, -70, 0);
  lv_obj_add_event_cb(zoomIn, imageViewerZoomInEvent, LV_EVENT_CLICKED, nullptr);
  imageViewerZoomLabel = lv_label_create(header);
  lv_obj_set_width(imageViewerZoomLabel, 62);
  lv_obj_set_style_text_align(imageViewerZoomLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(imageViewerZoomLabel, C_MUTED(), 0);
  lv_obj_align(imageViewerZoomLabel, LV_ALIGN_RIGHT_MID, -2, 0);

  imageViewerViewport = lv_obj_create(imageViewerScreen);
  lv_obj_set_size(imageViewerViewport, SCREEN_W, SCREEN_H - 64);
  lv_obj_set_pos(imageViewerViewport, 0, 64);
  lv_obj_set_style_bg_color(imageViewerViewport, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(imageViewerViewport, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(imageViewerViewport, 0, 0);
  lv_obj_set_style_radius(imageViewerViewport, 0, 0);
  lv_obj_set_style_pad_all(imageViewerViewport, 0, 0);
  lv_obj_add_flag(imageViewerViewport, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(imageViewerViewport, LV_DIR_ALL);
  lv_obj_set_scrollbar_mode(imageViewerViewport, LV_SCROLLBAR_MODE_AUTO);

  imageViewerContent = lv_obj_create(imageViewerViewport);
  lv_obj_remove_style_all(imageViewerContent);
  lv_obj_set_pos(imageViewerContent, 0, 0);
  lv_obj_remove_flag(imageViewerContent, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(imageViewerContent, LV_OBJ_FLAG_CLICKABLE);

  imageViewerObject = lv_image_create(imageViewerContent);
  lv_image_set_src(imageViewerObject, &imageViewerDsc);
  lv_obj_remove_flag(imageViewerObject, LV_OBJ_FLAG_CLICKABLE);

  lv_screen_load(imageViewerScreen);
  lv_obj_update_layout(imageViewerScreen);
  const int viewW = lv_obj_get_content_width(imageViewerViewport);
  const int viewH = lv_obj_get_content_height(imageViewerViewport);
  uint32_t fitX = static_cast<uint32_t>(viewW) * 256U / max(1, imageViewerBaseW);
  uint32_t fitY = static_cast<uint32_t>(viewH) * 256U / max(1, imageViewerBaseH);
  uint32_t fitScale = fitX < fitY ? fitX : fitY;
  if (fitScale > 256UL) fitScale = 256UL;
  imageViewerFitScale = static_cast<uint16_t>(fitScale);
  if (imageViewerFitScale < 32) imageViewerFitScale = 32;
  applyImageViewerScale(imageViewerFitScale);
}

static void stopImageViewer() {
  if (!imageViewerActive) return;
  imageViewerActive = false;
  imageViewerPath = "";
  if (imageViewerObject && lv_obj_is_valid(imageViewerObject))
    lv_image_set_src(imageViewerObject, nullptr);
  lv_obj_t* old = imageViewerScreen;
  lv_screen_load(root);
  imageViewerScreen = nullptr;
  imageViewerViewport = nullptr;
  imageViewerContent = nullptr;
  imageViewerObject = nullptr;
  imageViewerZoomLabel = nullptr;
  if (old && lv_obj_is_valid(old)) lv_obj_delete(old);
  freeImageViewerCanvas();
  lv_obj_invalidate(root);
  updateClockLabels(true);
}

static float readerDetectBookAspect() {
  // Fixed-layout EPUB can explicitly define its original page geometry.
  if (readerBook.defaultViewportW > 0 && readerBook.defaultViewportH > 0) {
    const float a = static_cast<float>(readerBook.defaultViewportW) /
                    static_cast<float>(readerBook.defaultViewportH);
    if (a > 0.25f && a < 4.0f) return a;
  }

  // Reflowable EPUB has no physical page size. The cover ratio is nevertheless
  // a useful publisher-provided hint for a book-like virtual page.
  if (readerBook.coverPath.length()) {
    uint8_t* data = nullptr;
    size_t len = 0;
    String err;
    if (epub_lite::extractResource(tab5FS, readerPath.c_str(), readerBook.coverPath,
                                   &data, &len, 12U * 1024U * 1024U, err)) {
      int w = 0, h = 0;
      const bool ok = imageNaturalSize(data, len, w, h);
      heap_caps_free(data);
      if (ok && w > 0 && h > 0) {
        float a = static_cast<float>(w) / static_cast<float>(h);
        // Covers can have unusual crops. Keep the heuristic within plausible
        // portrait-book bounds; fixed-layout viewport ratios are not clamped.
        if (a >= 0.52f && a <= 0.85f) return a;
      }
    }
  }

  return 2.0f / 3.0f;
}

static void readerApplyEpubGeometry(float aspect) {
  if (!readerContent || !readerIsEpub) return;
  if (aspect < 0.25f || aspect > 4.0f) aspect = 2.0f / 3.0f;

  readerDisplayedAspect = aspect;
  const int rw = lv_display_get_horizontal_resolution(lvDisplay);
  const int rh = lv_display_get_vertical_resolution(lvDisplay);
  const int zoneY = 58;
  const int zoneH = rh - 128;
  const auto box=epub_pages::geometry(rw,rh,aspect);
  const int pageW=box.width,pageH=box.height;

  lv_obj_set_size(readerContent, pageW, pageH);
  lv_obj_set_pos(readerContent, (rw - pageW) / 2, zoneY + (zoneH - pageH) / 2);
  lv_obj_set_style_bg_color(readerContent, C_SURFACE(), 0);Tab5ThemeLive::bind(readerContent,LV_STYLE_BG_COLOR,0,1);
  lv_obj_set_style_border_width(readerContent, 0, 0);
  lv_obj_set_style_radius(readerContent, 0, 0);
  // These are reading margins inside the page, not unused screen gutters.
  // Keep them modest so the 720 px panel is effectively maximized.
  constexpr int EPUB_PAD_X = 18;
  constexpr int EPUB_PAD_Y = 18;
  lv_obj_set_style_pad_left(readerContent, EPUB_PAD_X, 0);
  lv_obj_set_style_pad_right(readerContent, EPUB_PAD_X, 0);
  lv_obj_set_style_pad_top(readerContent, EPUB_PAD_Y, 0);
  lv_obj_set_style_pad_bottom(readerContent, EPUB_PAD_Y, 0);

  if (readerTextLabel) {
    lv_obj_set_width(readerTextLabel, pageW - 2 * EPUB_PAD_X);
    lv_obj_set_height(readerTextLabel, pageH - 2 * EPUB_PAD_Y);
  }
  lv_obj_update_layout(readerContent);
  const int contentW = lv_obj_get_content_width(readerContent);
  const int contentH = lv_obj_get_content_height(readerContent);
  if (readerRichHost) lv_obj_set_size(readerRichHost, contentW, contentH);
  if (readerMeasureHost) lv_obj_set_size(readerMeasureHost, contentW, contentH);

  if (readerTapLayer) {
    lv_obj_set_size(readerTapLayer, pageW, pageH);
    lv_obj_set_pos(readerTapLayer, (rw - pageW) / 2,
                   zoneY + (zoneH - pageH) / 2);
    lv_obj_move_foreground(readerTapLayer);
  }
}


// -----------------------------------------------------------------------------
// EPUB rich layout: XHTML/CSS display list -> LVGL spans/tables -> pages
// -----------------------------------------------------------------------------

static inline bool richLocEqual(const RichLoc& a, const RichLoc& b) {
  return a.block == b.block && a.offset == b.offset;
}

static RichLoc richDocEnd() {
  RichLoc l; l.block = readerRichDoc.blockCount; l.offset = 0; return l;
}

static lv_text_align_t richAlign(epub_render::Align a) {
  switch (a) {
    case epub_render::Align::Center: return LV_TEXT_ALIGN_CENTER;
    case epub_render::Align::Right: return LV_TEXT_ALIGN_RIGHT;
    case epub_render::Align::Justify: return LV_TEXT_ALIGN_LEFT; // LVGL span has no full justification
    default: return LV_TEXT_ALIGN_LEFT;
  }
}

static uint8_t richByteAt(const epub_render::Block& b, uint32_t logical) {
  uint32_t pos = 0;
  for (uint16_t i = 0; i < b.runCount; ++i) {
    const epub_render::Run& r = readerRichDoc.runs[b.runStart + i];
    if (logical < pos + r.textLen) return static_cast<uint8_t>(readerRichDoc.ptr(r.textOff)[logical - pos]);
    pos += r.textLen;
  }
  return 0;
}

static uint32_t richUtf8BoundaryBack(const epub_render::Block& b, uint32_t p, uint32_t floor) {
  while (p > floor && p < b.textLen && (richByteAt(b, p) & 0xC0) == 0x80) --p;
  return p;
}

static inline bool richSpace(uint8_t c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t'; }

static uint32_t richPrevWordBoundary(const epub_render::Block& b, uint32_t p, uint32_t floor) {
  if (p <= floor) return floor;
  p = richUtf8BoundaryBack(b, p, floor);
  while (p > floor && richSpace(richByteAt(b, p - 1))) --p;
  while (p > floor && !richSpace(richByteAt(b, p - 1))) --p;
  return p;
}

static uint32_t richNextWordBoundary(const epub_render::Block& b, uint32_t p) {
  while (p < b.textLen && richSpace(richByteAt(b, p))) ++p;
  while (p < b.textLen && !richSpace(richByteAt(b, p))) ++p;
  return richUtf8BoundaryBack(b, p, 0);
}

static lv_obj_t* richCreateSpanGroup(lv_obj_t* parent, const epub_render::Block& b,
                                     uint32_t startOff, int width) {
#if LV_USE_SPAN
  lv_obj_t* sp = lv_spangroup_create(parent);
  lv_obj_remove_style_all(sp);
  lv_obj_set_width(sp, width);
  lv_spangroup_set_mode(sp, LV_SPAN_MODE_BREAK);
  lv_spangroup_set_align(sp, richAlign(b.align));
  lv_spangroup_set_indent(sp, startOff == 0 ? b.indent : 0);
  lv_obj_set_style_text_color(sp, C_TEXT(), 0);
  lv_obj_set_style_text_line_space(sp, b.lineSpace, 0);

  if (b.kind == epub_render::BlockKind::Quote) {
    lv_obj_set_style_border_width(sp, 2, 0);
    lv_obj_set_style_border_side(sp, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_color(sp, C_BORDER(), 0);
    lv_obj_set_style_pad_left(sp, 10, 0);
  }

  return sp;
#else
  (void)parent; (void)b; (void)startOff; (void)width; return nullptr;
#endif
}

static lv_obj_t* richCreateSpanBlock(lv_obj_t* parent, const epub_render::Block& b,
                                     uint32_t startOff, uint32_t endOff, int width) {
#if LV_USE_SPAN
  if (!parent || endOff <= startOff || width < 40) return nullptr;
  lv_obj_t* sp = richCreateSpanGroup(parent, b, startOff, width);
  if (!sp) return nullptr;
  uint32_t logical = 0;
  for (uint16_t i = 0; i < b.runCount; ++i) {
    const epub_render::Run& r = readerRichDoc.runs[b.runStart + i];
    const uint32_t rs = logical;
    const uint32_t re = logical + r.textLen;
    logical = re;
    if (re <= startOff || rs >= endOff) continue;
    const uint32_t a = startOff > rs ? startOff - rs : 0;
    const uint32_t z = endOff < re ? endOff - rs : r.textLen;
    if (z <= a) continue;
    const size_t n = z - a;
    char* tmp = static_cast<char*>(heap_caps_malloc(n + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!tmp) { lv_obj_delete(sp); return nullptr; }
    memcpy(tmp, readerRichDoc.ptr(r.textOff) + a, n); tmp[n] = 0;
#if LVGL_VERSION_MINOR >= 4
    lv_span_t* span = lv_spangroup_add_span(sp);
#else
    lv_span_t* span = lv_spangroup_new_span(sp);
#endif
    if (!span) { heap_caps_free(tmp); lv_obj_delete(sp); return nullptr; }
    if (span) {
      lv_span_set_text(span, tmp); // copies text
      if (!lv_span_get_text(span)) { heap_caps_free(tmp); lv_obj_delete(sp); return nullptr; }
      // lv_span_t is intentionally opaque in LVGL 9 public headers.
      // Never access span->style directly; use the public accessor.
      lv_style_t* spanStyle = lv_span_get_style(span);
      if (spanStyle) {
        lv_style_set_text_font(spanStyle, richFontFor(r.style.fontPx, r.style.flags));
        lv_style_set_text_color(spanStyle, C_TEXT());
        if (r.style.flags & epub_render::TXT_UNDERLINE)
          lv_style_set_text_decor(spanStyle, LV_TEXT_DECOR_UNDERLINE);
      }
    }
    heap_caps_free(tmp);
  }
  // Refresh content now; the caller performs the single layout pass after
  // positioning the block. Avoiding a second update noticeably reduces page-turn cost.
  lv_spangroup_refresh(sp);
  return sp;
#else
  (void)parent; (void)b; (void)startOff; (void)endOff; (void)width; return nullptr;
#endif
}

#include "Tab5EpubTextLayout.inc"

static lv_obj_t* richCreateTable(lv_obj_t* parent, const epub_render::Block& b,
                                 uint16_t rowStart, uint16_t rowEnd, int width) {
#if LV_USE_TABLE
  if (!parent || rowEnd <= rowStart || !b.cols) return nullptr;
  lv_obj_t* t = lv_table_create(parent);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(t, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_set_style_bg_color(t, C_SURFACE(), 0);
  lv_obj_set_style_border_color(t, C_BORDER(), 0);
  lv_obj_set_style_border_width(t, 1, 0);
  lv_obj_set_style_text_color(t, C_TEXT(), LV_PART_ITEMS);
  lv_obj_set_style_text_font(t, richFontFor(fontSizeForIndex(settings.fontIndex), 0), LV_PART_ITEMS);
  lv_obj_set_style_pad_all(t, 5, LV_PART_ITEMS);
  lv_obj_set_style_border_color(t, C_BORDER(), LV_PART_ITEMS);
  lv_obj_set_style_border_width(t, 1, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(t, C_RAISED(), LV_PART_ITEMS);
  lv_table_set_row_count(t, rowEnd - rowStart);
  lv_table_set_column_count(t, b.cols);
  const int cw = width / b.cols;
  for (uint16_t c = 0; c < b.cols; ++c) lv_table_set_column_width(t, c, cw);
  for (uint16_t i = 0; i < b.cellCount; ++i) {
    const epub_render::Cell& cell = readerRichDoc.cells[b.cellStart + i];
    if (cell.row < rowStart || cell.row >= rowEnd || cell.col >= b.cols) continue;
    lv_table_set_cell_value(t, cell.row - rowStart, cell.col, readerRichDoc.ptr(cell.textOff));
  }
  lv_obj_set_height(t, LV_SIZE_CONTENT);
  lv_obj_update_layout(t);
  return t;
#else
  (void)parent; (void)b; (void)rowStart; (void)rowEnd; (void)width; return nullptr;
#endif
}

static int richMeasureTable(const epub_render::Block& b, uint16_t rowStart, uint16_t rowEnd, int width) {
  if (!readerMeasureHost) return 0;
  lv_obj_t* t = richCreateTable(readerMeasureHost, b, rowStart, rowEnd, width);
  if (!t) return 0;
  lv_obj_update_layout(t);
  const int h = lv_obj_get_height(t);
  lv_obj_delete(t);
  return h;
}

static RichLoc richComputeNextPageStart(RichLoc start) {
  richTextLayoutError = false;
  RichLoc cur = start;
  if (cur.block >= readerRichDoc.blockCount) return richDocEnd();
  const int pageW = readerContent ? lv_obj_get_content_width(readerContent) : 680;
  const int pageH = readerContent ? lv_obj_get_content_height(readerContent) : 1000;
  int y = 0;
  bool any = false;

  while (cur.block < readerRichDoc.blockCount) {
    const epub_render::Block& b = readerRichDoc.blocks[cur.block];
    const bool atStart = cur.offset == 0;
    const int mt = atStart ? b.marginTop : 0;
    const int mb = b.marginBottom;
    const int width = max(60, pageW - b.padLeft - b.padRight);

    if (b.kind == epub_render::BlockKind::Image) {
      if (any) return cur; // image starts next page
      RichLoc n; n.block = cur.block + 1; n.offset = 0; return n; // dedicated page
    }
    if (b.kind == epub_render::BlockKind::Rule) {
      const int h = mt + 14 + mb;
      if (any && y + h > pageH) return cur;
      y += h; any = true; ++cur.block; cur.offset = 0; continue;
    }
    if (b.kind == epub_render::BlockKind::Table) {
      uint16_t row = static_cast<uint16_t>(cur.offset);
      if (row >= b.rows) { ++cur.block; cur.offset = 0; continue; }
      const int avail = pageH - y - mt - mb;
      uint16_t best = row;
      for (uint16_t r = row + 1; r <= b.rows; ++r) {
        const int h = richMeasureTable(b, row, r, width);
        if (h <= avail || (!any && r == row + 1)) best = r; else break;
      }
      if (best == row) return cur;
      if (best < b.rows) { RichLoc n{cur.block, best}; return n; }
      y += mt + richMeasureTable(b, row, best, width) + mb;
      any = true; ++cur.block; cur.offset = 0; continue;
    }

    // Rich text block.
    uint32_t off = cur.offset;
    if (off >= b.textLen) { ++cur.block; cur.offset = 0; continue; }
    const int avail = pageH - y - mt;
    uint32_t end = off;
    const int fittedH = richLayoutText(b, off, b.textLen, width, avail, &end);
    if (richTextLayoutError) return start; // No unmeasured boundary may enter the cache.
    if (end == b.textLen) {
      y += mt + fittedH + mb; any = true; ++cur.block; cur.offset = 0;
      if (y >= pageH) return cur;
      continue;
    }
    if (end <= off) {
      if (any) return cur;
      end = richNextWordBoundary(b, off);
      if (end <= off) end = b.textLen;
    }
    RichLoc n{cur.block, end};
    return n;
  }
  return richDocEnd();
}

static bool readerRichResetPagination() {
  readerRichPageStartCount = 0;
  if (!readerRichPageStarts.ensure(1)) return false;
  readerPageIndex = 0;
  readerRichPageStartCount = 1;
  readerRichPageStarts[0] = RichLoc{0, 0};
  readerRichPaginationComplete = readerRichDoc.blockCount == 0;
  return true;
}

static bool readerRichEnsureNextPage(uint16_t page) {
  if (!readerRichDoc.blockCount || page >= READER_PAGE_CACHE_MAX) return false;
  if (page + 1 < readerRichPageStartCount) return true;
  if (page >= readerRichPageStartCount) return false;
  RichLoc next = richComputeNextPageStart(readerRichPageStarts[page]);
  if (richLocEqual(next, readerRichPageStarts[page])) return false;
  if (!readerRichPageStarts.ensure(readerRichPageStartCount + 1U)) return false;
  readerRichPageStarts[readerRichPageStartCount++] = next;
  if (next.block >= readerRichDoc.blockCount) readerRichPaginationComplete = true;
  return true;
}

static bool readerRichRenderPage() {
  if (!readerRichHost || !readerRichDoc.blockCount || readerPageIndex >= readerRichPageStartCount) return false;
  if (!readerRichEnsureNextPage(readerPageIndex)) {
    toast("Mémoire insuffisante pour la pagination", 3000);
    return false;
  }
  RichLoc start = readerRichPageStarts[readerPageIndex];
  RichLoc end = (readerPageIndex + 1 < readerRichPageStartCount) ? readerRichPageStarts[readerPageIndex + 1] : richDocEnd();

  lv_obj_clean(readerRichHost);
  lv_obj_add_flag(readerTextLabel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(readerImageObj, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(readerRichHost, LV_OBJ_FLAG_HIDDEN);

  const int pageW = lv_obj_get_width(readerRichHost);
  int y = 0;
  RichLoc cur = start;
  while (cur.block < readerRichDoc.blockCount &&
         (cur.block < end.block || (cur.block == end.block && cur.offset < end.offset))) {
    const epub_render::Block& b = readerRichDoc.blocks[cur.block];
    const bool atStart = cur.offset == 0;
    if (atStart) y += b.marginTop;
    const int x = b.padLeft;
    const int width = max(60, pageW - b.padLeft - b.padRight);

    if (b.kind == epub_render::BlockKind::Image) {
      // Dedicated image page. Existing image renderer already handles EPUB ZIP
      // extraction and RGB565 byte order correctly.
      lv_obj_add_flag(readerRichHost, LV_OBJ_FLAG_HIDDEN);
      return readerRenderImage(String(readerRichDoc.ptr(b.resourceOff)));
    }
    if (b.kind == epub_render::BlockKind::Rule) {
      lv_obj_t* hr = lv_obj_create(readerRichHost);
      lv_obj_remove_style_all(hr);
      lv_obj_set_size(hr, width, 1);
      lv_obj_set_pos(hr, x, y + 6);
      lv_obj_set_style_bg_color(hr, C_BORDER(), 0);
      lv_obj_set_style_bg_opa(hr, LV_OPA_COVER, 0);
      y += 14 + b.marginBottom;
      ++cur.block; cur.offset = 0; continue;
    }
    if (b.kind == epub_render::BlockKind::Table) {
      const uint16_t rowStart = static_cast<uint16_t>(cur.offset);
      const uint16_t rowEnd = (end.block == cur.block) ? static_cast<uint16_t>(end.offset) : b.rows;
      lv_obj_t* t = richCreateTable(readerRichHost, b, rowStart, rowEnd, width);
      if (!t) return false;
      if (t) { lv_obj_set_pos(t, x, y); lv_obj_update_layout(t); y += lv_obj_get_height(t); }
      if (rowEnd >= b.rows) { y += b.marginBottom; ++cur.block; cur.offset = 0; }
      else cur.offset = rowEnd;
      continue;
    }

    const uint32_t textEnd = (end.block == cur.block) ? end.offset : b.textLen;
    lv_obj_t* sp = richCreateSpanBlock(readerRichHost, b, cur.offset, textEnd, width);
    if (!sp) return false;
    if (sp) { lv_obj_set_pos(sp, x, y); lv_obj_update_layout(sp); y += lv_obj_get_height(sp); }
    if (textEnd >= b.textLen) { y += b.marginBottom; ++cur.block; cur.offset = 0; }
    else cur.offset = textEnd;
  }
  return true;
}

static bool readerRenderImage(const String& entryPath) {
  freeReaderImage();
  if (!readerImageObj) return false;

  uint8_t* data = nullptr;
  size_t len = 0;
  String err;
  if (!epub_lite::extractResource(tab5FS, readerPath.c_str(), entryPath, &data, &len, 12U * 1024U * 1024U, err)) {
    lv_obj_remove_flag(readerTextLabel, LV_OBJ_FLAG_HIDDEN);
    String msg = T("Image EPUB inaccessible\n\n") + entryPath + "\n\n" + translateRuntimeText(err);
    lv_label_set_text(readerTextLabel, msg.c_str());
    return false;
  }

  int naturalW = 0, naturalH = 0;
  imageNaturalSize(data, len, naturalW, naturalH);
  int targetW = readerContent ? lv_obj_get_content_width(readerContent) : 0;
  int targetH = readerContent ? lv_obj_get_content_height(readerContent) : 0;
  if (targetW < 100) targetW = lv_display_get_horizontal_resolution(lvDisplay) - 52;
  if (targetH < 100) targetH = lv_display_get_vertical_resolution(lvDisplay) - 138;

  readerImageCanvas = new M5Canvas(&M5.Display);
  if (!readerImageCanvas) { heap_caps_free(data); return false; }
  readerImageCanvas->setPsram(true);
  readerImageCanvas->setColorDepth(16);
  if (!readerImageCanvas->createSprite(targetW, targetH)) {
    delete readerImageCanvas; readerImageCanvas = nullptr; heap_caps_free(data); return false;
  }
  readerImageCanvas->setSwapBytes(false);
  readerImageCanvas->fillSprite(TFT_BLACK);

  float scale = 1.0f;
  if (naturalW > 0 && naturalH > 0) {
    const float sx = static_cast<float>(targetW) / naturalW;
    const float sy = static_cast<float>(targetH) / naturalH;
    scale = sx < sy ? sx : sy;
    if (scale > 1.0f) scale = 1.0f; // no needless enlargement of small illustrations
  }

  const int scaledW = (naturalW > 0) ? static_cast<int>(naturalW * scale) : targetW;
  const int scaledH = (naturalH > 0) ? static_cast<int>(naturalH * scale) : targetH;
  const int drawX = (targetW > scaledW) ? (targetW - scaledW) / 2 : 0;
  const int drawY = (targetH > scaledH) ? (targetH - scaledH) / 2 : 0;

  String low = entryPath; low.toLowerCase();
  Tab5ImageLock imageLock(3000);
  bool supported = true;
  if (!imageLock) supported=false;
  else if ((len > 2 && data[0] == 0xFF && data[1] == 0xD8) || low.endsWith(".jpg") || low.endsWith(".jpeg")) {
    readerImageCanvas->drawJpg(data, len, drawX, drawY, targetW - drawX, targetH - drawY, 0, 0, scale, scale, datum_t::top_left);
  } else if ((len > 8 && data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') || low.endsWith(".png")) {
    readerImageCanvas->drawPng(data, len, drawX, drawY, targetW - drawX, targetH - drawY, 0, 0, scale, scale, datum_t::top_left);
  } else if ((len > 2 && data[0] == 'B' && data[1] == 'M') || low.endsWith(".bmp")) {
    readerImageCanvas->drawBmp(data, len, drawX, drawY, targetW - drawX, targetH - drawY, 0, 0, scale, scale, datum_t::top_left);
  } else supported = false;
  heap_caps_free(data);

  if (!supported) {
    freeReaderImage();
    lv_obj_remove_flag(readerTextLabel, LV_OBJ_FLAG_HIDDEN);
    String msg = T("Image EPUB non prise en charge\n\n") + entryPath + T("\n\n(JPEG / PNG / BMP pris en charge)");
    lv_label_set_text(readerTextLabel, msg.c_str());
    return false;
  }

  memset(&readerImageDsc, 0, sizeof(readerImageDsc));
  readerImageDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  readerImageDsc.header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
  readerImageDsc.header.flags = 0;
  readerImageDsc.header.w = targetW;
  readerImageDsc.header.h = targetH;
  readerImageDsc.header.stride = targetW * 2;
  readerImageDsc.data_size = static_cast<uint32_t>(targetW * targetH * 2);
  readerImageDsc.data = static_cast<const uint8_t*>(readerImageCanvas->getBuffer());

  lv_image_set_src(readerImageObj, &readerImageDsc);
  lv_obj_set_size(readerImageObj, targetW, targetH);
  lv_obj_center(readerImageObj);
  lv_obj_add_flag(readerTextLabel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_remove_flag(readerImageObj, LV_OBJ_FLAG_HIDDEN);
  return true;
}

static uint32_t readerProgressHash(const String& path) {
  uint32_t h = 2166136261u; // FNV-1a 32-bit
  for (size_t i = 0; i < path.length(); ++i) {
    h ^= static_cast<uint8_t>(path[i]);
    h *= 16777619u;
  }
  return h;
}

static String readerProgressPath(const String& path) {
  char b[48];
  snprintf(b, sizeof(b), "%s/%08lX.pos", PROGRESS_DIR,
           static_cast<unsigned long>(readerProgressHash(path)));
  return String(b);
}

static bool loadReaderProgress(const String& path, uint16_t& chapter, uint16_t& block,
                               uint32_t& offset, int32_t& scrollY, bool& hasRichLoc) {
  if (!sdReady) return false;
  const String fn = readerProgressPath(path);
  bool present=false;if(!tab5AtomicProbe(fn.c_str(),present))return false;
  const String backup=fn+".bak";
  errno=0;File f = tab5FS.open(present?fn.c_str():backup.c_str(), FILE_READ);
  if (!f || f.isDirectory()) return false;
  errno=0;const size_t expected=f.size();if(errno||expected>2048)return false;
  uint16_t loadedChapter=0,loadedBlock=0;uint32_t loadedOffset=0;int32_t loadedScroll=0;bool loadedRich=false;
  bool pathOk = false;
  bool havePosition = false;
  size_t position=0;
  while (position<expected) {
    char buffer[1100];size_t used=0;
    while(position<expected){
      const int c=f.read();if(c<0)return false;++position;if(c=='\n')break;
      if(used==sizeof(buffer)-1||!c)return false;buffer[used++]=char(c);
    }
    buffer[used]=0;String line(buffer);if(line.length()!=used)return false;
    line.trim();
    int eq = line.indexOf('=');
    if (eq < 1) continue;
    String key = line.substring(0, eq);
    String value = line.substring(eq + 1);
    if (key == "path") pathOk = (value == path);
    else if (key == "chapter") { loadedChapter = static_cast<uint16_t>(value.toInt()); havePosition = true; }
    else if (key == "block") { loadedBlock = static_cast<uint16_t>(value.toInt()); loadedRich = true; havePosition = true; }
    else if (key == "offset") { loadedOffset = static_cast<uint32_t>(strtoul(value.c_str(), nullptr, 10)); havePosition = true; }
    else if (key == "scroll") { loadedScroll = static_cast<int32_t>(strtol(value.c_str(), nullptr, 10)); havePosition = true; }
  }
  errno=0;const bool complete=pathOk&&havePosition&&f.position()==expected&&f.size()==expected&&!errno;
  f.close();
  if(!complete)return false;
  chapter=loadedChapter;block=loadedBlock;offset=loadedOffset;scrollY=loadedScroll;hasRichLoc=loadedRich;return true;
}

static void migrateReaderProgress(const String& oldPath, const String& newPath) {
  if (!sdReady || oldPath == newPath || !oldPath.length() || !newPath.length()) return;
  uint16_t chapter = 0;
  uint16_t block = 0;
  uint32_t offset = 0;
  int32_t scrollY = 0;
  bool hasRichLoc = false;
  if (!loadReaderProgress(oldPath, chapter, block, offset, scrollY, hasRichLoc)) return;

  bool present=false;if(!tab5AtomicProbe(PROGRESS_DIR,present)||(!present&&!tab5FS.mkdir(PROGRESS_DIR)))return;
  const String oldFile = readerProgressPath(oldPath);
  const String newFile = readerProgressPath(newPath);
  const String tmp = newFile + ".tmp";
  tab5FS.remove(tmp.c_str());
  File f = tab5FS.open(tmp.c_str(), FILE_WRITE);
  if (!f) return;
  char payload[1280],rich[32]{};if(hasRichLoc)snprintf(rich,sizeof(rich),"block=%u\n",block);
  const int length=snprintf(payload,sizeof(payload),"path=%s\nchapter=%u\n%soffset=%lu\nscroll=%ld\n",newPath.c_str(),chapter,rich,static_cast<unsigned long>(offset),static_cast<long>(scrollY));
  bool complete=length>0&&size_t(length)<sizeof(payload)&&f.write(reinterpret_cast<const uint8_t*>(payload),size_t(length))==size_t(length);
  errno=0;f.flush();complete=!errno&&complete&&f.size()==size_t(length)&&!f.getWriteError();
  errno=0;f.close();complete=complete&&!errno;
  const String backup=newFile+".bak";
  if(complete&&tab5AtomicReplace(tmp.c_str(),newFile.c_str(),backup.c_str()))tab5FS.remove(oldFile.c_str());
}

static void saveReaderProgressNow() {
  if (!readerActive || !sdReady || !readerPath.length() || !readerContent) return;
  if (readerIsEpub && !readerPageDisplayed) return;
  bool present=false;if(!tab5AtomicProbe(PROGRESS_DIR,present)||(!present&&!tab5FS.mkdir(PROGRESS_DIR)))return;

  const String fn = readerProgressPath(readerPath);
  const String tmp = fn + ".tmp";
  const String bak = fn + ".bak";
  tab5FS.remove(tmp.c_str());
  File f = tab5FS.open(tmp.c_str(), FILE_WRITE);
  if (!f) return;

  uint16_t block = 0;
  uint32_t offset = 0;
  if (readerIsEpub && readerRichDoc.blockCount && readerPageIndex < readerRichPageStartCount) {
    block = readerRichPageStarts[readerPageIndex].block;
    offset = readerRichPageStarts[readerPageIndex].offset;
  } else if (readerIsEpub && readerText && readerPageIndex < readerPageStartCount) {
    offset = readerPageStarts[readerPageIndex];
  }
  const int32_t scrollY = readerIsEpub ? 0 : lv_obj_get_scroll_y(readerContent);
  char payload[1280],rich[32]{};if(readerIsEpub&&readerRichDoc.blockCount)snprintf(rich,sizeof(rich),"block=%u\n",block);
  const int length=snprintf(payload,sizeof(payload),"path=%s\nchapter=%u\n%soffset=%lu\nscroll=%ld\n",readerPath.c_str(),readerIsEpub?readerChapter:0,rich,static_cast<unsigned long>(offset),static_cast<long>(scrollY));
  bool complete=length>0&&size_t(length)<sizeof(payload)&&f.write(reinterpret_cast<const uint8_t*>(payload),size_t(length))==size_t(length);
  errno=0;f.flush();complete=!errno&&complete&&f.size()==size_t(length)&&!f.getWriteError();
  errno=0;f.close();complete=complete&&!errno;

  // Small transactional replacement: keep the previous position until the
  // newly written file has been renamed successfully.
  if(!complete||!tab5AtomicReplace(tmp.c_str(),fn.c_str(),bak.c_str()))return;
  readerProgressDirty = false;
}

static void markReaderProgressDirty() {
  if (!readerActive || !readerPath.length()) return;
  readerProgressDirty = true;
  lastReaderProgressChangeMs = millis();
}

static void readerPlainScrollEvent(lv_event_t* e) {
  if (!readerIsEpub && lv_event_get_code(e) == LV_EVENT_SCROLL_END) markReaderProgressDirty();
}

static String readerCurrentSectionName() {
  const char* supplied = epub_lite::sectionLabel(readerBook, readerChapter);
  if (supplied && *supplied) return String(supplied);

  // Navigation documents often omit the cover. Detect an image-only first
  // rich block without turning every spine item into a fake "chapter".
  if (readerChapter == 0 && readerRichDoc.blockCount == 1 &&
      readerRichDoc.blocks[0].kind == epub_render::BlockKind::Image) return T("Couverture");

  if (readerChapter == 0 && readerText && readerTextLen) {
    String imagePath; size_t markerEnd = 0;
    const size_t start = readerSkipPageWhitespace(0);
    if (readerImageMarkerAt(start, &imagePath, &markerEnd) &&
        readerSkipPageWhitespace(markerEnd) >= readerTextLen) return T("Couverture");
  }
  return String(T("Section")) + " " + String(readerChapter + 1);
}

#include "Tab5EpubBookIndex.inc"

static void readerUpdateEpubStatus() {
  if(readerStatusLabel && !readerSeekDragging)lv_label_set_text(readerStatusLabel,readerBookIndexStatusText().c_str());
  readerSeekSync();
}

static bool readerRenderEpubPage() {
  readerPageDisplayed = false;
  freeReaderImage();
  if (readerRichDoc.blockCount) {
    if (!readerRichRenderPage()) return false;
    readerPageDisplayed = true;
    readerUpdateEpubStatus();
    return true;
  }

  // Legacy plain-text fallback for malformed XHTML the rich parser cannot read.
  if (!readerText || !readerTextLabel || readerPageIndex >= readerPageStartCount) return false;
  const size_t start = readerPageStarts[readerPageIndex];
  if (!readerEnsureNextPageStart(readerPageIndex)) return false;
  const size_t end = (readerPageIndex + 1 < readerPageStartCount)
                       ? readerPageStarts[readerPageIndex + 1] : readerTextLen;
  String imagePath; size_t markerEnd = 0;
  if (readerImageMarkerAt(start, &imagePath, &markerEnd)) { if (!readerRenderImage(imagePath)) return false; }
  else {
    lv_obj_add_flag(readerImageObj, LV_OBJ_FLAG_HIDDEN);
    if (readerRichHost) lv_obj_add_flag(readerRichHost, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(readerTextLabel, LV_OBJ_FLAG_HIDDEN);
    size_t textEnd = end;
    while (textEnd > start && (readerText[textEnd - 1] == ' ' || readerText[textEnd - 1] == '\n')) --textEnd;
    char saved = readerText[textEnd]; readerText[textEnd] = 0;
    lv_label_set_text(readerTextLabel, readerText + start);
    readerText[textEnd] = saved;
  }
  readerUpdateEpubStatus();
  readerPageDisplayed = true;
  return true;
}

static bool richLocLE(const RichLoc& a, const RichLoc& b) {
  return a.block < b.block || (a.block == b.block && a.offset <= b.offset);
}

static bool readerLoadEpubChapter(uint16_t chapter, bool goLastPage = false) {
  readerPageDisplayed = false;
  freeReaderText();
  readerRichDoc.clear();
  readerChapter = chapter;
  if (readerStatusLabel) {
    lv_label_set_text(readerStatusLabel, T("Mise en page…"));
    lv_obj_invalidate(readerStatusLabel);
  }
  String err;
  readerChapterViewportW = 0;
  readerChapterViewportH = 0;

  const uint16_t richBase = fontSizeForIndex(settings.fontIndex);
  bool richOk = epub_render::buildDocument(tab5FS, readerPath.c_str(), readerBook, readerChapter,
                                           richBase, readerRichDoc, err);
  if (richOk && readerRichDoc.blockCount) {
    readerChapterViewportW = readerRichDoc.viewportW;
    readerChapterViewportH = readerRichDoc.viewportH;
  } else {
    // Keep a robust fallback for unusual/malformed EPUB content documents.
    readerRichDoc.clear();
    err = T("");
    if (!epub_lite::readChapter(tab5FS, readerPath.c_str(), readerBook, readerChapter,
                                &readerText, &readerTextLen, err,
                                &readerChapterViewportW, &readerChapterViewportH)) {
      lv_obj_remove_flag(readerTextLabel, LV_OBJ_FLAG_HIDDEN);
      String msg = String(T("Impossible de lire la section")) + "\n\n" + translateRuntimeText(err);
      lv_label_set_text(readerTextLabel, msg.c_str());
      return false;
    }
  }

  if (readerBook.fixedLayout && readerChapterViewportW > 0 && readerChapterViewportH > 0) {
    readerApplyEpubGeometry(static_cast<float>(readerChapterViewportW) /
                            static_cast<float>(readerChapterViewportH));
  } else readerApplyEpubGeometry(readerPageAspect);
  if (readerScreen) lv_obj_update_layout(readerScreen);

  if (readerRichDoc.blockCount) {
    if (!readerRichResetPagination()) { toast("Mémoire insuffisante pour la pagination", 3000); return false; }
    if (goLastPage) {
      while (readerRichPageStartCount <= READER_PAGE_CACHE_MAX) {
        uint16_t idx = readerRichPageStartCount - 1;
        if (!readerRichEnsureNextPage(idx)) break;
        if (readerRichPageStarts[readerRichPageStartCount - 1].block >= readerRichDoc.blockCount) break;
      }
      if (readerRichPageStartCount >= 2) readerPageIndex = readerRichPageStartCount - 2;
    } else if (readerResumePending && readerResumeChapter == readerChapter && readerResumeHasRichLoc) {
      RichLoc target{readerResumeBlock, readerResumeOffset};
      while (readerPageIndex < READER_PAGE_CACHE_MAX) {
        if (!readerRichEnsureNextPage(readerPageIndex)) break;
        if (readerPageIndex + 1 >= readerRichPageStartCount) break;
        RichLoc next = readerRichPageStarts[readerPageIndex + 1];
        if (richLocLE(next, target) && next.block < readerRichDoc.blockCount) ++readerPageIndex;
        else break;
      }
      readerResumePending = false;
    } else if (readerResumePending && readerResumeChapter == readerChapter) {
      // Old v0.5.x position files had no rich block location. Resume at the
      // beginning of the saved section rather than interpreting a stale byte
      // offset with a new DOM model.
      readerResumePending = false;
    }
  } else {
    if (!readerResetPagination()) { toast("Mémoire insuffisante pour la pagination", 3000); return false; }
    if (goLastPage && readerTextLen) {
      while (readerPageStartCount <= READER_PAGE_CACHE_MAX) {
        uint16_t idx = readerPageStartCount - 1;
        if (!readerEnsureNextPageStart(idx)) break;
        if (readerPageStarts[readerPageStartCount - 1] >= readerTextLen) break;
      }
      if (readerPageStartCount >= 2) readerPageIndex = readerPageStartCount - 2;
    } else if (readerResumePending && readerResumeChapter == readerChapter && readerTextLen) {
      while (readerPageIndex < READER_PAGE_CACHE_MAX) {
        if (!readerEnsureNextPageStart(readerPageIndex)) break;
        if (readerPageIndex + 1 >= readerPageStartCount) break;
        const uint32_t next = readerPageStarts[readerPageIndex + 1];
        if (next <= readerResumeOffset && next < readerTextLen) ++readerPageIndex; else break;
      }
      readerResumePending = false;
    }
  }
  readerRenderEpubPage();
  return readerPageDisplayed;
}

#include "Tab5EpubSeek.inc"

static void readerLoadCurrent() {
  if (!readerActive || !readerTextLabel) return;
  String title = readerIsEpub && readerBook.title.length() ? readerBook.title : baseNameOf(readerPath);
  lv_label_set_text(readerTitleLabel, title.c_str());
  if (readerIsEpub) { readerLoadEpubChapter(readerChapter, false); return; }

  freeReaderText();
  String err;
  if (!loadPlainReaderText(readerPath, err)) {
    String msg = String(T("Impossible de lire le document")) + "\n\n" + translateRuntimeText(err);
    lv_label_set_text(readerTextLabel, msg.c_str());
  } else {
    lv_obj_remove_flag(readerTextLabel, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_static(readerTextLabel, readerText);
  }
  lv_label_set_text(readerStatusLabel, T("Lecture portrait · défilement vertical"));
  if (readerContent) {
    lv_obj_update_layout(readerContent);
    lv_obj_scroll_to_y(readerContent, readerResumePending ? readerResumeScrollY : 0, LV_ANIM_OFF);
  }
  readerResumePending = false;
}

static bool readerGoNextPage() {
  if (!readerIsEpub) return false;
  readerSeekCancel();
  if (readerRichDoc.blockCount) {
    if (!readerRichEnsureNextPage(readerPageIndex)) {
      toast("Mémoire insuffisante pour la pagination", 3000); return false;
    }
    if (readerPageIndex + 1 < readerRichPageStartCount &&
        readerRichPageStarts[readerPageIndex + 1].block < readerRichDoc.blockCount) {
      if (!readerRichEnsureNextPage(readerPageIndex + 1)) {
        toast("Mémoire insuffisante pour la pagination", 3000); return false;
      }
      ++readerPageIndex; if (!readerRenderEpubPage()) { --readerPageIndex; readerRenderEpubPage(); return false; } markReaderProgressDirty(); return true;
    }
    if (readerChapter + 1 < readerBook.chapterCount) {
      bool ok = readerLoadEpubChapter(readerChapter + 1, false); if (ok) markReaderProgressDirty(); return ok;
    }
    return false;
  }
  if (!readerText) return false;
  if (!readerEnsureNextPageStart(readerPageIndex)) { toast("Mémoire insuffisante pour la pagination", 3000); return false; }
  if (readerPageIndex + 1 < readerPageStartCount && readerPageStarts[readerPageIndex + 1] < readerTextLen) {
    ++readerPageIndex; if (!readerRenderEpubPage()) { --readerPageIndex; readerRenderEpubPage(); return false; } markReaderProgressDirty(); return true;
  }
  if (readerChapter + 1 < readerBook.chapterCount) {
    bool ok = readerLoadEpubChapter(readerChapter + 1, false); if (ok) markReaderProgressDirty(); return ok;
  }
  return false;
}

static bool readerGoPrevPage() {
  if (!readerIsEpub) return false;
  readerSeekCancel();
  if (readerRichDoc.blockCount) {
    if (readerPageIndex > 0) { --readerPageIndex; if (!readerRenderEpubPage()) { ++readerPageIndex; readerRenderEpubPage(); return false; } markReaderProgressDirty(); return true; }
    if (readerChapter > 0) { bool ok = readerLoadEpubChapter(readerChapter - 1, true); if (ok) markReaderProgressDirty(); return ok; }
    return false;
  }
  if (!readerText) return false;
  if (readerPageIndex > 0) { --readerPageIndex; if (!readerRenderEpubPage()) { ++readerPageIndex; readerRenderEpubPage(); return false; } markReaderProgressDirty(); return true; }
  if (readerChapter > 0) { bool ok = readerLoadEpubChapter(readerChapter - 1, true); if (ok) markReaderProgressDirty(); return ok; }
  return false;
}

static void readerBackEvent(lv_event_t* e) { (void)e; stopReader(); }

static void readerRestartEvent(lv_event_t* e) {
  (void)e;
  if (!readerActive || !readerContent) return;
  readerSeekCancel();
  readerResumePending = false;
  readerResumeHasRichLoc = false;
  readerResumeChapter = 0;
  readerResumeBlock = 0;
  readerResumeOffset = 0;
  readerResumeScrollY = 0;
  readerChapter = 0;
  readerPageIndex = 0;

  if (readerIsEpub) readerLoadEpubChapter(0, false);
  else lv_obj_scroll_to_y(readerContent, 0, LV_ANIM_OFF);

  markReaderProgressDirty();
  saveReaderProgressNow();
}

static void readerTapEvent(lv_event_t* e) {
  if (!readerIsEpub || lv_event_get_code(e) != LV_EVENT_PRESSED) return;
  lv_indev_t* indev = lv_indev_active();
  if (!indev || !readerTapLayer) return;

  lv_point_t p;
  lv_indev_get_point(indev, &p);
  const int32_t pageX = lv_obj_get_x(readerTapLayer);
  const int32_t pageW = lv_obj_get_width(readerTapLayer);
  const int32_t localX = static_cast<int32_t>(p.x) - pageX;

  if (localX < pageW / 2) readerGoPrevPage();
  else readerGoNextPage();

  lv_indev_wait_release(indev);
}

static void startReader(const String& path) {
  if (!pathStorageReady(path) || (!isEpubExtension(path) && !isReadableTextFile(path))) return;
  // A reader opened directly from the dock must suspend the hidden map too.
  if (activePage == Page::Map) showPage(Page::Home);
  setKeyboardVisible(false);
  readerPath = path;
  readerIsEpub = isEpubExtension(path);
  readerChapter = 0;
  readerBook.clear();
  readerRichDoc.clear();
  readerResumePending = false;
  readerResumeHasRichLoc = false;
  readerResumeBlock = 0;

  if (readerIsEpub) {
    String err;
    if (!epub_lite::buildBook(tab5FS, path.c_str(), readerBook, err)) {
      toast(err.c_str(), 4200);
      readerPath = "";
      return;
    }
    readerPageAspect = readerDetectBookAspect();
    uint16_t savedChapter = 0;
    uint16_t savedBlock = 0;
    uint32_t savedOffset = 0;
    int32_t savedScroll = 0;
    bool savedRich = false;
    if (loadReaderProgress(path, savedChapter, savedBlock, savedOffset, savedScroll, savedRich) &&
        savedChapter < readerBook.chapterCount) {
      readerChapter = savedChapter;
      readerResumeChapter = savedChapter;
      readerResumeBlock = savedBlock;
      readerResumeOffset = savedOffset;
      readerResumeHasRichLoc = savedRich;
      readerResumePending = true;
    }
  } else {
    uint16_t savedChapter = 0;
    uint16_t savedBlock = 0;
    uint32_t savedOffset = 0;
    int32_t savedScroll = 0;
    bool savedRich = false;
    if (loadReaderProgress(path, savedChapter, savedBlock, savedOffset, savedScroll, savedRich)) {
      readerResumeScrollY = savedScroll;
      readerResumePending = true;
    }
  }
  if (lastReaderPath != path) {
    lastReaderPath = path;
    markSettingsDirty();
  }
  readerProgressDirty = false;

  // Reader font/cache is intentionally lazy to keep normal program startup fast.
  ensureReaderFont();
#if LV_USE_TINY_TTF
  if(readerTtfFont){ resizeConfiguredFont(readerTtfFont,readerStockFont,fontSizeForIndex(settings.fontIndex)); }
#endif

  readerActive = true;
  M5.Display.setRotation(Tab5DisplayOrientation::rotation(true, settings.screenUpsideDown));
  const int rw = M5.Display.width();
  const int rh = M5.Display.height();
  lv_display_set_resolution(lvDisplay, rw, rh);

  readerScreen = lv_obj_create(nullptr);
  lv_obj_set_size(readerScreen, rw, rh);
  lv_obj_set_style_bg_color(readerScreen, C_BG(), 0);Tab5ThemeLive::bind(readerScreen,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(readerScreen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(readerScreen, C_TEXT(), 0);Tab5ThemeLive::bind(readerScreen,LV_STYLE_TEXT_COLOR,0,4);
  lv_obj_set_style_text_font(readerScreen, uiFont(), 0);
  lv_obj_set_style_pad_all(readerScreen, 0, 0);
  lv_obj_remove_flag(readerScreen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* header = lv_obj_create(readerScreen);
  lv_obj_set_size(header, rw, 58);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_bg_color(header, C_SURFACE(), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header, 1, 0);
  lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(header, C_BORDER(), 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* back = makeButton(header, LV_SYMBOL_LEFT, 54, 46);
  lv_obj_set_pos(back, 6, 6);
  lv_obj_add_event_cb(back, readerBackEvent, LV_EVENT_CLICKED, nullptr);

  readerRestartBtn = makeIconButton(header, LV_SYMBOL_REFRESH, 50, 46);
  lv_obj_align(readerRestartBtn, LV_ALIGN_RIGHT_MID, -82, 0);
  lv_obj_add_event_cb(readerRestartBtn, readerRestartEvent, LV_EVENT_CLICKED, nullptr);

  readerTitleLabel = lv_label_create(header);
  lv_obj_set_width(readerTitleLabel, rw - 260);
  lv_label_set_long_mode(readerTitleLabel, LV_LABEL_LONG_DOT);
  lv_obj_align(readerTitleLabel, LV_ALIGN_LEFT_MID, 72, 0);

  readerClockLabel = lv_label_create(header);
  lv_obj_align(readerClockLabel, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_set_style_text_color(readerClockLabel, C_MUTED(), 0);

  readerContent = lv_obj_create(readerScreen);
  lv_obj_set_size(readerContent, rw, rh - 94);
  lv_obj_set_pos(readerContent, 0, 58);
  lv_obj_set_style_bg_color(readerContent, C_BG(), 0);Tab5ThemeLive::bind(readerContent,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(readerContent, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(readerContent, 0, 0);
  lv_obj_set_style_radius(readerContent, 0, 0);
  lv_obj_set_style_pad_left(readerContent, 26, 0);
  lv_obj_set_style_pad_right(readerContent, 26, 0);
  lv_obj_set_style_pad_top(readerContent, 22, 0);
  lv_obj_set_style_pad_bottom(readerContent, 22, 0);
  lv_obj_remove_flag(readerContent, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(readerContent, readerPlainScrollEvent, LV_EVENT_SCROLL_END, nullptr);

  if (readerIsEpub) {
    lv_obj_remove_flag(readerContent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(readerContent, LV_SCROLLBAR_MODE_OFF);
  } else {
    lv_obj_add_flag(readerContent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(readerContent, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(readerContent, LV_SCROLLBAR_MODE_AUTO);
  }

  readerTextLabel = lv_label_create(readerContent);
  lv_obj_set_width(readerTextLabel, rw - 52);
  if (readerIsEpub) lv_obj_set_height(readerTextLabel, rh - 138);
  lv_label_set_long_mode(readerTextLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(readerTextLabel, readerFont(), 0);
  lv_obj_set_style_text_color(readerTextLabel, C_TEXT(), 0);
  lv_obj_set_style_text_line_space(readerTextLabel, 8, 0);
  lv_obj_align(readerTextLabel, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_label_set_text(readerTextLabel, "");

  readerRichHost = lv_obj_create(readerContent);
  lv_obj_remove_style_all(readerRichHost);
  lv_obj_set_size(readerRichHost, rw - 52, rh - 138);
  lv_obj_set_pos(readerRichHost, 0, 0);
  lv_obj_remove_flag(readerRichHost, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(readerRichHost, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(readerRichHost, LV_OBJ_FLAG_HIDDEN);

  // Off-screen measurement surface. LVGL still lays it out, but it is never
  // visible. Pagination and rendering therefore use the exact same span/table
  // widgets and font metrics.
  readerMeasureHost = lv_obj_create(readerScreen);
  lv_obj_remove_style_all(readerMeasureHost);
  lv_obj_set_size(readerMeasureHost, rw - 52, rh - 138);
  lv_obj_set_pos(readerMeasureHost, -2000, 60);
  lv_obj_remove_flag(readerMeasureHost, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(readerMeasureHost, LV_OBJ_FLAG_CLICKABLE);

  readerImageObj = lv_image_create(readerContent);
  lv_obj_add_flag(readerImageObj, LV_OBJ_FLAG_HIDDEN);
  lv_obj_center(readerImageObj);

  readerTapLayer = nullptr;
  if (readerIsEpub) {
    readerTapLayer = lv_obj_create(readerScreen);
    lv_obj_remove_style_all(readerTapLayer);
    lv_obj_set_size(readerTapLayer, rw, rh - 94);
    lv_obj_set_pos(readerTapLayer, 0, 58);
    lv_obj_set_style_bg_opa(readerTapLayer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(readerTapLayer, 0, 0);
    lv_obj_add_flag(readerTapLayer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(readerTapLayer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(readerTapLayer, readerTapEvent, LV_EVENT_PRESSED, nullptr);
  }

  readerStatusLabel = lv_label_create(readerScreen);
  lv_obj_set_width(readerStatusLabel, rw - 24);
  lv_obj_align(readerStatusLabel, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_style_text_color(readerStatusLabel, C_MUTED(), 0);
  lv_obj_set_style_text_align(readerStatusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(readerStatusLabel, LV_LABEL_LONG_DOT);
  if (readerIsEpub) {
    readerSeekSlider = lv_slider_create(readerScreen);
    lv_obj_set_size(readerSeekSlider, rw - 72, 12);
    lv_obj_align(readerSeekSlider, LV_ALIGN_BOTTOM_MID, 0, -49);
    lv_obj_set_ext_click_area(readerSeekSlider, 14);
    Tab5UiStyle::control(readerSeekSlider);
    lv_slider_set_range(readerSeekSlider, 1, 1);
    lv_obj_add_state(readerSeekSlider, LV_STATE_DISABLED);
    Tab5Focus.describe(readerSeekSlider, T("page"));
    lv_obj_add_event_cb(readerSeekSlider, readerSeekEvent, LV_EVENT_ALL, nullptr);
  }

  if (readerIsEpub) readerApplyEpubGeometry(readerPageAspect);

  lv_screen_load(readerScreen);
  // LVGL layout is lazy. Force it now so first-page pagination and image sizing
  // use the real 720x1280 portrait content dimensions instead of stale/zero values.
  lv_obj_update_layout(readerScreen);
  updateClockLabels(true);
  if(readerIsEpub)readerBookIndexStart();
  readerLoadCurrent();
}

static void stopReader() {
  if (!readerActive) return;
  // Force the latest position to SD when leaving the reader.
  saveReaderProgressNow();
  readerBookIndexStop();
  freeReaderText();
  readerRichDoc.clear();
  readerBook.clear();
  readerActive = false;
  readerPageStarts.clear();
  readerRichPageStarts.clear();
  readerPageStartCount = readerRichPageStartCount = 0;
  readerPath = "";

  M5.Display.setRotation(Tab5DisplayOrientation::rotation(false, settings.screenUpsideDown));
  lv_display_set_resolution(lvDisplay, SCREEN_W, SCREEN_H);
  lv_screen_load(root);
  lv_obj_invalidate(root);

  lv_obj_t* old = readerScreen;
  readerScreen = nullptr;
  readerContent = nullptr;
  readerTapLayer = nullptr;
  readerImageObj = nullptr;
  readerRichHost = nullptr;
  readerMeasureHost = nullptr;
  readerTextLabel = readerTitleLabel = readerStatusLabel = readerClockLabel = nullptr;
  readerRestartBtn = nullptr;
  readerSeekSlider = nullptr; readerSeekDragging = false;
  readerResumePending = false;
  readerResumeHasRichLoc = false;
  readerProgressDirty = false;
  if (old && lv_obj_is_valid(old)) lv_obj_delete(old);
  updateClockLabels(true);
}

// -----------------------------------------------------------------------------
// UI construction
// -----------------------------------------------------------------------------

static const HomeCardDef HOME_CARDS[] = {
  {HomeAction::Files,      "Explorateur"},
  {HomeAction::Editor,     "Éditeur"},
  {HomeAction::Reader,     "eBook"},
  {HomeAction::Audio,      "Média"},
  {HomeAction::Map,        "Cartes"},
  {HomeAction::Calculator, "Calculatrice"},
  {HomeAction::Settings,   "Réglages"},
  {HomeAction::Terminal, "Terminal"},
  {HomeAction::Alarm, "Réveil"},
  {HomeAction::Sdr, "RTL-SDR"},
  {HomeAction::ImageEditor, "Dessin"},
  {HomeAction::Meshtastic, "Meshtastic"},
};

static const char* HOME_IDS[]={"files","editor","reader","audio","map","calculator","settings","terminal","alarm","sdr","image-editor","meshtastic"};
static const uint8_t HOME_SYMBOLS[]={1,2,10,3,4,11,5,12,16,3,7,15};


static void openHomeAction(HomeAction action) {
  Tab5ContextMenu::close();
  dockLaunchedKey=0;
  activeTool = action;
  switch (action) {
    case HomeAction::Meshtastic: setKeyboardVisible(false);showPage(Page::Home);Tab5Meshtastic::open();break;
    case HomeAction::Sdr: setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Sdr);break;
    case HomeAction::Storage: showPage(Page::Settings);showSettingsSection(3);break;
    case HomeAction::Pentest: setKeyboardVisible(false);Tab5Tools.launchExecutable("/Applications/Pentest.app.elf");break;
    case HomeAction::Alarm: setKeyboardVisible(false);Tab5Alarm::open();break;
    case HomeAction::Theme: setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Theme);break;
    case HomeAction::Terminal: setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Terminal);break;
    case HomeAction::NativeApps: homeOpenMenu(1);break;
    case HomeAction::Home:     showPythonLibrary(false);showPage(Page::Home); break;
    case HomeAction::Files:    showPage(Page::Files); break;
    case HomeAction::Editor:   showPage(Page::Writing); break;
    case HomeAction::ImageEditor: openImageEditor();break;
    case HomeAction::Reader:
      if (sdReady && lastReaderPath.length() && tab5FS.exists(lastReaderPath.c_str()) &&
          hasSupportedExtension(lastReaderPath)) {
        startReader(lastReaderPath);
      } else {
        if (lastReaderPath.length()) {
          lastReaderPath = "";
          markSettingsDirty();
        }
        showPage(Page::Files);
        toast("Aucune lecture récente — choisissez un document", 2800);
      }
      break;
    case HomeAction::Settings: showPage(Page::Settings); break;
    case HomeAction::Audio:    showPage(Page::Audio); break;
    case HomeAction::Map:      showPage(Page::Map); break;
    case HomeAction::Calculator:
      setKeyboardVisible(false);keyboardDockFocus=false;keyboardUiNavigation=true;
      Tab5Tools.open(Tab5Tool::Calculator);
      Tab5Focus.focusInitial(Tab5Tools.scope());break;
    case HomeAction::Python:
      showPage(Page::Home);showPythonLibrary(true);break;
  }
}


#include "Tab5HomeUi.h"
#include "Tab5ShellDock.h"

static void editorFileMenu(lv_event_t*){
  Tab5ContextMenu::Item items[]={
    {"Enregistrer","Ctrl+S",[](void*){saveEvent(nullptr);}},
    {"Enregistrer sous","Ctrl+Shift+S",[](void*){saveAsEvent(nullptr);}},
  };
  lv_area_t area;lv_obj_get_coords(saveBtn,&area);Tab5ContextMenu::show(saveBtn,area.x2,area.y2+4,items,sizeof(items)/sizeof(items[0]));
}

static void createTopBar() {
  topBar = lv_obj_create(root);
  lv_obj_set_size(topBar, SCREEN_W, TOP_H);
  lv_obj_set_pos(topBar, 0, 0);
  lv_obj_set_style_radius(topBar, 0, 0);
  lv_obj_set_style_pad_all(topBar, 0, 0);
  lv_obj_set_style_bg_color(topBar, C_SURFACE(), 0);Tab5ThemeLive::bind(topBar,LV_STYLE_BG_COLOR,0,1);
  lv_obj_set_style_bg_opa(topBar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(topBar, 1, 0);
  lv_obj_set_style_border_side(topBar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(topBar, C_BORDER(), 0);Tab5ThemeLive::bind(topBar,LV_STYLE_BORDER_COLOR,0,3);
  lv_obj_remove_flag(topBar, LV_OBJ_FLAG_SCROLLABLE);

  topHomeButton=makeIconButton(topBar,LV_SYMBOL_HOME,52,40);lv_obj_set_pos(topHomeButton,6,4);
  Tab5Focus.describe(topHomeButton,T("Accueil · Alt+1"));
  lv_obj_add_event_cb(topHomeButton,[](lv_event_t*){openHomeAction(HomeAction::Home);},LV_EVENT_CLICKED,nullptr);
  topPythonResume=makeIconButton(topBar,LV_SYMBOL_PLAY,56,40);
  lv_obj_set_pos(topPythonResume,18,4);
  lv_obj_set_style_border_color(topPythonResume,C_ACTIVE(),0);
  lv_obj_add_flag(topPythonResume,LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(topPythonResume,[](lv_event_t*){resumePythonApp();},LV_EVENT_CLICKED,nullptr);

  auto* meshBadge=Tab5MeshBadge::create(topBar,[](lv_event_t*){openHomeAction(HomeAction::Meshtastic);});
  lv_obj_set_pos(meshBadge,80,6);
  Tab5MeshBadge::update(false,false,0,T("Meshtastic"));

  // These two were previously arrow glyphs and could disappear with some TTFs.
  undoBtn = makeButton(topBar, "Undo", 64, 40);
  lv_obj_set_pos(undoBtn, 556, 4);
  lv_obj_add_event_cb(undoBtn, undoEvent, LV_EVENT_CLICKED, nullptr);

  redoBtn = makeButton(topBar, "Redo", 64, 40);
  lv_obj_set_pos(redoBtn, 624, 4);
  lv_obj_add_event_cb(redoBtn, redoEvent, LV_EVENT_CLICKED, nullptr);

  editorCutBtn = makeIconButton(topBar, LV_SYMBOL_CUT, 50, 40);
  lv_obj_set_pos(editorCutBtn, 692, 4);
  lv_obj_add_event_cb(editorCutBtn, editorCutEvent, LV_EVENT_CLICKED, nullptr);

  editorCopyBtn = makeIconButton(topBar, LV_SYMBOL_COPY, 50, 40);
  lv_obj_set_pos(editorCopyBtn, 746, 4);
  lv_obj_add_event_cb(editorCopyBtn, editorCopyEvent, LV_EVENT_CLICKED, nullptr);

  editorPasteBtn = makeIconButton(topBar, LV_SYMBOL_PASTE, 50, 40);
  lv_obj_set_pos(editorPasteBtn, 800, 4);
  lv_obj_add_event_cb(editorPasteBtn, editorPasteEvent, LV_EVENT_CLICKED, nullptr);

  titleLabel = lv_label_create(topBar);
  lv_obj_set_width(titleLabel, 420);
  lv_label_set_long_mode(titleLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(titleLabel, C_TEXT(), 0);
  lv_obj_align(titleLabel, LV_ALIGN_LEFT_MID, 112, 0);

  dirtyLabel = lv_label_create(topBar);
  lv_label_set_text(dirtyLabel, "");
  lv_obj_set_style_text_color(dirtyLabel, C_MUTED(), 0);
  lv_obj_align(dirtyLabel, LV_ALIGN_LEFT_MID, 546, 0);

  topAudioIndicator = lv_label_create(topBar);
  lv_obj_set_size(topAudioIndicator, 398, 30);
  lv_obj_set_style_pad_all(topAudioIndicator, 0, 0);
  lv_label_set_long_mode(topAudioIndicator, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(topAudioIndicator, C_MUTED(), 0);
  lv_obj_set_style_text_font(topAudioIndicator, uiFont(), 0);
  lv_obj_align(topAudioIndicator, LV_ALIGN_LEFT_MID, 550, 0);
  lv_obj_add_flag(topAudioIndicator, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(topAudioIndicator, topAudioIndicatorEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_add_flag(topAudioIndicator, LV_OBJ_FLAG_HIDDEN);

  batteryLabel = lv_label_create(topBar);
  lv_obj_set_size(batteryLabel, 92, 28);
  lv_obj_set_style_text_align(batteryLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(batteryLabel, C_MUTED(), 0);
  lv_obj_set_style_text_font(batteryLabel, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_line_space(batteryLabel, -2, 0);
  lv_obj_align(batteryLabel, LV_ALIGN_RIGHT_MID, -4, 0);

  topWifiLabel = lv_label_create(topBar);
  lv_obj_set_width(topWifiLabel, 28);
  lv_obj_set_style_text_align(topWifiLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(topWifiLabel, C_MUTED(), 0);
  lv_obj_set_style_text_font(topWifiLabel, &lv_font_montserrat_20, 0);
  lv_obj_align(topWifiLabel, LV_ALIGN_RIGHT_MID, -104, 0);

  dateLabel = lv_label_create(topBar);
  lv_obj_set_width(dateLabel, 112);
  lv_label_set_long_mode(dateLabel, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(dateLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(dateLabel, C_MUTED(), 0);
  lv_obj_set_style_text_font(dateLabel, uiFont(), 0);
  lv_obj_align(dateLabel, LV_ALIGN_RIGHT_MID, -214, 0);

  clockLabel = lv_label_create(topBar);
  lv_obj_set_width(clockLabel, 70);
  lv_obj_set_style_text_align(clockLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(clockLabel, C_TEXT(), 0);
  lv_obj_set_style_text_font(clockLabel, &lv_font_montserrat_24, 0);
  lv_obj_align(clockLabel, LV_ALIGN_RIGHT_MID, -138, 0);

  keyboardBtn = makeIconButton(topBar, LV_SYMBOL_KEYBOARD, 58, 40);
  lv_obj_align(keyboardBtn, LV_ALIGN_RIGHT_MID, -72, 0);
  lv_obj_add_event_cb(keyboardBtn, keyboardToggleEvent, LV_EVENT_CLICKED, nullptr);

  saveBtn = makeIconButton(topBar, LV_SYMBOL_SAVE " " LV_SYMBOL_DOWN, 58, 40);
  Tab5Focus.describe(saveBtn,T("Fichier"));
  lv_obj_align(saveBtn, LV_ALIGN_RIGHT_MID, -8, 0);
  lv_obj_add_event_cb(saveBtn, editorFileMenu, LV_EVENT_CLICKED, nullptr);
  updateUndoRedoState();
  updateEditorClipboardState();
  updateClockLabels(true);
  shellTopLayout();
}

static void createDock() {
  dock = lv_obj_create(root);
  lv_obj_set_size(dock, DOCK_W, PAGE_H);
  lv_obj_set_pos(dock, 0, TOP_H);
  lv_obj_set_style_radius(dock, 0, 0);
  lv_obj_set_style_border_width(dock, 0, 0);
  lv_obj_set_style_pad_all(dock, 0, 0);
  lv_obj_set_style_bg_color(dock, C_SURFACE(), 0);Tab5ThemeLive::bind(dock,LV_STYLE_BG_COLOR,0,1);
  lv_obj_set_style_bg_opa(dock, LV_OPA_COVER, 0);
  lv_obj_remove_flag(dock, LV_OBJ_FLAG_SCROLLABLE);

  shellRefreshDock();
  setTopActionVisible(dock,Tab5Shell::choices.dockWidth()!=0);
}

static void fileSearchEvent(lv_event_t*){openTextDialog(TextDialogAction::FileSearch,"Rechercher fichiers et dossiers",fileSearchQuery,fileContextDirectory());}
static void audioSearchEvent(lv_event_t*){openTextDialog(TextDialogAction::AudioSearch,audioView==AudioView::Radio?"Rechercher une radio":"Rechercher un titre",audioSearchQueries[unsigned(audioView)],"");}
static lv_obj_t* makeSearchButton(lv_obj_t* parent,int width=44,int height=36){
  auto* button=makeButton(parent,"",width,height);
  auto* ring=lv_obj_create(button);lv_obj_remove_style_all(ring);lv_obj_set_size(ring,16,16);lv_obj_set_pos(ring,10,7);
  lv_obj_set_style_radius(ring,LV_RADIUS_CIRCLE,0);lv_obj_set_style_border_width(ring,2,0);lv_obj_set_style_border_color(ring,C_TEXT(),0);
  lv_obj_remove_flag(ring,static_cast<lv_obj_flag_t>(lv_obj_flag_t(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE)));
  static const lv_point_precise_t points[]={{0,0},{9,9}};auto* handle=lv_line_create(button);lv_line_set_points(handle,points,2);
  lv_obj_set_pos(handle,23,20);lv_obj_set_style_line_width(handle,2,0);lv_obj_set_style_line_color(handle,C_TEXT(),0);
  lv_obj_set_style_pad_all(button,0,0);return button;
}
static lv_obj_t* makeOpenWithButton(lv_obj_t* parent,int width,int height){
  // A document leading to an application grid; independent of the UI font.
  auto* button=makeButton(parent,"",width,height);lv_obj_set_style_pad_all(button,0,0);
  static const lv_point_precise_t file[]={{0,0},{12,0},{18,6},{18,28},{0,28},{0,0}};
  static const lv_point_precise_t fold[]={{12,0},{12,6},{18,6}};
  static const lv_point_precise_t arrow[]={{0,5},{14,5},{9,0},{14,5},{9,10}};
  const lv_point_precise_t* drawings[]={file,fold,arrow};const unsigned counts[]={6,3,5};
  for(unsigned i=0;i<3;++i){auto* line=lv_line_create(button);lv_line_set_points(line,drawings[i],counts[i]);
    lv_obj_set_pos(line,i==2?24:7,i==2?17:9);lv_obj_set_style_line_width(line,2,0);lv_obj_set_style_line_color(line,C_TEXT(),0);
    lv_obj_remove_flag(line,LV_OBJ_FLAG_CLICKABLE);
  }
  for(unsigned i=0;i<4;++i){auto* square=lv_obj_create(button);lv_obj_remove_style_all(square);
    lv_obj_set_size(square,5,5);lv_obj_set_pos(square,42+int(i%2)*7,16+int(i/2)*7);
    lv_obj_set_style_bg_opa(square,LV_OPA_COVER,0);lv_obj_set_style_bg_color(square,C_TEXT(),0);
    lv_obj_remove_flag(square,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
  }
  return button;
}

#include "Tab5ExplorerMenus.inc"
static lv_obj_t* makeParentFolderButton(lv_obj_t* parent) {
  auto* button=makeButton(parent,"",52,46);lv_obj_set_style_pad_all(button,0,0);
  // Folder outline with an upward arrow: recognizable without a text glyph.
  static const lv_point_precise_t folder[]={{0,6},{0,0},{11,0},{16,6},{34,6},{34,29},{0,29},{0,6}};
  static const lv_point_precise_t arrow[]={{17,24},{17,11},{11,17},{17,11},{23,17}};
  for(unsigned i=0;i<2;++i){auto* line=lv_line_create(button);lv_line_set_points(line,i?arrow:folder,i?5:8);
    lv_obj_set_pos(line,9,8);lv_obj_set_style_line_width(line,2,0);lv_obj_set_style_line_color(line,C_TEXT(),0);lv_obj_remove_flag(line,LV_OBJ_FLAG_CLICKABLE);}
  Tab5Focus.describe(button,T("Dossier parent · Retour arrière"));lv_obj_add_event_cb(button,upFolderEvent,LV_EVENT_CLICKED,nullptr);
  return button;
}
static lv_obj_t* makeSortButton(lv_obj_t* parent){
  auto* button=makeButton(parent,"",52,46);lv_obj_set_style_pad_all(button,0,0);
  static const lv_point_precise_t top[]={{0,0},{20,0}};
  static const lv_point_precise_t middle[]={{0,0},{14,0}};
  static const lv_point_precise_t bottom[]={{0,0},{8,0}};
  static const lv_point_precise_t arrow[]={{0,0},{0,24},{-5,19},{0,24},{5,19}};
  const lv_point_precise_t* shapes[]={top,middle,bottom,arrow};
  for(unsigned i=0;i<4;++i){auto* line=lv_line_create(button);lv_line_set_points(line,shapes[i],i==3?5:2);
    lv_obj_set_pos(line,i==3?38:8,i==3?10:10+int(i)*10);lv_obj_set_style_line_width(line,2,0);
    lv_obj_set_style_line_color(line,C_TEXT(),0);lv_obj_remove_flag(line,LV_OBJ_FLAG_CLICKABLE);}
  Tab5Focus.describe(button,T("Trier par"));return button;
}
static void createFileToolbar(lv_obj_t* parent) {
  auto* bar=lv_obj_create(parent);lv_obj_remove_style_all(bar);lv_obj_set_pos(bar,0,0);
  lv_obj_set_size(bar,lv_pct(100),50);lv_obj_set_flex_flow(bar,LV_FLEX_FLOW_ROW);lv_obj_set_style_pad_column(bar,8,0);lv_obj_remove_flag(bar,LV_OBJ_FLAG_SCROLLABLE);
  fileParentButton=makeParentFolderButton(bar);
  auto menu=[&](const char* text,FileMenu which){
    lv_point_t measured;lv_text_get_size(&measured,T(text),uiFont(),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
    auto* b=makeButton(bar,text,138,46);lv_obj_set_width(b,0);
    lv_obj_set_flex_grow(b,static_cast<uint8_t>(std::min(255,int(measured.x)+36)));
    lv_obj_set_flex_flow(b,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_hor(b,6,0);lv_obj_set_style_pad_ver(b,2,0);lv_obj_set_style_pad_column(b,6,0);
    auto* label=lv_obj_get_child(b,0);lv_obj_set_width(label,0);lv_obj_set_flex_grow(label,1);
    lv_obj_set_height(label,lv_font_get_line_height(uiFont()));lv_label_set_long_mode(label,LV_LABEL_LONG_DOT);
    auto* arrow=lv_label_create(b);lv_label_set_text(arrow,LV_SYMBOL_DOWN);lv_obj_set_style_text_font(arrow,&lv_font_montserrat_14,0);lv_obj_set_width(arrow,14);
    lv_obj_add_event_cb(b,fileMenuEvent,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(which)));return b;
  };
  menu("Fichier",FileMenu::Create);menu("Édition",FileMenu::Edit);
  fileActionsMenuBtn=menu("Actions",FileMenu::Actions);menu("Affichage",FileMenu::View);
  auto* sort=makeSortButton(bar);
  lv_obj_add_event_cb(sort,fileMenuEvent,LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(FileMenu::Sort)));
  fileUsbButton=nullptr;
  fileSearchClear=makeSearchButton(bar,52,46);Tab5Focus.describe(fileSearchClear,T("Rechercher fichiers et dossiers"));
  lv_obj_add_event_cb(fileSearchClear,fileSearchEvent,LV_EVENT_CLICKED,nullptr);
}

#include "Tab5ExplorerUi.inc"

static void createEditorPage() {
  editorPage = lv_obj_create(pageHost);
  lv_obj_set_size(editorPage, WORK_W, PAGE_H);Tab5Shell::width(editorPage,0);
  lv_obj_set_pos(editorPage, 0, 0);
  lv_obj_set_style_border_width(editorPage, 0, 0);
  lv_obj_set_style_radius(editorPage, 0, 0);
  lv_obj_set_style_pad_all(editorPage, 0, 0);
  lv_obj_set_style_bg_color(editorPage, C_BG(), 0);Tab5ThemeLive::bind(editorPage,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(editorPage, LV_OPA_COVER, 0);
  lv_obj_remove_flag(editorPage, LV_OBJ_FLAG_SCROLLABLE);

  editor = lv_textarea_create(editorPage);
  lv_obj_set_size(editor, WORK_W, PAGE_H);Tab5Shell::width(editor,0);
  lv_obj_set_pos(editor, 0, 0);
  lv_textarea_set_placeholder_text(editor, T("Commencez à écrire…"));
  lv_textarea_set_one_line(editor, false);
  lv_textarea_set_cursor_click_pos(editor, true);
  lv_textarea_set_text_selection(editor, true);
  lv_textarea_set_max_length(editor, MAX_DOCUMENT_BYTES);
  lv_obj_set_style_radius(editor, 0, 0);
  lv_obj_set_style_border_width(editor, 0, 0);
  lv_obj_set_style_bg_color(editor, C_BG(), 0);Tab5ThemeLive::bind(editor,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(editor, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(editor, C_TEXT(), LV_PART_MAIN);
  lv_obj_set_style_text_letter_space(editor, 0, LV_PART_MAIN);
  lv_obj_set_style_text_line_space(editor, 5, LV_PART_MAIN);
  lv_obj_set_style_text_color(editor, C_CURSOR(), LV_PART_CURSOR);
  lv_obj_set_style_bg_color(editor, C_ACTIVE(), LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(editor, LV_OPA_COVER, LV_PART_SELECTED);
  lv_obj_set_style_text_color(editor, C_CURSOR(), LV_PART_SELECTED);
#if LVGL_VERSION_MAJOR > 9 || (LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 4)
  lv_obj_set_style_text_outline_stroke_width(editor, 0, LV_PART_MAIN);
#endif
  lv_obj_set_style_outline_width(editor, 0, LV_STATE_FOCUSED);
  lv_obj_set_style_pad_left(editor, 24, 0);
  lv_obj_set_style_pad_right(editor, 24, 0);
  lv_obj_set_style_pad_top(editor, 18, 0);
  lv_obj_set_style_pad_bottom(editor, 18, 0);
  lv_obj_add_event_cb(editor, editorEvent, LV_EVENT_ALL, nullptr);

  editorStatus = nullptr;  // Statistics are shown in Preview / file metadata, not under the keyboard.
}

static void createPreviewPage() {
  previewPage = lv_obj_create(pageHost);
  lv_obj_set_size(previewPage, WORK_W, PAGE_H);Tab5Shell::width(previewPage,0);
  lv_obj_set_pos(previewPage, 0, 0);
  lv_obj_set_style_border_width(previewPage, 0, 0);
  lv_obj_set_style_radius(previewPage, 0, 0);
  lv_obj_set_style_pad_all(previewPage, 0, 0);
  lv_obj_set_style_bg_color(previewPage, C_BG(), 0);Tab5ThemeLive::bind(previewPage,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(previewPage, LV_OPA_COVER, 0);
  lv_obj_remove_flag(previewPage, LV_OBJ_FLAG_SCROLLABLE);

  previewHint = lv_label_create(previewPage);
  lv_label_set_text(previewHint, T("Aperçu texte"));
  lv_obj_set_width(previewHint, WORK_W - 240);Tab5Shell::width(previewHint,-240);
  lv_label_set_long_mode(previewHint, LV_LABEL_LONG_DOT);
  lv_obj_align(previewHint, LV_ALIGN_TOP_LEFT, 20, 14);
  lv_obj_set_style_text_color(previewHint, C_MUTED(), 0);

  lv_obj_t* read = makeIconButton(previewPage, LV_SYMBOL_EYE_OPEN, 62, 44);
  lv_obj_align(read, LV_ALIGN_TOP_RIGHT, -14, 7);
  lv_obj_add_event_cb(read, previewReadEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* scroll = lv_obj_create(previewPage);
  lv_obj_set_size(scroll, WORK_W - 24, PAGE_H - 62);Tab5Shell::width(scroll,-24);
  lv_obj_align(scroll, LV_ALIGN_BOTTOM_MID, 0, -2);
  lv_obj_set_style_pad_all(scroll, 24, 0);
  lv_obj_set_style_bg_color(scroll, C_BG(), 0);
  lv_obj_set_style_bg_opa(scroll, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(scroll, 0, 0);

  previewLabel = lv_label_create(scroll);
  lv_obj_set_width(previewLabel, WORK_W - 90);Tab5Shell::width(previewLabel,-90);
  lv_label_set_long_mode(previewLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(previewLabel, "");
  lv_obj_align(previewLabel, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_set_style_text_color(previewLabel, C_TEXT(), 0);
}

static lv_obj_t* makeSettingsCard(lv_obj_t* parent, int x, int y, int width,
                                  int height, const char* title) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_size(card, width, height);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_style_bg_color(card, C_SURFACE(), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(card, C_BORDER(), 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_radius(card, 12, 0);Tab5UiStyle::control(card);
  lv_obj_set_style_pad_all(card, 0, 0);
  lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* heading = lv_label_create(card);
  lv_label_set_text(heading, T(title));
  lv_obj_set_style_text_font(heading, uiHeadingFont(), 0);
  lv_obj_set_style_text_color(heading, C_TEXT(), 0);
  lv_obj_set_pos(heading, 20, 14);
  return card;
}

static lv_obj_t* makeSettingsText(lv_obj_t* parent, const char* text,
                                  int x, int y, int width, bool muted) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, T(text));
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(label, muted ? C_MUTED() : C_TEXT(), 0);
  return label;
}

static void styleSettingsSlider(lv_obj_t* slider) {
  lv_obj_set_style_bg_color(slider, C_RAISED(), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, C_ACTIVE(), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, C_TEXT(), LV_PART_KNOB);
}

static void styleSettingsSwitch(lv_obj_t* sw) {
  lv_obj_remove_style_all(sw);
  lv_obj_set_size(sw, 74, 38);
  lv_obj_set_ext_click_area(sw,5); // 48 px effective touch height.
  lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw, C_RAISED(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw, C_ACTIVE(), LV_PART_MAIN | LV_STATE_CHECKED);
  lv_obj_set_style_border_width(sw, 1, LV_PART_MAIN);
  lv_obj_set_style_border_color(sw, C_BORDER(), LV_PART_MAIN);
  lv_obj_set_style_pad_all(sw, 4, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(sw, 0, LV_PART_MAIN);

  lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(sw, LV_OPA_TRANSP, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sw, C_ACTIVE(), LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);

  lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);
  lv_obj_set_style_bg_color(sw, C_MUTED(), LV_PART_KNOB);
  lv_obj_set_style_bg_color(sw, C_TEXT(), LV_PART_KNOB | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_KNOB);
  lv_obj_set_style_shadow_width(sw, 0, LV_PART_KNOB);
  Tab5UiStyle::control(sw);
}

static void styleSettingsDropdown(lv_obj_t* dropdown) {
  lv_obj_set_style_bg_color(dropdown, C_RAISED(), LV_PART_MAIN);
  lv_obj_set_style_text_color(dropdown, C_TEXT(), LV_PART_MAIN);
  lv_obj_set_style_border_color(dropdown, C_BORDER(), LV_PART_MAIN);
  lv_obj_set_style_text_font(dropdown, uiFont(), LV_PART_MAIN);
}

// -----------------------------------------------------------------------------
// Audio page
// -----------------------------------------------------------------------------

static bool audioPathIsRecording(const String& path) {
  return path == "/Recordings" || path.startsWith("/Recordings/");
}

static bool audioPathIsSeekable(const String& path) {
  String lower = path;
  lower.toLowerCase();
  return lower.endsWith(".mp3") || lower.endsWith(".wav") ||
         lower.endsWith(".flac");
}

static String audioAlbumName(const String& folder) {
  if (folder == "/Audio") return T("Sans sous-dossier  •  /Audio");
  if (folder == "/mp3") return T("Sans sous-dossier  •  /mp3");
  String result = folder;
  if (result.startsWith("/Audio/")) result.remove(0, 7);
  else if (result.startsWith("/mp3/")) result.remove(0, 5);
  result.replace("/", "  /  ");
  return result.length() ? result : folder;
}

static String audioStationCategory(const Tab5AudioStation& station) {
  return station.style.length() ? station.style : String("Autres");
}

static void addUniqueAudioGroup(std::vector<String>& groups,
                                const String& value) {
  for (const String& existing : groups) {
    if (existing == value) return;
  }
  groups.push_back(value);
}

static void sortAudioGroups(std::vector<String>& groups) {
  std::sort(groups.begin(), groups.end(), [](const String& a, const String& b) {
    String lowerA = a;
    String lowerB = b;
    lowerA.toLowerCase();
    lowerB.toLowerCase();
    return lowerA.compareTo(lowerB) < 0;
  });
}

static void setAudioTabState(lv_obj_t* tab, bool active) {
  if (!tab) return;
  if (active) lv_obj_add_state(tab, LV_STATE_CHECKED);
  else lv_obj_remove_state(tab, LV_STATE_CHECKED);
}

static void videoViewerBackEvent(lv_event_t* e) {
  (void)e;
  stopVideoViewer();
}

static void videoViewerPauseEvent(lv_event_t* e) {
  (void)e;
  Tab5Video.togglePause();
  updateVideoViewer(true);
}

static void createVideoViewer() {
  videoViewerScreen = lv_obj_create(nullptr);
  lv_obj_set_size(videoViewerScreen, SCREEN_W, SCREEN_H);
  lv_obj_set_style_bg_color(videoViewerScreen, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(videoViewerScreen, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(videoViewerScreen, 0, 0);
  lv_obj_set_style_pad_all(videoViewerScreen, 0, 0);
  lv_obj_remove_flag(videoViewerScreen, LV_OBJ_FLAG_SCROLLABLE);

  videoViewerImage = lv_image_create(videoViewerScreen);
  lv_obj_center(videoViewerImage);
  lv_obj_remove_flag(videoViewerImage, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* header = lv_obj_create(videoViewerScreen);
  lv_obj_set_size(header, SCREEN_W, 68);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_bg_color(header, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_70, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_radius(header, 0, 0);
  lv_obj_set_style_pad_all(header, 7, 0);
  lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* back = makeIconButton(header, LV_SYMBOL_LEFT, 54, 52);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_add_event_cb(back, videoViewerBackEvent, LV_EVENT_CLICKED, nullptr);

  videoViewerTitle = lv_label_create(header);
  lv_obj_set_width(videoViewerTitle, 720);
  lv_label_set_long_mode(videoViewerTitle, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(videoViewerTitle, lv_color_white(), 0);
  lv_obj_set_style_text_font(videoViewerTitle, uiFont(), 0);
  lv_obj_align(videoViewerTitle, LV_ALIGN_LEFT_MID, 70, -12);

  videoViewerStatus = lv_label_create(header);
  lv_obj_set_width(videoViewerStatus, 720);
  lv_label_set_long_mode(videoViewerStatus, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(videoViewerStatus, lv_color_hex(0xb8b8b8), 0);
  lv_obj_align(videoViewerStatus, LV_ALIGN_LEFT_MID, 70, 16);

  videoViewerPause = makeIconButton(header, LV_SYMBOL_PAUSE, 60, 52);
  lv_obj_align(videoViewerPause, LV_ALIGN_RIGHT_MID, -68, 0);
  lv_obj_add_event_cb(videoViewerPause, videoViewerPauseEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_t* stop = makeIconButton(header, LV_SYMBOL_STOP, 60, 52);
  lv_obj_align(stop, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(stop, videoViewerBackEvent, LV_EVENT_CLICKED, nullptr);

  lv_screen_load(videoViewerScreen);
}

static void startVideoViewerSource(const String& source, const String& name,
                                   float fps, const String& format) {
  if (!isVideoExtension(source) && format != "avi" && format != "mjpeg" &&
      format != "mjpg") {
    toast("Vidéo prise en charge : AVI/MJPEG ou MJPEG brut", 3200);
    return;
  }
  if (videoViewerActive) stopVideoViewer();
  if (!Tab5Video.playSource(source, name, fps, format)) {
    toast("Impossible de lancer la vidéo", 2800);
    return;
  }
  videoViewerActive = true;
  shownVideoGeneration = 0;
  memset(&videoViewerDsc, 0, sizeof(videoViewerDsc));
  createVideoViewer();
  updateVideoViewer(true);
}

static void startVideoViewer(size_t index, uint32_t revision) {
  Tab5VideoItem item;
  if (!Tab5Video.item(index, item)) {
    toast("Vidéo introuvable");
    return;
  }
  if(!revision || revision!=Tab5Video.catalogueRevision()){refreshAudioList();return;}
  startVideoViewerSource(item.source, item.name, item.fps, item.format);
}

static void stopVideoViewer() {
  if (!videoViewerActive) return;
  Tab5Video.stop();
  videoViewerActive = false;
  if (videoViewerImage && lv_obj_is_valid(videoViewerImage))
    lv_image_set_src(videoViewerImage, nullptr);
  Tab5Video.detachDisplay();
  lv_obj_t* old = videoViewerScreen;
  lv_screen_load(root);
  videoViewerScreen = nullptr;
  videoViewerImage = nullptr;
  videoViewerTitle = nullptr;
  videoViewerStatus = nullptr;
  videoViewerPause = nullptr;
  shownVideoGeneration = 0;
  memset(&videoViewerDsc, 0, sizeof(videoViewerDsc));
  if (old && lv_obj_is_valid(old)) lv_obj_delete(old);
  lv_obj_invalidate(root);
  updateClockLabels(true);
}

static void updateVideoViewer(bool force) {
  if (!videoViewerActive || !videoViewerScreen) return;
  const bool dirty = Tab5Video.takeUiDirty();
  if (!force && !dirty) return;
  Tab5VideoSnapshot state;
  if (!Tab5Video.snapshot(state)) return;
  if (videoViewerTitle) setLabelIfChanged(videoViewerTitle, state.name.c_str());
  if (videoViewerStatus) {
    char details[160];
    snprintf(details, sizeof(details), "%s  •  %02lu:%02lu  •  %ux%u%s",
             translateRuntimeText(state.status).c_str(),
             static_cast<unsigned long>(state.elapsedMs / 60000UL),
             static_cast<unsigned long>((state.elapsedMs / 1000UL) % 60UL),
             static_cast<unsigned>(state.width),
             static_cast<unsigned>(state.height),
             state.hasAudio ? "  •  PCM" : "");
    setLabelIfChanged(videoViewerStatus, details);
  }
  if (videoViewerPause) {
    setIconButtonSymbol(videoViewerPause,
        state.state == Tab5VideoState::Paused ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
  }

  Tab5VideoFrame frame;
  if (!videoViewerImage || !Tab5Video.frame(frame) ||
      (!force && frame.generation == shownVideoGeneration)) return;
  shownVideoGeneration = frame.generation;
  memset(&videoViewerDsc, 0, sizeof(videoViewerDsc));
  videoViewerDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  videoViewerDsc.header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
  videoViewerDsc.header.w = frame.width;
  videoViewerDsc.header.h = frame.height;
  videoViewerDsc.header.stride = frame.stride;
  videoViewerDsc.data_size = static_cast<uint32_t>(frame.stride) * frame.height;
  videoViewerDsc.data = frame.data;
  lv_image_set_src(videoViewerImage, nullptr);
  lv_image_set_src(videoViewerImage, &videoViewerDsc);
  const uint32_t fitX = static_cast<uint32_t>(SCREEN_W) * 256U /
                        std::max<uint16_t>(1, frame.width);
  const uint32_t fitY = static_cast<uint32_t>(SCREEN_H) * 256U /
                        std::max<uint16_t>(1, frame.height);
  lv_image_set_scale(videoViewerImage,
      static_cast<uint16_t>(std::min<uint32_t>(256U, std::min(fitX, fitY))));
  lv_obj_center(videoViewerImage);
  lv_obj_invalidate(videoViewerImage);
}

static void audioRememberPlayback(const Tab5AudioSnapshot& audioState) {
  const uint32_t now=millis();
  settings.audioVolume=audioState.volume;
  if(audioState.mode==Tab5AudioMode::Local&&audioState.currentPath.length()){
    const unsigned view=audioPathIsRecording(audioState.currentPath)?2:0;
    if(audioState.currentPath!=Tab5MediaSession::state().selected[view])
      Tab5MediaSession::select(view,audioState.currentPath.c_str(),audioState.nowPlaying.c_str(),now);
  }else if(audioState.mode==Tab5AudioMode::Radio&&audioState.currentStationUrl.length()&&
           audioState.currentStationUrl!=Tab5MediaSession::state().selected[1]){
    String title=audioState.nowPlaying;
    for(size_t i=0;i<Tab5Audio.stationCount();++i){Tab5AudioStation station;if(Tab5Audio.station(i,station)&&station.url==audioState.currentStationUrl){title=station.name;break;}}
    Tab5MediaSession::select(1,audioState.currentStationUrl.c_str(),title.c_str(),now);
  }
}

static void audioRestoreBrowser() {
  const auto& saved=Tab5MediaSession::state();
  audioView=static_cast<AudioView>(saved.view);
  audioAlbumFolder=saved.folder;audioRadioCategory=saved.category;
  audioFavoritesOnly[0]=(saved.favorites&1)!=0;audioFavoritesOnly[1]=(saved.favorites&2)!=0;
  for(unsigned i=0;i<4;++i)audioSearchQueries[i]=saved.search[i];
}
static void audioRememberBrowser() {
  const uint32_t now=millis();
  Tab5MediaSession::browser(unsigned(audioView),(audioFavoritesOnly[0]?1:0)|(audioFavoritesOnly[1]?2:0),audioAlbumFolder.c_str(),audioRadioCategory.c_str(),now);
  for(unsigned i=0;i<4;++i)Tab5MediaSession::search(i,audioSearchQueries[i].c_str(),now);
}
static void audioRememberScroll() {
  if(audioList&&!audioListRebuilding&&activePage==Page::Audio)
    Tab5MediaSession::scroll(unsigned(audioView),lv_obj_get_scroll_y(audioList),millis());
}
static uint32_t audioCatalogueRevision(AudioView view) {
  return view==AudioView::Videos?Tab5Video.catalogueRevision():
      view==AudioView::Radio?Tab5Audio.stationCatalogueRevision():Tab5Audio.trackCatalogueRevision();
}
static bool audioModelCurrent() {
  return audioView==audioRenderedView && audioRenderedRevision &&
      audioRenderedRevision==audioCatalogueRevision(audioView);
}
static bool audioRememberItem(size_t index) {
  if(!audioModelCurrent())return false;
  String id,title;
  if(audioView==AudioView::Radio){Tab5AudioStation station;if(!Tab5Audio.station(index,station))return false;id=station.url;title=station.name;}
  else if(audioView==AudioView::Videos){Tab5VideoItem video;if(!Tab5Video.item(index,video))return false;id=video.source;title=video.name;}
  else{id=Tab5Audio.trackPath(index);title=Tab5Audio.trackName(index);}
  if(!audioModelCurrent()||!id.length())return false;
  Tab5MediaSession::select(unsigned(audioView),id.c_str(),title.c_str(),millis());
  if(audioRows)audioRows->select(index);
  audioRememberScroll();audioRememberBrowser();return true;
}

static void audioGroupBackEvent(lv_event_t* e) {
  if (e && lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  Tab5MediaSession::scroll(unsigned(audioView),0,millis());
  if (audioView == AudioView::Local){audioAlbumFolder=parentDirOf(audioAlbumFolder);if(audioAlbumFolder=="/")audioAlbumFolder="";}
  else if (audioView == AudioView::Radio){int at=audioRadioCategory.lastIndexOf('/');audioRadioCategory=at<0?String(""):audioRadioCategory.substring(0,at);}
  refreshAudioList();
}

static lv_obj_t* audioRowTitle(lv_obj_t* row) {
  if (!row) return nullptr;
  const uint32_t count = lv_obj_get_child_count(row);
  return count ? lv_obj_get_child(row, static_cast<int32_t>(count - 1)) : nullptr;
}

static void stopAudioRowMarquee(lv_obj_t* row, int width) {
  lv_obj_t* label = audioRowTitle(row);
  if (!label) return;
  lv_obj_set_width(label, width);
  lv_obj_set_flex_grow(label, 1);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
}

static String audioFavoriteId(size_t index){
 if(audioView==AudioView::Radio){Tab5AudioStation station;return Tab5Audio.station(index,station)?station.url:String("");}return Tab5Audio.trackPath(index);
}
static void audioMediaMetadata(Tab5MediaList::Entry entry,String& name,uint64_t& bytes,int64_t& modified) {
  bytes=0;modified=0;String path;
  if(entry.kind==Tab5MediaList::Folder){const auto& groups=audioView==AudioView::Radio?audioRadioCategories:audioAlbumFolders;if(entry.index<groups.size()){path=groups[entry.index];name=baseNameOf(path);}}
  else if(entry.kind==Tab5MediaList::Media){
    if(audioView==AudioView::Radio){Tab5AudioStation station;if(Tab5Audio.station(entry.index,station))name=station.name;}
    else if(audioView==AudioView::Videos){Tab5VideoItem item;if(Tab5Video.item(entry.index,item)){name=item.name;if(!item.remote)path=item.source;}}
    else{path=Tab5Audio.trackPath(entry.index);name=Tab5Audio.trackName(entry.index);}
  }
  if(audioView!=AudioView::Radio&&unsigned(Tab5FileSort::selected)>=2&&path.length()){
    File file=tab5FS.open(path);if(file){bytes=file.size();modified=file.getLastWrite();file.close();}
  }
}
static void audioMediaFormat(Tab5MediaList::Entry entry,String& label,const char*& icon,bool& favorite,bool& canFavorite,bool& trash) {
  icon=LV_SYMBOL_AUDIO;favorite=false;canFavorite=false;trash=false;
  if(!audioModelCurrent()){label=T("Chargement");icon="";return;}
  if(entry.kind==Tab5MediaList::Parent){label=T("Dossier parent");icon=LV_SYMBOL_LEFT;return;}
  if(entry.kind==Tab5MediaList::Folder){const auto& groups=audioView==AudioView::Radio?audioRadioCategories:audioAlbumFolders;label=entry.index<groups.size()?baseNameOf(groups[entry.index]):String();icon=LV_SYMBOL_DIRECTORY;return;}
  if(entry.kind==Tab5MediaList::Import){label=T("Ajouter des radios");icon=LV_SYMBOL_PLUS;return;}
  if(entry.kind==Tab5MediaList::Message){icon="";label=T(entry.index==1?"Mémoire insuffisante pour la liste des médias":audioFavoritesOnly[unsigned(audioView)<2?unsigned(audioView):0]&&unsigned(audioView)<2?"Aucun favori":audioSearchQueries[unsigned(audioView)].length()?"Aucun résultat":audioView==AudioView::Radio?"Aucune radio enregistrée":"Dossier vide");return;}
  const bool detail=audioSearchQueries[unsigned(audioView)].length()||(unsigned(audioView)<2&&audioFavoritesOnly[unsigned(audioView)]);
  if(audioView==AudioView::Radio){Tab5AudioStation station;if(Tab5Audio.station(entry.index,station)){label=station.name;if(detail)label+="\n"+audioStationCategory(station);}icon=LV_SYMBOL_WIFI;}
  else if(audioView==AudioView::Videos){Tab5VideoItem video;if(Tab5Video.item(entry.index,video))label=video.name;icon=LV_SYMBOL_IMAGE;}
  else{label=Tab5Audio.trackName(entry.index);if(detail)label+="\n"+audioAlbumName(parentDirOf(Tab5Audio.trackPath(entry.index)));}
  canFavorite=audioView==AudioView::Local||audioView==AudioView::Radio;
  favorite=canFavorite&&Tab5Favorites::contains(audioView==AudioView::Radio,audioFavoriteId(entry.index));
  if(audioView==AudioView::Recordings){canFavorite=true;trash=true;}
}
static void audioMediaAction(Tab5MediaList::Entry entry,bool favorite) {
  if(!audioModelCurrent()){refreshAudioList();return;}
  if(entry.kind==Tab5MediaList::Parent){audioGroupBackEvent(nullptr);return;}
  if(entry.kind==Tab5MediaList::Folder){
    auto& groups=audioView==AudioView::Radio?audioRadioCategories:audioAlbumFolders;
    if(entry.index>=groups.size())return;
    if(audioView==AudioView::Radio)audioRadioCategory=groups[entry.index];else audioAlbumFolder=groups[entry.index];
    Tab5MediaSession::scroll(unsigned(audioView),0,millis());refreshAudioList();return;
  }
  if(entry.kind==Tab5MediaList::Import){showPage(Page::Home);showPythonLibrary(true);return;}
  if(entry.kind!=Tab5MediaList::Media)return;
  if(favorite){if(audioView==AudioView::Recordings){const String path=Tab5Audio.trackPath(entry.index);if(!audioModelCurrent()){refreshAudioList();return;}if(audioPathIsRecording(path))askDeletePath(path);return;}const String favoriteId=audioFavoriteId(entry.index);if(!audioModelCurrent()){refreshAudioList();return;}if(!Tab5Favorites::toggle(audioView==AudioView::Radio,favoriteId)){toast("Enregistrement des favoris impossible");return;}audioRememberScroll();refreshAudioList();return;}
  if(!audioRememberItem(entry.index)){refreshAudioList();return;}
  if(audioView==AudioView::Videos){startVideoViewer(entry.index,audioRenderedRevision);return;}
  Tab5Video.stop();
  if(audioView==AudioView::Radio)Tab5Audio.playStation(entry.index,audioRenderedRevision);else Tab5Audio.playTrack(entry.index,audioRenderedRevision);
  updateAudioUi(true);
}
static String audioChildGroup(const String& full,const String& parent){
 String prefix=parent.length()?parent+"/":String("");if(!full.startsWith(prefix)||full==parent)return "";
 int next=full.indexOf('/',prefix.length());return next<0?full:full.substring(0,next);
}
static void refreshAudioList() {
  audioRefreshPending=false;
  if(audioView==AudioView::Videos)Tab5Video.ensureLibraryLoaded();else Tab5Audio.ensureLibraryLoaded();
  if(!audioList||!audioListTitle)return;
  if(!audioRows)audioRows=new(std::nothrow)Tab5MediaList(audioList,uiFont(),audioMediaFormat,audioMediaAction);
  if(!audioRows){audioRestoreBrowser();toast("Mémoire insuffisante pour la liste des médias");return;}
  const uint32_t sourceRevision=audioCatalogueRevision(audioView);
  if(!sourceRevision){audioRestoreBrowser();audioRefreshPending=true;return;}
  static auto appliedSort=Tab5FileSort::selected;
  audioListRebuilding=true;audioRows->reset();
  std::vector<String> previousAlbums,previousCategories;
  previousAlbums.swap(audioAlbumFolders);previousCategories.swap(audioRadioCategories);
  size_t shown=0;bool complete=true;
  auto add=[&](Tab5MediaList::Kind kind,uint32_t index=0){if(!audioRows->add(kind,index))complete=false;};
  const bool supportsFavorites=unsigned(audioView)<2;
  const bool favorites=supportsFavorites&&audioFavoritesOnly[unsigned(audioView)];
  const String searchText=audioSearchQueries[unsigned(audioView)];const bool searching=searchText.length()>0;
  tab5search::Query query(searchText.c_str());
  if(!searching&&!favorites&&supportsFavorites){
    auto& groups=audioView==AudioView::Radio?audioRadioCategories:audioAlbumFolders;
    String parent=audioView==AudioView::Radio?audioRadioCategory:audioAlbumFolder;
    if(audioView==AudioView::Radio){for(size_t i=0;i<Tab5Audio.stationCount();++i){Tab5AudioStation radio;if(!Tab5Audio.station(i,radio))continue;String child=audioChildGroup(audioStationCategory(radio),parent);if(child.length())addUniqueAudioGroup(groups,child);}}
    else if(!parent.length()){if(tab5FS.exists("/Audio"))groups.push_back("/Audio");if(tab5FS.exists("/mp3"))groups.push_back("/mp3");}
    else{File dir=tab5FS.open(parent);if(dir&&dir.isDirectory()){File child=dir.openNextFile();while(child&&groups.size()<800){if(child.isDirectory()){String name=baseNameOf(String(child.name()));if(name.length()&&name[0]!='.')addUniqueAudioGroup(groups,joinPath(parent,name));}child.close();child=dir.openNextFile();}}}
    sortAudioGroups(groups);if(parent.length())add(Tab5MediaList::Parent);
    for(size_t g=0;g<groups.size()&&complete;++g){add(Tab5MediaList::Folder,g);++shown;}
  }
  const size_t total=audioView==AudioView::Radio?Tab5Audio.stationCount():audioView==AudioView::Videos?Tab5Video.count():Tab5Audio.trackCount();
  uint32_t selected=UINT32_MAX;const char* wanted=Tab5MediaSession::state().selected[unsigned(audioView)];
  for(size_t i=0;i<total&&complete;++i){
    String id,text,match;
    if(audioView==AudioView::Radio){Tab5AudioStation radio;if(!Tab5Audio.station(i,radio))continue;String category=audioStationCategory(radio);if(!searching&&!favorites&&category!=audioRadioCategory)continue;id=radio.url;text=radio.name;match=text+" "+category;}
    else if(audioView==AudioView::Videos){Tab5VideoItem video;if(!Tab5Video.item(i,video))continue;id=video.source;text=match=video.name;}
    else{String path=Tab5Audio.trackPath(i);if(audioPathIsRecording(path)!=(audioView==AudioView::Recordings))continue;if(!searching&&!favorites&&audioView==AudioView::Local&&parentDirOf(path)!=audioAlbumFolder)continue;id=path;text=Tab5Audio.trackName(i);match=text+" "+path;}
    if(searching&&!query.matches(match.c_str()))continue;
    if(favorites&&!Tab5Favorites::contains(audioView==AudioView::Radio,id))continue;
    if(id==wanted)selected=i;add(Tab5MediaList::Media,i);++shown;
  }
  String title=favorites?String(T("Favoris")):audioView==AudioView::Radio?(audioRadioCategory.length()?audioRadioCategory:String(T("Catégories radio"))):audioView==AudioView::Local?(audioAlbumFolder.length()?audioAlbumFolder:String(T("Albums"))):String(T(audioView==AudioView::Videos?"Vidéos":"Enregistrements"));
  if(searching)title+=" — "+searchText;
  if(!shown)add(Tab5MediaList::Message);
  if(audioView==AudioView::Radio&&!searching&&!favorites&&!total)add(Tab5MediaList::Import);
  if(complete&&!audioRows->sort(audioView==AudioView::Radio?static_cast<Tab5FileSort::Order>(unsigned(Tab5FileSort::selected)%2):Tab5FileSort::selected,audioMediaMetadata))complete=false;
  const bool stale=!sourceRevision || sourceRevision!=audioCatalogueRevision(audioView);
  if(!complete || stale){
    audioRows->cancel();
    audioAlbumFolders.swap(previousAlbums);audioRadioCategories.swap(previousCategories);
    audioRestoreBrowser();
    if(!audioRenderedRevision || audioRenderedRevision!=audioCatalogueRevision(audioRenderedView)){
      audioRows->release();audioRenderedView=audioView;
      audioRenderedRevision=audioCatalogueRevision(audioView);
    }
    audioRefreshPending=stale;
    if(audioSortSelector)lv_dropdown_set_selected(audioSortSelector,
        audioView==AudioView::Radio?unsigned(appliedSort)%2:unsigned(appliedSort));
    if(audioFavoritesSelector)lv_dropdown_set_selected(audioFavoritesSelector,
        unsigned(audioView)<2&&audioFavoritesOnly[unsigned(audioView)]?1:0);
    audioListRebuilding=false;
    if(!stale)toast("Mémoire insuffisante pour la liste des médias");return;
  }
  appliedSort=Tab5FileSort::selected;
  audioRows->appearance(uiFont());
  setLabelIfChanged(audioListTitle,title.c_str());
  if(audioSortSelector){String options;const unsigned count=audioView==AudioView::Radio?2:6;for(unsigned i=0;i<count;++i){if(i)options+="\n";options+=T(Tab5FileSort::caption(i));}if(strcmp(lv_dropdown_get_options(audioSortSelector),options.c_str()))lv_dropdown_set_options(audioSortSelector,options.c_str());lv_dropdown_set_selected(audioSortSelector,audioView==AudioView::Radio?unsigned(Tab5FileSort::selected)%2:unsigned(Tab5FileSort::selected));}
  if(audioFavoritesTab){if(supportsFavorites)lv_obj_remove_flag(audioFavoritesTab,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(audioFavoritesTab,LV_OBJ_FLAG_HIDDEN);setAudioTabState(audioFavoritesTab,favorites);}
  if(audioFavoritesSelector){if(supportsFavorites)lv_obj_remove_flag(audioFavoritesSelector,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(audioFavoritesSelector,LV_OBJ_FLAG_HIDDEN);lv_dropdown_set_selected(audioFavoritesSelector,favorites?1:0);}
  if(audioNewFolder){if(audioView==AudioView::Local&&!favorites)lv_obj_remove_flag(audioNewFolder,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(audioNewFolder,LV_OBJ_FLAG_HIDDEN);}
  if(audioSearchClear){if(searching)lv_obj_remove_flag(audioSearchClear,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(audioSearchClear,LV_OBJ_FLAG_HIDDEN);}
  audioRenderedView=audioView;audioRenderedRevision=sourceRevision;
  audioRows->show(Tab5MediaSession::state().scroll[unsigned(audioView)],selected);
  audioListRebuilding=false;audioRememberBrowser();
  setAudioTabState(audioLocalTab,audioView==AudioView::Local);setAudioTabState(audioRadioTab,audioView==AudioView::Radio);setAudioTabState(audioRecordTab,audioView==AudioView::Recordings);setAudioTabState(audioVideoTab,audioView==AudioView::Videos);
}

static void audioLocalTabEvent(lv_event_t* e) {
  (void)e;if(audioView==AudioView::Local)return;audioRememberScroll();audioView=AudioView::Local;
  refreshAudioList();updateAudioUi(true);
}

static void audioRadioTabEvent(lv_event_t* e) {
  (void)e;if(audioView==AudioView::Radio)return;audioRememberScroll();audioView=AudioView::Radio;
  refreshAudioList();updateAudioUi(true);
}

static void audioRecordTabEvent(lv_event_t* e) {
  (void)e;if(audioView==AudioView::Recordings)return;audioRememberScroll();audioView=AudioView::Recordings;
  refreshAudioList();updateAudioUi(true);
}

static void audioVideoTabEvent(lv_event_t* e) {
  (void)e;if(audioView==AudioView::Videos)return;audioRememberScroll();audioView=AudioView::Videos;
  refreshAudioList();updateAudioUi(true);
}

static void audioReloadEvent(lv_event_t* e) {
  (void)e;audioRememberScroll();Tab5Audio.reload();Tab5Video.reload();
  refreshAudioList();updateAudioUi(true);
}

static void audioPreviousEvent(lv_event_t* e) {
  (void)e;
  Tab5Audio.previous();
  updateAudioUi(true);
}

static void audioPlayEvent(lv_event_t* e) {
  (void)e;
  if(!audioModelCurrent()){refreshAudioList();return;}
  if(audioView==AudioView::Videos){toast("Touchez une vidéo pour l’ouvrir");return;}
  Tab5AudioSnapshot state;if(!Tab5Audio.snapshot(state)||state.recording)return;
  if(state.mode!=Tab5AudioMode::Idle){Tab5Audio.togglePause();updateAudioUi(true);return;}
  const bool radio=audioView==AudioView::Radio;
  const bool favorite=unsigned(audioView)<2&&audioFavoritesOnly[unsigned(audioView)];
  const char* selected=Tab5MediaSession::state().selected[unsigned(audioView)];
  const size_t count=radio?Tab5Audio.stationCount():Tab5Audio.trackCount();
  size_t chosen=SIZE_MAX;
  tab5search::Query query(audioSearchQueries[unsigned(audioView)].c_str());
  const bool searching=audioSearchQueries[unsigned(audioView)].length()>0;
  for(size_t i=0;i<count;++i){
    String id,match;
    if(radio){Tab5AudioStation station;if(!Tab5Audio.station(i,station))continue;id=station.url;match=station.name+" "+audioStationCategory(station);if(!favorite&&!searching&&audioStationCategory(station)!=audioRadioCategory)continue;}
    else{id=Tab5Audio.trackPath(i);if(audioPathIsRecording(id)!=(audioView==AudioView::Recordings))continue;if(!favorite&&!searching&&audioView==AudioView::Local&&parentDirOf(id)!=audioAlbumFolder)continue;match=Tab5Audio.trackName(i)+" "+id;}
    if(favorite&&!Tab5Favorites::contains(radio,id))continue;
    if(searching&&!query.matches(match.c_str()))continue;
    if(chosen==SIZE_MAX)chosen=i;
    if(id==selected){chosen=i;break;}
  }
  if(chosen!=SIZE_MAX){if(!audioRememberItem(chosen)){refreshAudioList();return;}if(radio)Tab5Audio.playStation(chosen,audioRenderedRevision);else Tab5Audio.playTrack(chosen,audioRenderedRevision);}
  else toast(radio?"Choisissez une catégorie radio":"Choisissez un album");
  updateAudioUi(true);
}

static void audioNextEvent(lv_event_t* e) {
  (void)e;
  Tab5Audio.next();
  updateAudioUi(true);
}

static void audioStopEvent(lv_event_t* e) {
  (void)e;
  Tab5Audio.stop();
  refreshAudioList();
  updateAudioUi(true);
}

static void audioRecordEvent(lv_event_t* e) {
  (void)e;
  static uint32_t lastRecordActionMs = 0;
  const uint32_t now = millis();
  if (lastRecordActionMs && now - lastRecordActionMs < 700) return;
  lastRecordActionMs = now;

  Tab5AudioSnapshot audioState;
  if (!Tab5Audio.snapshot(audioState)) return;
  if (audioState.recording) Tab5Audio.stopRecording();
  else {
    audioView = AudioView::Recordings;
    Tab5Audio.startRecording();
  }
  refreshAudioList();
  updateAudioUi(true);
}

static void audioVolumeEvent(lv_event_t* e) {
  settings.audioVolume = static_cast<uint8_t>(
      lv_slider_get_value(lv_event_get_target_obj(e)));
  Tab5Audio.setVolume(settings.audioVolume);
  updateAudioUi(true);
}

static void audioProgressEvent(lv_event_t* e) {
  if (!audioProgressSlider) return;
  const lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    Tab5AudioSnapshot state;
    audioProgressDragging = Tab5Audio.snapshot(state) &&
        state.mode == Tab5AudioMode::Local &&
        state.playbackEstimatedSeconds > 0 &&
        audioPathIsSeekable(state.currentPath);
    return;
  }
  if (code == LV_EVENT_VALUE_CHANGED && audioProgressDragging) {
    Tab5AudioSnapshot state;
    if (!Tab5Audio.snapshot(state) || !state.playbackEstimatedSeconds) return;
    const uint32_t target = static_cast<uint32_t>(
        static_cast<uint64_t>(state.playbackEstimatedSeconds) *
        lv_slider_get_value(audioProgressSlider) / 1000ULL);
    char preview[40];
    snprintf(preview, sizeof(preview), "%02lu:%02lu  /  %02lu:%02lu",
             static_cast<unsigned long>(target / 60),
             static_cast<unsigned long>(target % 60),
             static_cast<unsigned long>(state.playbackEstimatedSeconds / 60),
             static_cast<unsigned long>(state.playbackEstimatedSeconds % 60));
    setLabelIfChanged(audioTimeLabel, preview);
    return;
  }
  if (code == LV_EVENT_RELEASED) {
    if (audioProgressDragging) {
      const uint16_t target = static_cast<uint16_t>(
          lv_slider_get_value(audioProgressSlider));
      if (!Tab5Audio.seekPermille(target))
        toast("Commande de déplacement indisponible");
    }
    audioProgressDragging = false;
  } else if (code == LV_EVENT_CANCEL) {
    audioProgressDragging = false;
  }
}

static void refreshWifiPopupActions() {
  if (!wifiOverlay) return;
  const bool idle = !tab5FileShareBusy() && !Tab5Audio.wifiScanRunning();
  const bool saved = Tab5Audio.hasSavedWifi();
  const bool selectedSaved = saved && wifiSelectedSsid == Tab5Audio.savedWifiSsid();
  const bool havePassword = wifiPasswordInput && *lv_textarea_get_text(wifiPasswordInput);
  setButtonEnabled(wifiScanButton, idle);
  setButtonEnabled(wifiConnectButton, idle && wifiSelectedSsid.length() &&
      (!wifiSelectedSecured || selectedSaved || havePassword));
  setButtonEnabled(wifiForgetButton, idle && saved);
}

static void refreshWifiPopupList(bool force) {
  if (!wifiOverlay || !wifiNetworkList) return;
  refreshWifiPopupActions();
  const uint32_t revision = Tab5Audio.wifiScanRevision();
  if (!force && revision == shownWifiScanRevision) return;
  shownWifiScanRevision = revision;
  lv_obj_clean(wifiNetworkList);

  if (Tab5Audio.wifiScanRunning()) {
    lv_list_add_text(wifiNetworkList, T("Recherche en cours…"));
    return;
  }
  if (!Tab5Audio.wifiNetworkCount()) {
    const String scanStatus = Tab5Audio.wifiScanStatus();
    lv_list_add_text(wifiNetworkList, T(scanStatus.length() ? scanStatus.c_str() : "Aucun réseau détecté"));
    return;
  }
  for (size_t i = 0; i < Tab5Audio.wifiNetworkCount(); ++i) {
    Tab5WifiNetwork network;
    if (!Tab5Audio.wifiNetwork(i, network)) continue;
    String label = network.ssid + "  •  " + String(network.rssi) + " dBm";
    lv_obj_t* row = lv_list_add_button(
        wifiNetworkList, network.secured ? LV_SYMBOL_WIFI : LV_SYMBOL_OK,
        label.c_str());
    lv_obj_set_height(row, 54);
    lv_obj_set_style_bg_color(row, C_SURFACE(), 0);
    lv_obj_set_style_bg_color(row, C_ACTIVE(), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(row, C_TEXT(), 0);
    stopAudioRowMarquee(row, 360);
    lv_obj_add_event_cb(row, [](lv_event_t* event) {
      const size_t index = static_cast<size_t>(
          reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
      Tab5WifiNetwork selected;
      if (!Tab5Audio.wifiNetwork(index, selected)) return;
      wifiSelectedSsid = selected.ssid;
      wifiSelectedSecured = selected.secured;
      String text = selected.ssid;
      text += selected.secured ? T("  •  protégé") : T("  •  ouvert");
      lv_label_set_text(wifiSelectedLabel, text.c_str());
      lv_textarea_set_text(wifiPasswordInput, "");
      refreshWifiPopupActions();
    }, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
  }
}

static void closeWifiPopup() {
  if (keyboardTarget == wifiPasswordInput) {
    setKeyboardVisible(false);
    keyboardTarget = nullptr;
  }
  if (wifiOverlay && lv_obj_is_valid(wifiOverlay)) lv_obj_delete(wifiOverlay);
  wifiOverlay = nullptr;
  wifiNetworkList = nullptr;
  wifiSelectedLabel = nullptr;
  wifiPasswordInput = nullptr;
  wifiPopupAutoSwitch = nullptr;
  wifiScanButton = wifiConnectButton = wifiForgetButton = nullptr;
  shownWifiScanRevision = 0;
}

static void wifiCloseEvent(lv_event_t* e) {
  (void)e;
  closeWifiPopup();
}

static void wifiScanEvent(lv_event_t* e) {
  if(Tab5Audio.wifiScanRunning())return;
  if(!wifiChangeAllowed())return;
  (void)e;
  Tab5Audio.startWifiScan();
  refreshWifiPopupList(true);
}

static void wifiPasswordFocusEvent(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED &&
      lv_event_get_code(e) != LV_EVENT_FOCUSED) return;
  setKeyboardVisible(!writing::usbConnected(), wifiPasswordInput);
}

static void wifiConnectEvent(lv_event_t* e) {
  if(Tab5Audio.wifiScanRunning())return;
  if(!wifiChangeAllowed())return;
  (void)e;
  if (!wifiSelectedSsid.length()) {
    toast("Sélectionnez un réseau Wi-Fi");
    return;
  }
  const String password = wifiPasswordInput
      ? String(lv_textarea_get_text(wifiPasswordInput)) : String();
  const bool savedNetwork = Tab5Audio.hasSavedWifi() &&
                            wifiSelectedSsid == Tab5Audio.savedWifiSsid();
  if (wifiSelectedSecured && !password.length() && !savedNetwork) {
    toast("Saisissez le mot de passe Wi-Fi");
    return;
  }
  if (Tab5Audio.connectWifi(wifiSelectedSsid, password, true)) {
    closeWifiPopup();
    lastWifiSettingsUpdateMs = 0;
  }
}

static void wifiForgetEvent(lv_event_t* e) {
  if(Tab5Audio.wifiScanRunning()||!Tab5Audio.hasSavedWifi())return;
  if(!wifiChangeAllowed())return;
  (void)e;
  // SNTP utilise la boîte tcpip de lwIP : l'arrêter avant que le moteur audio
  // ne désinitialise le contrôleur Wi-Fi P4/C6.
  stopNetworkTime();
  Tab5Audio.forgetWifi();
  wifiSelectedSsid = "";
  if (wifiSelectedLabel) lv_label_set_text(wifiSelectedLabel, T("Aucun réseau sélectionné"));
  if (wifiPasswordInput) lv_textarea_set_text(wifiPasswordInput, "");
  lastWifiSettingsUpdateMs = 0;
  updateAudioUi(true);
}

static void showWifiPopup() {
  closeWifiPopup();
  wifiSelectedSsid = Tab5Audio.hasSavedWifi() ? Tab5Audio.savedWifiSsid() : String();
  wifiSelectedSecured = true;

  wifiOverlay = lv_obj_create(root);
  lv_obj_set_size(wifiOverlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(wifiOverlay, 0, 0);
  lv_obj_set_style_bg_color(wifiOverlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(wifiOverlay, LV_OPA_60, 0);
  lv_obj_set_style_border_width(wifiOverlay, 0, 0);
  lv_obj_remove_flag(wifiOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(wifiOverlay);

  lv_obj_t* box = lv_obj_create(wifiOverlay);
  lv_obj_set_size(box, 1040, 440);
  lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 18);
  lv_obj_set_style_bg_color(box, C_SURFACE(), 0);
  lv_obj_set_style_border_color(box, C_BORDER(), 0);
  lv_obj_set_style_radius(box, 12, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(box);
  lv_label_set_text(title, T("Réseaux Wi-Fi"));
  lv_obj_set_style_text_font(title, uiHeadingFont(), 0);
  lv_obj_set_pos(title, 8, 2);

  lv_obj_t* close = makeIconButton(box, LV_SYMBOL_CLOSE, 46, 42);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_add_event_cb(close, wifiCloseEvent, LV_EVENT_CLICKED, nullptr);

  wifiNetworkList = lv_list_create(box);
  lv_obj_set_size(wifiNetworkList, 500, 348);
  lv_obj_set_pos(wifiNetworkList, 0, 54);
  lv_obj_set_style_bg_color(wifiNetworkList, C_BG(), 0);
  lv_obj_set_style_border_width(wifiNetworkList, 0, 0);
  lv_obj_set_style_pad_all(wifiNetworkList, 4, 0);
  lv_obj_set_style_pad_right(wifiNetworkList, 28, 0);

  lv_obj_t* scan = makeIconButton(box, LV_SYMBOL_REFRESH, 54, 48);
  wifiScanButton = scan;
  lv_obj_set_pos(scan, 520, 62);
  lv_obj_add_event_cb(scan, wifiScanEvent, LV_EVENT_CLICKED, nullptr);

  wifiSelectedLabel = lv_label_create(box);
  const String selectedText = wifiSelectedSsid.length()
      ? wifiSelectedSsid + T("  •  mémorisé")
      : String(T("Aucun réseau sélectionné"));
  lv_label_set_text(wifiSelectedLabel, selectedText.c_str());
  lv_obj_set_width(wifiSelectedLabel, 390);
  lv_label_set_long_mode(wifiSelectedLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_color(wifiSelectedLabel, C_TEXT(), 0);
  lv_obj_set_pos(wifiSelectedLabel, 590, 70);

  wifiPasswordInput = lv_textarea_create(box);
  lv_textarea_set_one_line(wifiPasswordInput, true);
  lv_textarea_set_password_mode(wifiPasswordInput, true);
  lv_textarea_set_placeholder_text(wifiPasswordInput, T("Mot de passe"));
  lv_obj_set_size(wifiPasswordInput, 390, 58);
  lv_obj_set_pos(wifiPasswordInput, 590, 132);
  lv_obj_set_style_bg_color(wifiPasswordInput, C_BG(), 0);
  lv_obj_set_style_text_color(wifiPasswordInput, C_TEXT(), 0);
  lv_obj_set_style_text_font(wifiPasswordInput, uiFont(), 0);
  lv_obj_add_event_cb(wifiPasswordInput, wifiPasswordFocusEvent,
                      LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(wifiPasswordInput, [](lv_event_t*){refreshWifiPopupActions();}, LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t* connect = makeIconButton(box, LV_SYMBOL_OK, 90, 58);
  wifiConnectButton = connect;
  lv_obj_set_pos(connect, 590, 220);
  lv_obj_add_event_cb(connect, wifiConnectEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* forget = makeIconButton(box, LV_SYMBOL_TRASH, 90, 58);
  wifiForgetButton = forget;
  lv_obj_set_pos(forget, 710, 220);
  lv_obj_add_event_cb(forget, wifiForgetEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* autoLabel = lv_label_create(box);
  lv_label_set_text(autoLabel, T("Activer le Wi-Fi au démarrage"));
  lv_obj_set_width(autoLabel, 300);
  lv_obj_set_style_text_color(autoLabel, C_TEXT(), 0);
  lv_obj_set_pos(autoLabel, 590, 305);

  wifiPopupAutoSwitch = lv_switch_create(box);
  styleSettingsSwitch(wifiPopupAutoSwitch);
  lv_obj_set_pos(wifiPopupAutoSwitch, 900, 294);
  if (Tab5Audio.wifiAutoStart())
    lv_obj_add_state(wifiPopupAutoSwitch, LV_STATE_CHECKED);
  lv_obj_add_event_cb(wifiPopupAutoSwitch, wifiAutoStartEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);

  lv_obj_t* hint = lv_label_create(box);
  lv_label_set_text(hint, T("Connexion enregistrée dans la mémoire interne."));
  lv_obj_set_width(hint, 390);
  lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(hint, C_MUTED(), 0);
  lv_obj_set_pos(hint, 590, 365);

  Tab5Audio.startWifiScan();
  refreshWifiPopupList(true);
}

static void refreshWifiSettingsUi(bool force) {
  if (force) Tab5Audio.requestWifiSignal();
  const uint32_t now = millis();
  if (!force && now - lastWifiSettingsUpdateMs < 400) return;
  lastWifiSettingsUpdateMs = now;

  static Tab5AudioSnapshot audioState;
  if (!Tab5Audio.snapshot(audioState)) return;

  if (wifiPowerSwitch) {
    if (audioState.wifiEnabled) lv_obj_add_state(wifiPowerSwitch, LV_STATE_CHECKED);
    else lv_obj_remove_state(wifiPowerSwitch, LV_STATE_CHECKED);
  }
  if (wifiPopupAutoSwitch) {
    if (audioState.wifiAutoStart)
      lv_obj_add_state(wifiPopupAutoSwitch, LV_STATE_CHECKED);
    else
      lv_obj_remove_state(wifiPopupAutoSwitch, LV_STATE_CHECKED);
  }
  if (!wifiSettingsStatus) return;

  String status;
  if (!audioState.wifiEnabled) status = T("Wi-Fi désactivé");
  else if (audioState.wifiConnecting) status = T("Wi-Fi  •  connexion…");
  else if (audioState.wifiConnected) {
    status = audioState.savedWifiSsid;
    if (!status.length()) status = T("Wi-Fi connecté");
    if (audioState.wifiRssi < 0) status += "  •  " + String(audioState.wifiRssi) + " dBm";
  } else if (audioState.hasSavedWifi) {
    status = audioState.savedWifiSsid + "  •  hors ligne";
  } else status = T("Wi-Fi activé  •  aucun réseau");
  if (audioState.wifiEnabled) {
    const auto compatibility = Tab5Compatibility::snapshot();
    using Firmware = Tab5Compatibility::Firmware;
    if (compatibility.firmware == Firmware::DifferentVersion ||
        compatibility.transport == Tab5Compatibility::Transport::Failed) {
      status += "\n";
      status += T(compatibility.transport == Tab5Compatibility::Transport::Failed ?
          Tab5Compatibility::transportMessage(compatibility.transport) :
          Tab5Compatibility::firmwareMessage(compatibility.firmware));
    }
  }
  if (Tab5Locale.hasTranslations()) {
    const String translatedStatus = translateRuntimeText(status);
    setLabelIfChanged(wifiSettingsStatus, translatedStatus.c_str());
  } else {
    setLabelIfChanged(wifiSettingsStatus, status.c_str());
  }
}

static void wifiPowerEvent(lv_event_t* e) {
  (void)e;
  if(!wifiChangeAllowed()){refreshWifiSettingsUi(true);return;}
  const bool enabled = lv_obj_has_state(wifiPowerSwitch, LV_STATE_CHECKED);
  if (!enabled) stopNetworkTime();
  Tab5Audio.setWifiEnabled(enabled);
  refreshWifiSettingsUi(true);
  updateTopAudioIndicator(true);
}

static void wifiAutoStartEvent(lv_event_t* e) {
  lv_obj_t* target = lv_event_get_target_obj(e);
  Tab5Audio.setWifiAutoStart(target &&
      lv_obj_has_state(target, LV_STATE_CHECKED));
  refreshWifiSettingsUi(true);
}

static void wifiSettingsEvent(lv_event_t* e) {
  if(!wifiChangeAllowed())return;
  (void)e;
  showWifiPopup();
  refreshWifiSettingsUi(true);
}

static void updateAudioUi(bool force) {
  if (!audioPage) return;
  if(activePage==Page::Audio && !videoViewerActive && audioRows && !audioListRebuilding){
    const uint32_t revision=audioCatalogueRevision(audioRenderedView);
    if(revision && (audioRefreshPending || revision!=audioRenderedRevision))refreshAudioList();
  }
  static uint32_t seen = 0;
  static uint32_t lastApplied = 0;
  static bool forcePending = false, videoPending = false;
  forcePending = forcePending || force;
  // The full-screen viewer owns video notifications while its screen is active.
  if (videoViewerActive) return;
  const bool videoView = audioView == AudioView::Videos;
  if (videoView) videoPending = Tab5Video.takeUiDirty() || videoPending;
  const uint32_t now = millis();
  // Coalesce progress/meter events to the existing 4 Hz presentation rate.
  // An unchanged revision still performs no copy when the interval expires.
  if (!forcePending && !videoPending && now - lastApplied < 250) return;
  static Tab5AudioSnapshot audioState;
  const bool audioChanged = Tab5Audio.mediaSnapshot(audioState, seen, forcePending || videoPending);
  if (!audioChanged) return;
  Tab5VideoSnapshot videoState;
  const bool haveVideoState = videoView && Tab5Video.snapshot(videoState);
  if (videoView && !haveVideoState) { forcePending = true; return; }
  forcePending = false;
  videoPending = false;
  lastApplied = now;
  audioRememberPlayback(audioState);

  String current = haveVideoState && videoState.name.length()
      ? videoState.name : audioState.nowPlaying;
  if((!videoView&&audioState.mode==Tab5AudioMode::Idle)||(videoView&&(!haveVideoState||videoState.state==Tab5VideoState::Idle)))
    current=Tab5MediaSession::state().title[unsigned(audioView)];
  if (!current.length())
    current = videoView ? T("Sélectionnez une vidéo") : T("Aucune lecture");
  setLabelIfChanged(audioNowPlaying, current.c_str());

  String state = haveVideoState ? videoState.status : audioState.status;
  if (videoView && (!haveVideoState || videoState.state == Tab5VideoState::Idle))
    state = "";
  if((!videoView&&audioState.mode==Tab5AudioMode::Idle)||(videoView&&(!haveVideoState||videoState.state==Tab5VideoState::Idle)))
    if(Tab5MediaSession::state().selected[unsigned(audioView)][0])state=T("Sélection mémorisée");
  if (Tab5Locale.hasTranslations()) {
    const String translatedState = translateRuntimeText(state);
    setLabelIfChanged(audioStatus, translatedState.c_str());
  } else {
    setLabelIfChanged(audioStatus, state.c_str());
  }

  uint16_t progress = 0;
  uint32_t elapsedSeconds = 0;
  uint32_t totalSeconds = 0;
  if (audioState.recording) {
    elapsedSeconds = audioState.recordingSeconds;
  } else {
    progress = audioState.playbackProgressPermille;
    elapsedSeconds = audioState.playbackElapsedSeconds;
    totalSeconds = audioState.playbackEstimatedSeconds;
  }
  const bool canSeek = audioState.mode == Tab5AudioMode::Local &&
      totalSeconds > 0 && audioPathIsSeekable(audioState.currentPath);
  setButtonEnabled(audioProgressSlider, canSeek);
  if (!audioProgressDragging)
    lv_slider_set_value(audioProgressSlider, progress, LV_ANIM_OFF);
  char timeText[40];
  if (audioState.mode == Tab5AudioMode::Radio) {
    snprintf(timeText, sizeof(timeText), T("DIRECT  •  %02lu:%02lu"),
             static_cast<unsigned long>(elapsedSeconds / 60),
             static_cast<unsigned long>(elapsedSeconds % 60));
  } else if (audioState.recording) {
    snprintf(timeText, sizeof(timeText), T("REC  •  %02lu:%02lu"),
             static_cast<unsigned long>(elapsedSeconds / 60),
             static_cast<unsigned long>(elapsedSeconds % 60));
  } else if (audioState.mode == Tab5AudioMode::Local && totalSeconds) {
    snprintf(timeText, sizeof(timeText), "%02lu:%02lu  /  %02lu:%02lu",
             static_cast<unsigned long>(elapsedSeconds / 60),
             static_cast<unsigned long>(elapsedSeconds % 60),
             static_cast<unsigned long>(totalSeconds / 60),
             static_cast<unsigned long>(totalSeconds % 60));
  } else {
    snprintf(timeText, sizeof(timeText), "%02lu:%02lu",
             static_cast<unsigned long>(elapsedSeconds / 60),
             static_cast<unsigned long>(elapsedSeconds % 60));
  }
  setLabelIfChanged(audioTimeLabel, timeText);

  setIconButtonSymbol(audioPlayBtn,
      audioState.playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
  setButtonEnabled(audioPlayBtn, !audioState.recording && !videoView);
  setTopActionVisible(audioStopBtn, audioState.mode != Tab5AudioMode::Idle);

  const bool recording = audioState.recording;
  lv_obj_t* recordLabel = audioRecordBtn ? lv_obj_get_child(audioRecordBtn, 0) : nullptr;
  if (recordLabel) {
    if (recording) {
      setLabelIfChanged(recordLabel, LV_SYMBOL_STOP);
      lv_obj_remove_flag(recordLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(recordLabel, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (audioRecordGraphic) {
    if (recording) lv_obj_add_flag(audioRecordGraphic, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(audioRecordGraphic, LV_OBJ_FLAG_HIDDEN);
  }
  const bool showRecordControls = audioView == AudioView::Recordings || recording;
  setTopActionVisible(audioRecordControls, showRecordControls);
  const int volumeY = showRecordControls ? 490 : 390;
  if (audioVolumeIcon) lv_obj_set_pos(audioVolumeIcon, 2, volumeY);
  if (audioVolumeValue &&
      (lv_obj_get_style_align(audioVolumeValue, LV_PART_MAIN) != LV_ALIGN_TOP_RIGHT ||
       lv_obj_get_style_x(audioVolumeValue, LV_PART_MAIN) != -2 ||
       lv_obj_get_style_y(audioVolumeValue, LV_PART_MAIN) != volumeY))
    lv_obj_align(audioVolumeValue, LV_ALIGN_TOP_RIGHT, -2, volumeY);
  if (audioVolumeSlider) lv_obj_set_pos(audioVolumeSlider, 2, volumeY + 48);
  lv_bar_set_value(audioLevelBar, audioState.recordingLevel, LV_ANIM_OFF);
  if (audioVolumeSlider &&
      lv_slider_get_value(audioVolumeSlider) != audioState.volume) {
    lv_slider_set_value(audioVolumeSlider, audioState.volume, LV_ANIM_OFF);
  }
  char volumeText[16];
  snprintf(volumeText, sizeof(volumeText), "%u%%",
           static_cast<unsigned>((audioState.volume * 100U) / 255U));
  setLabelIfChanged(audioVolumeValue, volumeText);
  updateTopAudioIndicator(false);
}

static void openAudioPath(const String& path) {
  Tab5Video.stop();
  audioView = audioPathIsRecording(path) ? AudioView::Recordings : AudioView::Local;
  if (audioView == AudioView::Local) audioAlbumFolder = parentDirOf(path);
  Tab5MediaSession::select(unsigned(audioView),path.c_str(),baseNameOf(path).c_str(),millis());
  Tab5MediaSession::scroll(unsigned(audioView),0,millis());
  Tab5Audio.playPath(path);
  showPage(Page::Audio);
  updateAudioUi(true);
}

static void createAudioPage() {
  audioPage = lv_obj_create(pageHost);
  lv_obj_set_size(audioPage, WORK_W, PAGE_H);Tab5Shell::width(audioPage,0);
  lv_obj_set_pos(audioPage, 0, 0);
  lv_obj_set_style_border_width(audioPage, 0, 0);
  lv_obj_set_style_radius(audioPage, 0, 0);
  lv_obj_set_style_pad_all(audioPage, 10, 0);
  lv_obj_set_style_bg_color(audioPage, C_BG(), 0);Tab5ThemeLive::bind(audioPage,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(audioPage, LV_OPA_COVER, 0);
  lv_obj_remove_flag(audioPage, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* sourcePanel = lv_obj_create(audioPage);
  lv_obj_set_size(sourcePanel, 180, PAGE_H - 20);
  lv_obj_set_pos(sourcePanel, 0, 0);
  lv_obj_set_style_bg_color(sourcePanel, C_SURFACE(), 0);
  lv_obj_set_style_bg_opa(sourcePanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(sourcePanel, C_BORDER(), 0);
  lv_obj_set_style_radius(sourcePanel, 12, 0);
  lv_obj_set_style_pad_all(sourcePanel, 0, 0);
  lv_obj_remove_flag(sourcePanel, LV_OBJ_FLAG_SCROLLABLE);

  audioLocalTab = makeIconButton(sourcePanel, LV_SYMBOL_AUDIO, 140, 72);
  lv_obj_set_pos(audioLocalTab, 20, 16);
  lv_obj_add_event_cb(audioLocalTab, audioLocalTabEvent, LV_EVENT_CLICKED, nullptr);

  audioRadioTab = makeIconButton(sourcePanel, LV_SYMBOL_WIFI, 140, 72);
  lv_obj_set_pos(audioRadioTab, 20, 98);
  lv_obj_add_event_cb(audioRadioTab, audioRadioTabEvent, LV_EVENT_CLICKED, nullptr);

  audioRecordTab = makeMicrophoneButton(sourcePanel, 140, 72, nullptr);
  lv_obj_set_pos(audioRecordTab, 20, 180);
  lv_obj_add_event_cb(audioRecordTab, audioRecordTabEvent, LV_EVENT_CLICKED, nullptr);

  audioVideoTab = makeIconButton(sourcePanel, LV_SYMBOL_IMAGE, 140, 72);
  lv_obj_set_pos(audioVideoTab, 20, 262);
  lv_obj_add_event_cb(audioVideoTab, audioVideoTabEvent, LV_EVENT_CLICKED, nullptr);

  audioFavoritesTab=Tab5MediaList::starButton(sourcePanel,140,54);lv_obj_set_pos(audioFavoritesTab,20,352);
  Tab5Focus.describe(audioFavoritesTab,T("Favoris"));
  lv_obj_add_event_cb(audioFavoritesTab,[](lv_event_t*){if(unsigned(audioView)<2){audioFavoritesOnly[unsigned(audioView)]=!audioFavoritesOnly[unsigned(audioView)];Tab5MediaSession::scroll(unsigned(audioView),0,millis());refreshAudioList();}},LV_EVENT_CLICKED,nullptr);

  audioNewFolder=makeIconButton(sourcePanel,LV_SYMBOL_PLUS,64,54);lv_obj_set_pos(audioNewFolder,58,418);Tab5Focus.describe(audioNewFolder,T("Créer une sous-catégorie"));
  lv_obj_add_event_cb(audioNewFolder,[](lv_event_t*){String folder=audioAlbumFolder.length()?audioAlbumFolder:String("/mp3");if(!tab5FS.exists(folder))tab5FS.mkdir(folder);openTextDialog(TextDialogAction::NewFolder,"Nouvelle catégorie","",folder);},LV_EVENT_CLICKED,nullptr);
  lv_obj_t* reload = makeIconButton(sourcePanel, LV_SYMBOL_REFRESH, 64, 54);
  lv_obj_align(reload, LV_ALIGN_BOTTOM_MID, 0, -22);
  lv_obj_add_event_cb(reload, audioReloadEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* libraryPanel = lv_obj_create(audioPage);
  lv_obj_set_size(libraryPanel, 570, PAGE_H - 20);
  lv_obj_set_pos(libraryPanel, 190, 0);
  lv_obj_set_style_bg_color(libraryPanel, C_SURFACE(), 0);
  lv_obj_set_style_bg_opa(libraryPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(libraryPanel, C_BORDER(), 0);
  lv_obj_set_style_radius(libraryPanel, 12, 0);
  lv_obj_set_style_pad_all(libraryPanel, 12, 0);
  lv_obj_remove_flag(libraryPanel, LV_OBJ_FLAG_SCROLLABLE);

  audioListTitle = lv_label_create(libraryPanel);
  lv_obj_set_pos(audioListTitle, 8, 4);
  lv_obj_set_width(audioListTitle, 424);
  lv_label_set_long_mode(audioListTitle,LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_font(audioListTitle, uiHeadingFont(), 0);
  lv_obj_set_style_text_color(audioListTitle, C_TEXT(), 0);
  auto* search=makeSearchButton(libraryPanel);lv_obj_set_pos(search,498,0);lv_obj_add_event_cb(search,audioSearchEvent,LV_EVENT_CLICKED,nullptr);
  Tab5Focus.describe(search,T("Rechercher · Ctrl+F"));
  audioSearchClear=makeIconButton(libraryPanel,LV_SYMBOL_CLOSE,40,36);lv_obj_set_pos(audioSearchClear,452,0);lv_obj_add_flag(audioSearchClear,LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(audioSearchClear,[](lv_event_t*){audioSearchQueries[unsigned(audioView)]="";refreshAudioList();},LV_EVENT_CLICKED,nullptr);

  audioList = lv_obj_create(libraryPanel);
  lv_obj_add_event_cb(audioList,[](lv_event_t*){audioRememberScroll();},LV_EVENT_SCROLL_END,nullptr);
  lv_obj_set_size(audioList, LV_PCT(100), PAGE_H - 154);
  lv_obj_set_pos(audioList, 0, 104);
  lv_obj_set_style_bg_color(audioList, C_SURFACE(), 0);Tab5ThemeLive::bind(audioList,LV_STYLE_BG_COLOR,0,1);
  lv_obj_set_style_border_width(audioList, 0, 0);
  lv_obj_set_style_pad_all(audioList, 0, 0);
  lv_obj_set_style_pad_right(audioList, 28, 0);

  audioFavoritesSelector=lv_dropdown_create(libraryPanel);lv_obj_set_size(audioFavoritesSelector,200,46);lv_obj_set_pos(audioFavoritesSelector,342,50);
  lv_obj_set_style_text_font(audioFavoritesSelector,uiFont(),0);
  String favoriteOptions=String(T("Tous"))+"\n"+T("Favoris");lv_dropdown_set_options(audioFavoritesSelector,favoriteOptions.c_str());Tab5UiStyle::control(audioFavoritesSelector);
  Tab5Focus.describe(audioFavoritesSelector,T("Filtrer les médias"));
  lv_obj_add_event_cb(audioFavoritesSelector,[](lv_event_t* e){if(unsigned(audioView)<2){audioFavoritesOnly[unsigned(audioView)]=lv_dropdown_get_selected(lv_event_get_target_obj(e))==1;Tab5MediaSession::scroll(unsigned(audioView),0,millis());refreshAudioList();}},LV_EVENT_VALUE_CHANGED,nullptr);
  audioSortSelector=lv_dropdown_create(libraryPanel);lv_obj_set_size(audioSortSelector,330,46);lv_obj_set_pos(audioSortSelector,0,50);
  lv_obj_set_style_text_font(audioSortSelector,uiFont(),0);Tab5UiStyle::control(audioSortSelector);Tab5Focus.describe(audioSortSelector,T("Trier par"));
  lv_obj_add_event_cb(audioSortSelector,[](lv_event_t* e){Tab5FileSort::selected=static_cast<Tab5FileSort::Order>(lv_dropdown_get_selected(lv_event_get_target_obj(e)));audioRememberScroll();refreshAudioList();},LV_EVENT_VALUE_CHANGED,nullptr);
  lv_obj_t* playerPanel = lv_obj_create(audioPage);
  lv_obj_set_size(playerPanel, WORK_W - 790, PAGE_H - 20);Tab5Shell::width(playerPanel,-790);
  lv_obj_set_pos(playerPanel, 770, 0);
  lv_obj_set_style_bg_color(playerPanel, C_SURFACE(), 0);
  lv_obj_set_style_bg_opa(playerPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(playerPanel, C_BORDER(), 0);
  lv_obj_set_style_radius(playerPanel, 12, 0);
  lv_obj_set_style_pad_all(playerPanel, 18, 0);
  lv_obj_remove_flag(playerPanel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* nowCaption = lv_label_create(playerPanel);
  lv_label_set_text(nowCaption, T("EN COURS"));
  lv_obj_set_style_text_color(nowCaption, C_MUTED(), 0);
  lv_obj_set_pos(nowCaption, 2, 2);

  audioNowPlaying = lv_label_create(playerPanel);
  lv_obj_set_width(audioNowPlaying, WORK_W - 834);Tab5Shell::width(audioNowPlaying,-834);
  lv_obj_set_height(audioNowPlaying, 100);
  lv_label_set_long_mode(audioNowPlaying, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(audioNowPlaying, uiHeadingFont(), 0);
  lv_obj_set_style_text_color(audioNowPlaying, C_TEXT(), 0);
  lv_obj_set_pos(audioNowPlaying, 2, 32);

  audioStatus = lv_label_create(playerPanel);
  lv_obj_set_width(audioStatus, WORK_W - 834);Tab5Shell::width(audioStatus,-834);
  lv_label_set_long_mode(audioStatus, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(audioStatus, C_MUTED(), 0);
  lv_obj_set_pos(audioStatus, 2, 140);

  audioProgressSlider = lv_slider_create(playerPanel);
  lv_obj_set_size(audioProgressSlider, WORK_W - 840, 28);Tab5Shell::width(audioProgressSlider,-840);
  lv_obj_set_pos(audioProgressSlider, 2, 198);
  lv_slider_set_range(audioProgressSlider, 0, 1000);
  lv_slider_set_value(audioProgressSlider, 0, LV_ANIM_OFF);
  styleSettingsSlider(audioProgressSlider);
  lv_obj_set_style_radius(audioProgressSlider, LV_RADIUS_CIRCLE,
                          LV_PART_MAIN);
  lv_obj_set_style_radius(audioProgressSlider, LV_RADIUS_CIRCLE,
                          LV_PART_INDICATOR);
  lv_obj_set_style_width(audioProgressSlider, 24, LV_PART_KNOB);
  lv_obj_set_style_height(audioProgressSlider, 24, LV_PART_KNOB);
  lv_obj_set_style_border_width(audioProgressSlider, 3, LV_PART_KNOB);
  lv_obj_set_style_border_color(audioProgressSlider, C_ACTIVE(), LV_PART_KNOB);
  lv_obj_set_style_bg_opa(audioProgressSlider, LV_OPA_TRANSP,
                          LV_PART_KNOB | LV_STATE_DISABLED);
  lv_obj_set_style_opa(audioProgressSlider, LV_OPA_TRANSP,
                       LV_PART_KNOB | LV_STATE_DISABLED);
  lv_obj_set_style_bg_opa(audioProgressSlider, LV_OPA_50,
                          LV_PART_INDICATOR | LV_STATE_DISABLED);
  lv_obj_add_event_cb(audioProgressSlider, audioProgressEvent, LV_EVENT_ALL,
                      nullptr);

  audioTimeLabel = lv_label_create(playerPanel);
  lv_obj_set_width(audioTimeLabel, WORK_W - 840);Tab5Shell::width(audioTimeLabel,-840);
  lv_obj_set_style_text_align(audioTimeLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(audioTimeLabel, C_MUTED(), 0);
  lv_obj_set_pos(audioTimeLabel, 2, 230);

  lv_obj_t* controls = lv_obj_create(playerPanel);
  lv_obj_remove_style_all(controls);
  lv_obj_set_size(controls, WORK_W - 830, 66);Tab5Shell::width(controls,-830);
  lv_obj_set_pos(controls, 0, 278);
  lv_obj_set_flex_flow(controls, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(controls, LV_FLEX_ALIGN_END,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(controls,8,0);
  lv_obj_remove_flag(controls, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* previous = makeIconButton(controls, LV_SYMBOL_PREV, 58, 56);
  audioPlayBtn = makeIconButton(controls, LV_SYMBOL_PLAY, 64, 60);
  lv_obj_t* next = makeIconButton(controls, LV_SYMBOL_NEXT, 58, 56);
  audioStopBtn = makeIconButton(controls, LV_SYMBOL_STOP, 58, 56);
  lv_obj_add_flag(audioStopBtn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(previous, audioPreviousEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(audioPlayBtn, audioPlayEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(next, audioNextEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(audioStopBtn, audioStopEvent, LV_EVENT_CLICKED, nullptr);

  audioRecordControls = lv_obj_create(playerPanel);
  lv_obj_remove_style_all(audioRecordControls);
  lv_obj_set_size(audioRecordControls, WORK_W - 830, 80);Tab5Shell::width(audioRecordControls,-830);
  lv_obj_set_pos(audioRecordControls, 0, 370);
  lv_obj_remove_flag(audioRecordControls, LV_OBJ_FLAG_SCROLLABLE);

  audioRecordBtn = makeMicrophoneButton(audioRecordControls, 82, 64,
                                        &audioRecordGraphic);
  lv_obj_set_pos(audioRecordBtn, 2, 4);
  lv_obj_set_style_bg_color(audioRecordBtn, lv_color_hex(0x5A2428), 0);
  lv_obj_add_event_cb(audioRecordBtn, audioRecordEvent, LV_EVENT_CLICKED, nullptr);

  audioLevelBar = lv_bar_create(audioRecordControls);
  lv_obj_set_size(audioLevelBar, WORK_W - 936, 22);Tab5Shell::width(audioLevelBar,-936);
  lv_obj_set_pos(audioLevelBar, 100, 25);
  lv_bar_set_range(audioLevelBar, 0, 32767);
  lv_obj_set_style_bg_color(audioLevelBar, C_RAISED(), LV_PART_MAIN);
  lv_obj_set_style_bg_color(audioLevelBar, C_ACTIVE(), LV_PART_INDICATOR);

  audioVolumeIcon = lv_label_create(playerPanel);
  lv_label_set_text(audioVolumeIcon, LV_SYMBOL_VOLUME_MAX);
  lv_obj_set_style_text_font(audioVolumeIcon, &lv_font_montserrat_24, 0);
  lv_obj_set_pos(audioVolumeIcon, 2, 490);

  audioVolumeValue = lv_label_create(playerPanel);
  lv_obj_set_width(audioVolumeValue, 72);
  lv_obj_set_style_text_align(audioVolumeValue, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(audioVolumeValue, C_MUTED(), 0);
  lv_obj_align(audioVolumeValue, LV_ALIGN_TOP_RIGHT, -2, 490);

  audioVolumeSlider = lv_slider_create(playerPanel);
  lv_obj_set_size(audioVolumeSlider, WORK_W - 840, 26);Tab5Shell::width(audioVolumeSlider,-840);
  lv_obj_set_pos(audioVolumeSlider, 2, 538);
  lv_slider_set_range(audioVolumeSlider, 0, 255);
  lv_slider_set_value(audioVolumeSlider, settings.audioVolume, LV_ANIM_OFF);
  styleSettingsSlider(audioVolumeSlider);
  lv_obj_add_event_cb(audioVolumeSlider,[](lv_event_t*){Tab5MediaSession::flush(millis());},LV_EVENT_RELEASED,nullptr);
  lv_obj_add_event_cb(audioVolumeSlider, audioVolumeEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);

  // Catalogues are loaded on the first visit to Media, after the home is visible.
  updateAudioUi(true);
}

// -----------------------------------------------------------------------------
// OpenStreetMap raster viewer
// -----------------------------------------------------------------------------

static constexpr double MAP_MAX_LATITUDE = 85.05112878;
static constexpr double MAP_PI = 3.14159265358979323846;
static constexpr uint8_t MAP_MIN_ZOOM = 0;
static constexpr uint8_t MAP_MAX_ZOOM = Tab5MapViewPolicy::MaxZoom;

static bool mapNetworkConnected() {
  // WiFi.status() reads the SDK event cache. Audio may be busy opening/closing
  // a stream and has not necessarily published the latest network transition.
  return tab5NetworkEnabled() && WiFi.status() == WL_CONNECTED;
}

static bool mapDownloadsAllowed(bool* connectedOut = nullptr) {
  Tab5AudioRoutingSnapshot state{};
  const bool haveState = Tab5Audio.routingSnapshot(state);
  const bool connected = mapNetworkConnected();
  if (connectedOut) *connectedOut = connected;
  if (!haveState) return false;
  // A paused local decoder has no SD reads. Recording and active local
  // playback keep their existing storage priority; this is not Wi-Fi state.
  return state.mode != Tab5AudioMode::Recording &&
      (state.mode != Tab5AudioMode::Local || state.paused);
}

static void clampMapView() {
  mapLatitude = constrain(mapLatitude, -MAP_MAX_LATITUDE, MAP_MAX_LATITUDE);
  while (mapLongitude < -180.0) mapLongitude += 360.0;
  while (mapLongitude >= 180.0) mapLongitude -= 360.0;
  const uint8_t styleMax = min(MAP_MAX_ZOOM,
      Tab5Map.styleViewMaxZoom(Tab5Map.selectedStyle()));
  mapZoom = static_cast<uint8_t>(constrain(
      static_cast<int>(mapZoom), static_cast<int>(Tab5Map.styleMinZoom(Tab5Map.selectedStyle())),
      static_cast<int>(styleMax)));
}

static void syncMapZoomControl() {
  if (!mapZoomSlider || !mapZoomValueLabel) return;
  const uint8_t styleMax = min(MAP_MAX_ZOOM,
      Tab5Map.styleViewMaxZoom(Tab5Map.selectedStyle()));
  lv_slider_set_range(mapZoomSlider, Tab5Map.styleMinZoom(Tab5Map.selectedStyle()), styleMax);
  if (!mapZoomDragging)
    lv_slider_set_value(mapZoomSlider, mapZoom, LV_ANIM_OFF);
  const int shown = mapZoomDragging
      ? lv_slider_get_value(mapZoomSlider) : mapZoom;
  char label[8];
  snprintf(label, sizeof(label), "z%d", shown);
  lv_label_set_text(mapZoomValueLabel, label);
}

static void mapZoomSliderEvent(lv_event_t* event) {
  const lv_event_code_t code = lv_event_get_code(event);
  if (code == LV_EVENT_PRESSED) {
    mapZoomDragging = true;
    syncMapZoomControl();
    return;
  }
  if (code == LV_EVENT_VALUE_CHANGED && mapZoomDragging) {
    syncMapZoomControl();
    return;
  }
  // Physical keyboard changes do not generate pointer press/release events.
  if (code == LV_EVENT_RELEASED || (code == LV_EVENT_VALUE_CHANGED && !mapZoomDragging)) {
    const uint8_t requested = static_cast<uint8_t>(
        lv_slider_get_value(mapZoomSlider));
    mapZoomDragging = false;
    if (requested != mapZoom) {
      mapZoom = requested;
      markMapViewDirty();
      renderMapViewport(false);
    } else {
      syncMapZoomControl();
    }
  } else if (code == LV_EVENT_CANCEL) {
    mapZoomDragging = false;
    syncMapZoomControl();
  }
}

static double mapWorldX(double longitude, uint8_t zoom) {
  const double world = 256.0 * static_cast<double>(1UL << zoom);
  return (longitude + 180.0) / 360.0 * world;
}

static double mapWorldY(double latitude, uint8_t zoom) {
  latitude = constrain(latitude, -MAP_MAX_LATITUDE, MAP_MAX_LATITUDE);
  const double radians = latitude * MAP_PI / 180.0;
  const double mercator = log(tan(radians) + 1.0 / cos(radians));
  const double world = 256.0 * static_cast<double>(1UL << zoom);
  return (1.0 - mercator / MAP_PI) * 0.5 * world;
}

static void mapCoordinatesFromWorld(double worldX, double worldY,
                                    uint8_t zoom, double& latitude,
                                    double& longitude) {
  const double world = 256.0 * static_cast<double>(1UL << zoom);
  while (worldX < 0.0) worldX += world;
  while (worldX >= world) worldX -= world;
  worldY = constrain(worldY, 0.0, world - 1.0);
  longitude = worldX / world * 360.0 - 180.0;
  const double mercator = MAP_PI * (1.0 - 2.0 * worldY / world);
  latitude = atan(sinh(mercator)) * 180.0 / MAP_PI;
}

static uint8_t mapSourceZoom() {
  return Tab5Map.styleSourceZoom(Tab5Map.selectedStyle(), mapZoom);
}

static int mapSourceTileScreenSize() {
  return Tab5MapViewPolicy::tilePixels(mapZoom, mapSourceZoom());
}

static void mapVisibleBounds(int64_t& firstX, int64_t& lastX,
                             int64_t& firstY, int64_t& lastY,
                             double& left, double& top) {
  const double centerX = mapWorldX(mapLongitude, mapZoom);
  const double centerY = mapWorldY(mapLatitude, mapZoom);
  left = centerX - MAP_VIEW_W * 0.5;
  top = centerY - MAP_VIEW_H * 0.5;
  const double tileSize = mapSourceTileScreenSize();
  firstX = static_cast<int64_t>(floor(left / tileSize));
  lastX = static_cast<int64_t>(floor((left + MAP_VIEW_W - 1) / tileSize));
  firstY = static_cast<int64_t>(floor(top / tileSize));
  lastY = static_cast<int64_t>(floor((top + MAP_VIEW_H - 1) / tileSize));
}

static bool ensureMapCanvas() {
  if (mapCanvas) return true;
  auto admission=tab5ReserveMemory(Tab5Resources::Owner::Map,
      {0,0,0,size_t(MAP_VIEW_W)*MAP_VIEW_H*2,size_t(MAP_VIEW_W)*MAP_VIEW_H*2},"canevas carte");
  if(!admission)return false;
  Tab5DebugSerial.printf("[MAP][OPEN] allocation : interne=%u Ko, PSRAM=%u Ko, "
                "bloc PSRAM=%u Ko\n",
                static_cast<unsigned>(heap_caps_get_free_size(
                    MALLOC_CAP_INTERNAL) / 1024U),
                static_cast<unsigned>(heap_caps_get_free_size(
                    MALLOC_CAP_SPIRAM) / 1024U),
                static_cast<unsigned>(heap_caps_get_largest_free_block(
                    MALLOC_CAP_SPIRAM) / 1024U));
  if (!mapCanvas) {
    mapCanvas = new (std::nothrow) M5Canvas(&M5.Display);
    if (mapCanvas) {
      mapCanvas->setPsram(true);
      mapCanvas->setColorDepth(16);
      if (!mapCanvas->createSprite(MAP_VIEW_W, MAP_VIEW_H)) {
        delete mapCanvas;
        mapCanvas = nullptr;
      }
    }
    Tab5DebugSerial.printf("[MAP][OPEN] canevas %dx%d RGB565 : %s\n",
                  MAP_VIEW_W, MAP_VIEW_H, mapCanvas ? "OK" : "échec");
  }
  if (!mapCanvas) return false;
  mapCanvas->setSwapBytes(false);
  memset(&mapImageDsc, 0, sizeof(mapImageDsc));
  mapImageDsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  mapImageDsc.header.cf = LV_COLOR_FORMAT_RGB565_SWAPPED;
  mapImageDsc.header.flags = 0;
  mapImageDsc.header.w = MAP_VIEW_W;
  mapImageDsc.header.h = MAP_VIEW_H;
  mapImageDsc.header.stride = MAP_VIEW_W * 2;
  mapImageDsc.data_size = static_cast<uint32_t>(MAP_VIEW_W * MAP_VIEW_H * 2);
  mapImageDsc.data = static_cast<const uint8_t*>(mapCanvas->getBuffer());
  Tab5DebugSerial.println("[MAP][OPEN] liaison du canevas à LVGL");
  if (mapImage) lv_image_set_src(mapImage, &mapImageDsc);
  Tab5DebugSerial.println("[MAP][OPEN] canevas LVGL : OK");
  return true;
}

static void freeMapCanvas() {
  mapRenderedGeneration = 0;
  mapRasterCount = mapRasterIndex = 0;
  if (mapPlacesDeleteOverlay && lv_obj_is_valid(mapPlacesDeleteOverlay))
    lv_obj_delete(mapPlacesDeleteOverlay);
  if (mapPlacesOverlay && lv_obj_is_valid(mapPlacesOverlay))
    lv_obj_delete(mapPlacesOverlay);
  if (mapStylesOverlay && lv_obj_is_valid(mapStylesOverlay))
    lv_obj_delete(mapStylesOverlay);
  if (mapSearchOverlay && lv_obj_is_valid(mapSearchOverlay))
    lv_obj_delete(mapSearchOverlay);
  if (mapDownloadOverlay && lv_obj_is_valid(mapDownloadOverlay))
    lv_obj_delete(mapDownloadOverlay);
  mapPlacesDeleteOverlay = nullptr;
  mapPlacesOverlay = nullptr;
  mapPlacesList = nullptr;
  mapSelectedPlaceButton = nullptr;
  mapStylesOverlay = nullptr;
  mapSearchOverlay = nullptr;
  mapDownloadOverlay = nullptr;
  if (mapImage && lv_obj_is_valid(mapImage)) lv_image_set_src(mapImage, nullptr);
  if (mapCanvas) {
    mapCanvas->deleteSprite();
    delete mapCanvas;
    mapCanvas = nullptr;
  }
  memset(&mapImageDsc, 0, sizeof(mapImageDsc));
  Tab5Map.clearMemoryCache();
}

static void updateMapAttribution() {
  if (!mapAttributionLabel) return;
  const String attribution = Tab5Map.attribution();
  lv_label_set_text(mapAttributionLabel, attribution.c_str());
}

static void updateMapLabels() {
  if (mapInfoLabel) {
    char coordinates[72];
    snprintf(coordinates, sizeof(coordinates), "z%u  •  %.4f, %.4f",
             static_cast<unsigned>(mapZoom), mapLatitude, mapLongitude);
    setLabelIfChanged(mapInfoLabel, coordinates);
  }
  if (!mapStatusLabel) return;
  bool connected = false;
  const bool allowed = mapDownloadsAllowed(&connected);
  String status = Tab5Map.styleName(Tab5Map.selectedStyle());
  if (!Tab5Map.ready()) status = T("microSD absente");
  else if (mapSearchPending) status = T("Recherche…");
  else if (mapTilesPending) {
    status = T("Chargement");
    status += "  •  " + String(mapTilesReady) + "/" + String(mapTilesNeeded);
  } else if (mapTilesReady < mapTilesNeeded) {
    status = !connected ? T("Hors ligne") : !allowed ? T("Téléchargement indisponible : ressources occupées") : T("Carte incomplète");
    status += "  •  " + String(mapTilesReady) + "/" + String(mapTilesNeeded);
    if (mapLastTlsError == -32512 || mapLastHttpError == -103)
      status += T("  •  mémoire insuffisante");
    else if(mapLastHttpError==-109)status+=T("  •  réseau différé : mémoire occupée");
    else if (mapLastHttpError && mapLastHttpError != -107)
      status += "  •  HTTP " + String(mapLastHttpError);
  } else if (Tab5Map.offlinePackCountForSelectedStyle()) status += "  •  SD";
  setLabelIfChanged(mapStatusLabel, status.c_str());
}

static bool drawMapTile(uint8_t zoom, uint32_t x, uint32_t y,
                        const uint8_t* data, size_t size) {
  const uint8_t sourceZoom = mapSourceZoom();
  if (!mapCanvas || zoom != sourceZoom || !data || size < 8) return false;
  const int tileScreenSize = mapSourceTileScreenSize();
  const int64_t tileCount = 1LL << zoom;
  const double centerTileX = mapWorldX(mapLongitude, mapZoom) /
                             static_cast<double>(tileScreenSize);
  int64_t rawX = static_cast<int64_t>(x);
  rawX += static_cast<int64_t>(llround(
      (centerTileX - static_cast<double>(rawX)) /
      static_cast<double>(tileCount))) * tileCount;
  // L'espace écran reste exprimé au zoom demandé. Le mode rapide charge une
  // tuile d'un niveau inférieur, puis l'agrandit dans ce même espace.
  const double left = mapWorldX(mapLongitude, mapZoom) - MAP_VIEW_W * 0.5;
  const double top = mapWorldY(mapLatitude, mapZoom) - MAP_VIEW_H * 0.5;
  const int destinationX = static_cast<int>(
      lround(rawX * static_cast<double>(tileScreenSize) - left));
  const int destinationY = static_cast<int>(
      lround(y * static_cast<double>(tileScreenSize) - top));
  if (destinationX >= MAP_VIEW_W || destinationY >= MAP_VIEW_H ||
      destinationX + tileScreenSize <= 0 ||
      destinationY + tileScreenSize <= 0) return false;

  const int drawX = max(0, destinationX);
  const int drawY = max(0, destinationY);
  const int drawW = min(tileScreenSize - max(0, -destinationX),
                        MAP_VIEW_W - drawX);
  const int drawH = min(tileScreenSize - max(0, -destinationY),
                        MAP_VIEW_H - drawY);
  if (drawW <= 0 || drawH <= 0) return false;
  const bool ok=Tab5TileRaster::draw(static_cast<uint16_t*>(mapCanvas->getBuffer()),
      MAP_VIEW_W,MAP_VIEW_H,data,size,destinationX,destinationY,tileScreenSize);
  if(!ok)Tab5Map.rejectCachedTile(uint8_t(Tab5Map.selectedStyle()),zoom,x,y,data,size);
  if(ok&&mapImage){
    lv_area_t area;lv_obj_get_coords(mapImage,&area);
    area.x1+=drawX;area.y1+=drawY;
    area.x2=area.x1+drawW-1;area.y2=area.y1+drawH-1;
    lv_obj_invalidate_area(mapImage,&area);
  }
  return ok;
}







static void renderMapViewport(bool refreshTiles, bool preserveCanvas) {
  clampMapView();
  syncMapZoomControl();
  const bool keepCanvas = preserveCanvas && !refreshTiles && mapCanvas &&
      mapRenderedGeneration && mapRenderedGeneration == mapGeneration &&
      Tab5MapWheel::same(mapRenderedView, {mapLatitude,mapLongitude,mapZoom}) &&
      mapRenderedStyle == Tab5Map.selectedStyle() &&
      mapRenderedWidth == MAP_VIEW_W && mapRenderedHeight == MAP_VIEW_H;
  // Failed preparation must never leave an old frame eligible for reuse.
  mapRenderedGeneration = 0;
  if (!Tab5Map.ready() || !mapRasterJobs) { updateMapLabels(); return; }
  if (!ensureMapCanvas()) {
    if (mapStatusLabel) setLabelIfChanged(mapStatusLabel, T("Mémoire insuffisante pour la carte"));
    return;
  }
  Tab5MapOverlay::viewport(mapLatitude,mapLongitude,mapZoom,MAP_VIEW_W,MAP_VIEW_H);
  ++mapGeneration;
  if (!mapGeneration) ++mapGeneration;
  Tab5Map.beginGeneration(mapGeneration);
  mapRasterIndex = mapRasterCount = 0;
  mapRasterRefresh = refreshTiles;
  mapTilesNeeded = mapTilesReady = mapTilesPending = mapTilesFailed = 0;
  mapLastHttpError = mapLastTlsError = 0;
  if (!keepCanvas) mapCanvas->fillSprite(0x1E2226);
  int64_t firstX, lastX, firstY, lastY;
  double left, top;
  mapVisibleBounds(firstX, lastX, firstY, lastY, left, top);
  const int tileSize = mapSourceTileScreenSize();
  const int64_t count = 1LL << mapSourceZoom();
  for (int64_t rawY = firstY; rawY <= lastY; ++rawY) {
    if (rawY < 0 || rawY >= count) continue;
    for (int64_t rawX = firstX; rawX <= lastX; ++rawX) {
      const int px = static_cast<int>(lround(rawX * tileSize - left));
      const int py = static_cast<int>(lround(rawY * tileSize - top));
      if (!keepCanvas) mapCanvas->drawRect(px, py, tileSize, tileSize, 0x3A424A);
      ++mapTilesNeeded;
      if (mapRasterCount >= 32) { ++mapTilesFailed; continue; }
      const uint32_t x = static_cast<uint32_t>((rawX % count + count) % count);
      const uint16_t distance = static_cast<uint16_t>(std::min(65535,
          abs(px + tileSize / 2 - MAP_VIEW_W / 2) +
          abs(py + tileSize / 2 - MAP_VIEW_H / 2)));
      mapRasterJobs[mapRasterCount++] = {x, static_cast<uint32_t>(rawY), distance};
      ++mapTilesPending;
    }
  }
  std::sort(mapRasterJobs, mapRasterJobs + mapRasterCount,
      [](const MapRasterJob& a, const MapRasterJob& b) { return a.distance < b.distance; });
  mapRenderedView = {mapLatitude,mapLongitude,mapZoom};
  mapRenderedStyle = Tab5Map.selectedStyle();
  mapRenderedWidth = MAP_VIEW_W; mapRenderedHeight = MAP_VIEW_H;
  mapRenderedGeneration = mapGeneration;
  if (!keepCanvas && mapImage) lv_obj_invalidate(mapImage);
  Tab5DebugSerial.printf("[MAP][VIEW] %.5f, %.5f z%u : %u tuiles raster progressives\n",
      mapLatitude, mapLongitude, mapZoom, mapRasterCount);
  updateMapLabels();
}

static void pumpMapRasterJob() {
  if (activePage != Page::Map || Tab5MapPrint::busy() || Tab5Jpeg::busy() || !mapRasterJobs || mapRasterIndex >= mapRasterCount) return;
  if(millis()-lastMapViewChangeMs<350)return; // Coalesce rapid zoom/pan input before network work.
  const MapRasterJob job = mapRasterJobs[mapRasterIndex++];
  uint8_t* data = nullptr;
  size_t size = 0;
  const uint8_t zoom = mapSourceZoom();
  const bool cached = Tab5Map.readCachedTile(zoom, job.x, job.y, data, size);
  const bool drawn = cached && drawMapTile(zoom, job.x, job.y, data, size);
  heap_caps_free(data);
  if (drawn) ++mapTilesReady;
  bool connected = false;
  const bool allowed = mapDownloadsAllowed(&connected);
  const bool queued = (!drawn || mapRasterRefresh) && allowed && connected &&
      Tab5Map.requestTile(zoom, job.x, job.y, mapGeneration);
  if (!queued && mapTilesPending) --mapTilesPending;
  updateMapLabels();
}

static void markMapViewDirty() {
  lastMapViewChangeMs = millis();
}

static void panMapByPixels(int deltaX, int deltaY) {
  if (!deltaX && !deltaY) return;
  double worldX = mapWorldX(mapLongitude, mapZoom) - deltaX;
  double worldY = mapWorldY(mapLatitude, mapZoom) - deltaY;
  mapCoordinatesFromWorld(worldX, worldY, mapZoom,
                          mapLatitude, mapLongitude);
  markMapViewDirty();
  renderMapViewport(false);
}

static void mapTouchEvent(lv_event_t* event) {
  const lv_event_code_t code = lv_event_get_code(event);
  if(code==LV_EVENT_KEY) {
    uint32_t k=lv_event_get_key(event);
    if(k==LV_KEY_LEFT)panMapByPixels(80,0);
    else if(k==LV_KEY_RIGHT)panMapByPixels(-80,0);
    else if(k==LV_KEY_UP)panMapByPixels(0,80);
    else if(k==LV_KEY_DOWN)panMapByPixels(0,-80);
    else if(k=='+' || k=='-' || k=='=') {
      mapZoom=std::max<int>(MAP_MIN_ZOOM,std::min<int>(MAP_MAX_ZOOM,mapZoom+(k=='-'?-1:1)));
      syncMapZoomControl();markMapViewDirty();renderMapViewport(false);
    } else if(k=='i'||k=='I'){mapPointInfo(MAP_VIEW_W/2,MAP_VIEW_H/2);}
    else if(k==LV_KEY_HOME){mapLatitude=mapHomeLatitude;mapLongitude=mapHomeLongitude;mapZoom=mapHomeZoom;syncMapZoomControl();markMapViewDirty();renderMapViewport(false);}
    return;
  }
  lv_indev_t* indev = lv_indev_active();
  if (!indev) return;
  lv_point_t point;
  lv_indev_get_point(indev, &point);
  if (code == LV_EVENT_PRESSED) {
    mapDragStart = mapDragLast = point;
    mapDragging = true;
  } else if (code == LV_EVENT_PRESSING && mapDragging) {
    mapDragLast = point;
  } else if (code == LV_EVENT_LONG_PRESSED && mapDragging) {
    if (abs(point.x-mapDragStart.x)<5 && abs(point.y-mapDragStart.y)<5) {
      mapDragging = false;
      lv_area_t area; lv_obj_get_coords(mapImage, &area);
      mapPointInfo(point.x-area.x1,point.y-area.y1);
    }
  } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_CANCEL) &&
             mapDragging) {
    mapDragLast = point;
    mapDragging = false;
    const int dx = static_cast<int>(mapDragLast.x - mapDragStart.x);
    const int dy = static_cast<int>(mapDragLast.y - mapDragStart.y);
    if (code == LV_EVENT_CANCEL) return;
    if (abs(dx) >= 5 || abs(dy) >= 5) panMapByPixels(dx, dy);
    else {lv_area_t area;lv_obj_get_coords(mapImage,&area);Tab5MapMarker marker;
      if(Tab5MapOverlay::hit(point.x-area.x1,point.y-area.y1,marker))mapPointInfo(point.x-area.x1,point.y-area.y1);}

  }
}

static void mapHomeEvent(lv_event_t* event) {
  (void)event;
  mapLatitude = mapHomeLatitude;
  mapLongitude = mapHomeLongitude;
  mapZoom = mapHomeZoom;
  markMapViewDirty();
  renderMapViewport(false);
}

static lv_obj_t* createMapOverlay(lv_obj_t*& overlay, const char* title,
                                  int width, int height) {
  if (overlay && lv_obj_is_valid(overlay)) lv_obj_delete(overlay);
  overlay = lv_obj_create(root);
  lv_obj_set_size(overlay, SCREEN_W, SCREEN_H);
  lv_obj_set_pos(overlay, 0, 0);
  lv_obj_set_style_bg_color(overlay, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_60, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_set_style_pad_all(overlay, 0, 0);
  lv_obj_remove_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(overlay);

  lv_obj_t* box = lv_obj_create(overlay);
  lv_obj_set_size(box, width, height);
  lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_bg_color(box, C_SURFACE(), 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(box, C_BORDER(), 0);
  lv_obj_set_style_radius(box, 18, 0);
  lv_obj_set_style_pad_all(box, 18, 0);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* heading = lv_label_create(box);
  lv_label_set_text(heading, T(title));
  lv_obj_set_style_text_font(heading, richFontFor(24, 0), 0);
  lv_obj_set_style_text_color(heading, C_TEXT(), 0);
  lv_obj_set_pos(heading, 4, 2);
  return box;
}

#include "Tab5MapImportUi.inc"

static void closeMapPlacesEvent(lv_event_t* event) {
  (void)event;
  if (mapPlacesOverlay && lv_obj_is_valid(mapPlacesOverlay))
    lv_obj_delete(mapPlacesOverlay);
  mapPlacesOverlay = nullptr;
  mapPlacesList = nullptr;
  mapSelectedPlaceButton = nullptr;
  mapSelectedPlace = -1;
}

static void goToMapPlace(size_t index) {
  const Tab5MapPlace* place = Tab5Map.place(index);
  if (!place) return;
  mapLatitude = place->latitude;
  mapLongitude = place->longitude;
  mapZoom = place->zoom;
  closeMapPlacesEvent(nullptr);
  markMapViewDirty();
  renderMapViewport(false);
}

static void mapPlaceSelectEvent(lv_event_t* event) {
  const size_t index = reinterpret_cast<uintptr_t>(
      lv_event_get_user_data(event));
  if (index >= Tab5Map.placeCount()) return;
  if (mapSelectedPlaceButton && lv_obj_is_valid(mapSelectedPlaceButton))
    lv_obj_set_style_bg_color(mapSelectedPlaceButton, C_BG(), 0);
  mapSelectedPlace = static_cast<int>(index);
  mapSelectedPlaceButton = static_cast<lv_obj_t*>(lv_event_get_target(event));
  lv_obj_set_style_bg_color(mapSelectedPlaceButton, C_SELECTED(), 0);
}

static void mapPlaceActivateEvent(lv_event_t* event) {
  const size_t index = reinterpret_cast<uintptr_t>(
      lv_event_get_user_data(event));
  goToMapPlace(index);
}

static void refreshMapPlacesList() {
  if (!mapPlacesList || !lv_obj_is_valid(mapPlacesList)) return;
  lv_obj_clean(mapPlacesList);
  mapSelectedPlaceButton = nullptr;
  if (!Tab5Map.placeCount()) {
    lv_obj_t* empty = lv_label_create(mapPlacesList);
    lv_label_set_text(empty,
        T("Aucun lieu enregistré"));
    lv_obj_set_style_text_color(empty, C_MUTED(), 0);
    lv_obj_set_width(empty, 620);
    return;
  }
  for (size_t i = 0; i < Tab5Map.placeCount(); ++i) {
    const Tab5MapPlace* place = Tab5Map.place(i);
    if (!place) continue;
    char coordinates[64];
    snprintf(coordinates, sizeof(coordinates), "%.5f, %.5f  •  z%u",
             place->latitude, place->longitude,
             static_cast<unsigned>(place->zoom));
    const String placeLabel = String(place->name) + "\n" + coordinates;
    lv_obj_t* button = lv_list_add_button(
        mapPlacesList, LV_SYMBOL_GPS, placeLabel.c_str());
    stopAudioRowMarquee(button, 590);
    lv_obj_set_height(button, 76);
    lv_obj_set_style_bg_color(button, C_BG(), 0);
    lv_obj_set_style_bg_color(button, C_ACTIVE(), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, C_TEXT(), 0);
    lv_obj_set_style_text_font(button, uiFont(), 0);
    lv_obj_add_event_cb(button, mapPlaceSelectEvent, LV_EVENT_FOCUSED,
                        reinterpret_cast<void*>(i));
    Tab5Focus.activation(button,mapPlaceActivateEvent,reinterpret_cast<void*>(i));
    lv_obj_add_event_cb(button, mapPlaceSelectEvent, LV_EVENT_SINGLE_CLICKED,
        reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
    lv_obj_add_event_cb(button, mapPlaceActivateEvent, LV_EVENT_DOUBLE_CLICKED,
        reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
    if (static_cast<int>(i) == mapSelectedPlace) {
      mapSelectedPlaceButton = button;
      lv_obj_set_style_bg_color(button, C_SELECTED(), 0);
    }
  }
}

static void mapPlaceGoEvent(lv_event_t* event) {
  (void)event;
  if (mapSelectedPlace < 0) {
    toast("Sélectionnez un lieu");
    return;
  }
  goToMapPlace(static_cast<size_t>(mapSelectedPlace));
}

static void mapPlaceRenameEvent(lv_event_t* event) {
  (void)event;
  if (mapSelectedPlace < 0) {
    toast("Sélectionnez un lieu");
    return;
  }
  const size_t index = static_cast<size_t>(mapSelectedPlace);
  const Tab5MapPlace* place = Tab5Map.place(index);
  if (!place) return;
  openTextDialog(TextDialogAction::MapRenamePlace, "Renommer le lieu",
                 place->name, "");
  pendingMapPlaceIndex = index;
}

static void closeMapPlaceDeleteOverlay() {
  if (mapPlacesDeleteOverlay && lv_obj_is_valid(mapPlacesDeleteOverlay))
    lv_obj_delete(mapPlacesDeleteOverlay);
  mapPlacesDeleteOverlay = nullptr;
}

static void mapPlaceDeleteNoEvent(lv_event_t* event) {
  (void)event;
  closeMapPlaceDeleteOverlay();
}

static void mapPlaceDeleteYesEvent(lv_event_t* event) {
  (void)event;
  const int index = mapSelectedPlace;
  closeMapPlaceDeleteOverlay();
  if (index >= 0 && Tab5Map.removePlace(static_cast<size_t>(index))) {
    mapSelectedPlace = -1;
    refreshMapPlacesList();
    toast("Lieu supprimé");
  } else {
    toast("Suppression impossible");
  }
}

static void mapPlaceDeleteEvent(lv_event_t* event) {
  (void)event;
  if (mapSelectedPlace < 0) {
    toast("Sélectionnez un lieu");
    return;
  }
  lv_obj_t* box = createMapOverlay(mapPlacesDeleteOverlay,
                                   "Supprimer ce lieu ?", 620, 220);
  const Tab5MapPlace* place = Tab5Map.place(
      static_cast<size_t>(mapSelectedPlace));
  lv_obj_t* label = lv_label_create(box);
  lv_label_set_text(label, place ? place->name : T("Lieu sélectionné"));
  lv_obj_set_style_text_color(label, C_MUTED(), 0);
  lv_obj_align(label, LV_ALIGN_CENTER, 0, -10);
  lv_obj_t* no = makeButton(box, "Annuler", 190, 52);
  lv_obj_align(no, LV_ALIGN_BOTTOM_LEFT, 8, -2);
  lv_obj_add_event_cb(no, mapPlaceDeleteNoEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* yes = makeButton(box, "Supprimer", 190, 52);
  lv_obj_align(yes, LV_ALIGN_BOTTOM_RIGHT, -8, -2);
  lv_obj_add_event_cb(yes, mapPlaceDeleteYesEvent, LV_EVENT_CLICKED, nullptr);
}

static void mapSetHomeEvent(lv_event_t* event) {
  (void)event;
  mapHomeLatitude = mapLatitude;
  mapHomeLongitude = mapLongitude;
  mapHomeZoom = mapZoom;
  if (Tab5Map.saveHomeView(mapHomeLatitude, mapHomeLongitude, mapHomeZoom))
    toast("Position d’accueil enregistrée");
  else
    toast("Position d’accueil non enregistrée");
}

static void mapSavePlaceEvent(lv_event_t* event) {
  (void)event;
  String initial = "Lieu ";
  initial += String(Tab5Map.placeCount() + 1);
  openTextDialog(TextDialogAction::MapAddPlace, "Nom du lieu",
                 initial, "");
}

static void mapPlacesEvent(lv_event_t* event) {
  (void)event;
  lv_obj_t* box = createMapOverlay(mapPlacesOverlay,
                                   "Lieux enregistrés", 1000, 610);
  mapSelectedPlace = -1;
  mapPlacesList = lv_list_create(box);
  lv_obj_set_style_pad_right(mapPlacesList,28,0);
  lv_obj_set_size(mapPlacesList, 670, 510);
  lv_obj_set_pos(mapPlacesList, 0, 52);
  lv_obj_set_style_bg_color(mapPlacesList, C_BG(), 0);
  lv_obj_set_style_border_color(mapPlacesList, C_BORDER(), 0);

  lv_obj_t* go = makeButton(box, "Atteindre", 245, 54);
  lv_obj_set_pos(go, 700, 74);
  lv_obj_add_event_cb(go, mapPlaceGoEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* rename = makeButton(box, "Renommer", 245, 54);
  lv_obj_set_pos(rename, 700, 144);
  lv_obj_add_event_cb(rename, mapPlaceRenameEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* remove = makeButton(box, "Supprimer", 245, 54);
  lv_obj_set_pos(remove, 700, 214);
  lv_obj_add_event_cb(remove, mapPlaceDeleteEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* setHome = makeButton(box, "Définir l’accueil ici", 245, 116);
  lv_obj_set_pos(setHome, 700, 304);
  auto* setHomeLabel=lv_obj_get_child(setHome,0);
  lv_obj_set_width(setHomeLabel,213);
  lv_label_set_long_mode(setHomeLabel,LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(setHomeLabel,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_center(setHomeLabel);
  Tab5Focus.describe(setHome,T("Position actuelle = accueil"));
  lv_obj_add_event_cb(setHome, mapSetHomeEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* close = makeButton(box, "Fermer", 245, 54);
  lv_obj_set_pos(close, 700, 508);
  lv_obj_add_event_cb(close, closeMapPlacesEvent, LV_EVENT_CLICKED, nullptr);
  refreshMapPlacesList();
}

static void closeMapStylesEvent(lv_event_t* event) {
  (void)event;
  if (mapStylesOverlay && lv_obj_is_valid(mapStylesOverlay))
    lv_obj_delete(mapStylesOverlay);
  mapStylesOverlay = nullptr;
}

static void mapStyleSelectEvent(lv_event_t* event) {
  const size_t index = reinterpret_cast<uintptr_t>(
      lv_event_get_user_data(event));
  if (index >= Tab5Map.styleCount()) return;
  ++mapGeneration;
  if (!mapGeneration) ++mapGeneration;
  Tab5Map.setActive(false, mapGeneration);
    mapWheel.cancel();
    mapSearchPending=mapSearchResultsReady=false;++mapSearchRequestId;if(!mapSearchRequestId)++mapSearchRequestId;
  Tab5Map.clearMemoryCache();
  Tab5Map.selectStyle(index);
  Tab5Map.setActive(true, mapGeneration);
  closeMapStylesEvent(nullptr);
  clampMapView();
  updateMapAttribution();
  renderMapViewport(false);
}

static void mapRememberViewEvent(lv_event_t* event) {
  auto* sw = static_cast<lv_obj_t*>(lv_event_get_target(event));
  const bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
  if (Tab5Map.setRememberView(enabled)) return;
  if (Tab5Map.rememberView()) lv_obj_add_state(sw, LV_STATE_CHECKED);
  else lv_obj_remove_state(sw, LV_STATE_CHECKED);
  toast("Réglage non enregistré");
}

static void mapStylesEvent(lv_event_t* event) {
  (void)event;
  lv_obj_t* box = createMapOverlay(mapStylesOverlay,
                                   "Réglages de la carte", 820, 620);
  lv_obj_t* list = lv_list_create(box);
  lv_obj_set_style_pad_right(list,28,0);
  lv_obj_set_size(list, 770, 320);
  lv_obj_set_pos(list, 0, 52);
  lv_obj_set_style_bg_color(list, C_BG(), 0);
  lv_obj_set_style_border_color(list, C_BORDER(), 0);
  for (size_t i = 0; i < Tab5Map.styleCount(); ++i) {
    String label = Tab5Map.styleName(i);
    if (i == Tab5Map.selectedStyle()) label += T("  •  actif");
    else if (Tab5Map.styleIsLightweight(i)) label += T("  •  léger");
    lv_obj_t* button = lv_list_add_button(
        list, i == Tab5Map.selectedStyle() ? LV_SYMBOL_OK : LV_SYMBOL_IMAGE,
        label.c_str());
    stopAudioRowMarquee(button, 640);
    lv_obj_set_height(button, 74);
    lv_obj_set_style_bg_color(button,
        i == Tab5Map.selectedStyle() ? C_SELECTED() : C_BG(), 0);
    lv_obj_set_style_text_color(button, C_TEXT(), 0);
    lv_obj_set_style_text_font(button, uiFont(), 0);
    lv_obj_add_event_cb(button, mapStyleSelectEvent, LV_EVENT_CLICKED,
        reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
  }
  auto* rememberLabel = lv_label_create(box);
  lv_obj_set_pos(rememberLabel, 4, 390);
  lv_obj_set_width(rememberLabel, 650);
  lv_label_set_long_mode(rememberLabel, LV_LABEL_LONG_WRAP);
  lv_label_set_text(rememberLabel, T("Mémoriser la dernière vue"));
  lv_obj_set_style_text_font(rememberLabel, uiFont(), 0);
  lv_obj_set_style_text_color(rememberLabel, C_TEXT(), 0);
  auto* remember = lv_switch_create(box);
  styleSettingsSwitch(remember);
  lv_obj_set_pos(remember, 692, 389);
  if (Tab5Map.rememberView()) lv_obj_add_state(remember, LV_STATE_CHECKED);
  Tab5Focus.describe(remember, T("Mémoriser la dernière vue"));
  lv_obj_add_event_cb(remember, mapRememberViewEvent, LV_EVENT_VALUE_CHANGED, nullptr);
  auto* rememberHelp = lv_label_create(box);
  lv_obj_set_pos(rememberHelp, 4, 446);
  lv_obj_set_width(rememberHelp, 754);
  lv_label_set_long_mode(rememberHelp, LV_LABEL_LONG_WRAP);
  lv_label_set_text(rememberHelp,
      T("Position et zoom enregistrés en quittant Cartes. Désactivé : ouvrir l'accueil."));
  lv_obj_set_style_text_font(rememberHelp, uiFont(), 0);
  lv_obj_set_style_text_color(rememberHelp, C_MUTED(), 0);
  lv_obj_t* close = makeButton(box, "Fermer", 200, 52);
  lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(close, closeMapStylesEvent, LV_EVENT_CLICKED, nullptr);
}

static void closeMapSearchEvent(lv_event_t* event) {
  (void)event;
  if (mapSearchOverlay && lv_obj_is_valid(mapSearchOverlay))
    lv_obj_delete(mapSearchOverlay);
  mapSearchOverlay = nullptr;
}

static void mapPointInfo(int x, int y) {
  x = constrain(x, 0, MAP_VIEW_W-1); y = constrain(y, 0, MAP_VIEW_H-1);
  mapCoordinatesFromWorld(mapWorldX(mapLongitude,mapZoom)-MAP_VIEW_W*0.5+x,
      mapWorldY(mapLatitude,mapZoom)-MAP_VIEW_H*0.5+y,mapZoom,mapPointLatitude,mapPointLongitude);
  Tab5MapMarker selected;const bool haveMarker=Tab5MapOverlay::hit(x,y,selected);
  if(haveMarker){mapPointLatitude=selected.latitude;mapPointLongitude=selected.longitude;}
  lv_obj_t* box = createMapOverlay(mapStylesOverlay,"Informations sur ce point",840,490);
  lv_obj_t* text = lv_label_create(box);
  lv_obj_set_size(text, 790, 310); lv_obj_set_pos(text, 4, 54);
  lv_obj_set_style_text_font(text, uiFont(), 0); lv_obj_set_style_text_color(text, C_TEXT(), 0);
  const uint8_t sourceZoom = mapSourceZoom();
  const double meters = 156543.033928*cos(mapPointLatitude*MAP_PI/180.0)/(1UL<<mapZoom);
  char coordinates[120];
  snprintf(coordinates,sizeof(coordinates),"%.6f, %.6f\nz%u · %s z%u · %.3f m/px",
      mapPointLatitude,mapPointLongitude,unsigned(mapZoom),T("Source"),unsigned(sourceZoom),meters);
  String details = String(coordinates)+"\n"+Tab5Map.styleName(Tab5Map.selectedStyle());
  if (sourceZoom < mapZoom) details += String("\n") + T("Agrandissement visuel : les détails de la source sont conservés.");
  if(haveMarker){
    details += String("\n") + T(selected.kind==Tab5MapMarkerKind::Mesh?"Meshtastic":selected.kind==Tab5MapMarkerKind::Saved?"Lieu enregistré":"Résultat de recherche")+" : "+selected.name;
    if(selected.node){char node[40];snprintf(node,sizeof(node),"\n!%08lx",static_cast<unsigned long>(selected.node));details+=node;}
    if(selected.timestamp){time_t epoch=selected.timestamp;struct tm utc{};gmtime_r(&epoch,&utc);char stamp[40];strftime(stamp,sizeof(stamp),"\n%Y-%m-%d %H:%M UTC",&utc);details+=stamp;}
  }
  details += String("\n")+T("Paquets hors ligne")+" : "+String(Tab5Map.offlinePackCountForSelectedStyle());
  lv_label_set_text(text,details.c_str());
  lv_obj_t* center = makeButton(box,"Centrer ici",200,52);
  lv_obj_align(center,LV_ALIGN_BOTTOM_LEFT,0,0);
  lv_obj_add_event_cb(center,[](lv_event_t*){
    mapLatitude=mapPointLatitude;mapLongitude=mapPointLongitude;
    closeMapStylesEvent(nullptr);markMapViewDirty();renderMapViewport(false);
  },LV_EVENT_CLICKED,nullptr);
  lv_obj_t* save = makeIconButton(box,LV_SYMBOL_SAVE,60,52);
  lv_obj_align(save,LV_ALIGN_BOTTOM_MID,0,0);Tab5Focus.describe(save,T("Enregistrer ce lieu"));
  lv_obj_add_event_cb(save,[](lv_event_t*){
    mapLatitude=mapPointLatitude;mapLongitude=mapPointLongitude;
    closeMapStylesEvent(nullptr);markMapViewDirty();renderMapViewport(false);mapSavePlaceEvent(nullptr);
  },LV_EVENT_CLICKED,nullptr);
  lv_obj_t* close = makeButton(box,"Fermer",200,52);
  lv_obj_align(close,LV_ALIGN_BOTTOM_RIGHT,0,0);
  lv_obj_add_event_cb(close,closeMapStylesEvent,LV_EVENT_CLICKED,nullptr);
  Tab5Focus.focusInitial(mapStylesOverlay,center);
}

static void mapInfoEvent(lv_event_t*) {
  mapDragging = false;
  mapPointInfo(MAP_VIEW_W/2, MAP_VIEW_H/2);
}

static void mapSearchResultEvent(lv_event_t* event) {
  const size_t index = reinterpret_cast<uintptr_t>(
      lv_event_get_user_data(event));
  if (!mapSearchItems || index >= mapSearchCount) return;
  mapLatitude = mapSearchItems[index].latitude;
  mapLongitude = mapSearchItems[index].longitude;
  mapZoom = mapSearchItems[index].zoom;
  Tab5Map.selectSearchMarker(mapSearchItems[index]);
  closeMapSearchEvent(nullptr);
  markMapViewDirty();
  renderMapViewport(false);
}

static void showMapSearchResults() {
  if (!mapSearchItems) {
    toast("Recherche indisponible : PSRAM insuffisante", 3000);
    return;
  }
  lv_obj_t* box = createMapOverlay(mapSearchOverlay,
      mapSearchCount ? "Résultats de recherche" : "Aucun résultat",
      1060, 620);
  lv_obj_t* list = lv_list_create(box);
  lv_obj_set_style_pad_right(list,28,0);
  lv_obj_set_size(list, 1010, 395);
  lv_obj_set_pos(list, 0, 52);
  lv_obj_set_style_bg_color(list, C_BG(), 0);
  lv_obj_set_style_border_color(list, C_BORDER(), 0);
  if (!mapSearchCount) {
    lv_obj_t* empty = lv_label_create(list);
    lv_label_set_text(empty, T("Aucun lieu ne correspond à cette recherche."));
    lv_obj_set_style_text_color(empty, C_MUTED(), 0);
  }
  for (size_t i = 0; i < mapSearchCount; ++i) {
    const Tab5MapSearchItem& item = mapSearchItems[i];
    String label = item.name;
    if (item.alternative[0] && String(item.alternative) != label)
      label += String("  ·  ") + item.alternative;
    if (item.displayName[0] && String(item.displayName) != label)
      label += String("\n") + item.displayName;
    lv_obj_t* button = lv_list_add_button(
        list, LV_SYMBOL_GPS, label.c_str());
    stopAudioRowMarquee(button, 800);
    lv_obj_set_height(button, 86);
    lv_obj_set_style_bg_color(button, C_BG(), 0);
    lv_obj_set_style_bg_color(button, C_ACTIVE(), LV_STATE_PRESSED);
    lv_obj_set_style_text_color(button, C_TEXT(), 0);
    lv_obj_set_style_text_font(button, uiFont(), 0);
    lv_obj_add_event_cb(button, mapSearchResultEvent, LV_EVENT_CLICKED,
        reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
  }
  lv_obj_t* attribution = lv_label_create(box);
  String source=mapSearchLocal?String("GeoNames · CC BY 4.0"):String(T("Recherche © OpenStreetMap contributors"));
  if(mapSearchLocal&&!mapGazetteerAvailable)source=String(T("Index local indisponible · lieux enregistrés et Mesh uniquement"))+"\n/System/maps/gazetteer/cities.t5geo";
  else if(mapSearchLocal&&mapSearchTruncated)source+=String(" · ")+T("Précisez la recherche pour affiner les résultats");
  lv_label_set_text(attribution,source.c_str());
  lv_obj_set_style_text_color(attribution, C_MUTED(), 0);
  lv_obj_set_width(attribution,1000);lv_obj_set_pos(attribution,4,456);
  if(mapSearchLocal&&mapNetworkConnected()){
    auto* online=makeButton(box,"Recherche en ligne",320,52);lv_obj_align(online,LV_ALIGN_BOTTOM_LEFT,0,0);
    lv_obj_add_event_cb(online,[](lv_event_t*){
      ++mapSearchRequestId;if(!mapSearchRequestId)++mapSearchRequestId;
      mapSearchPending=Tab5Map.requestSearch(mapSearchQuery,mapSearchRequestId,mapLatitude,mapLongitude,true);
      if(mapSearchPending){closeMapSearchEvent(nullptr);updateMapLabels();}else toast("Recherche impossible");
    },LV_EVENT_CLICKED,nullptr);
  }
  lv_obj_t* close = makeButton(box, "Fermer", 200, 52);
  lv_obj_align(close, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_add_event_cb(close, closeMapSearchEvent, LV_EVENT_CLICKED, nullptr);
}

static void mapSearchEvent(lv_event_t* event) {
  (void)event;
  mapSearchPending=mapSearchResultsReady=false;++mapSearchRequestId;if(!mapSearchRequestId)++mapSearchRequestId;
  closeMapSearchEvent(nullptr);
  openTextDialog(TextDialogAction::MapSearch, "Rechercher un lieu",
                 mapSearchQuery, "");
  // SearchRequest is a bounded UTF-8 byte buffer; rejecting excess input here
  // also prevents silently querying a different, truncated place name.
  if (dialogInput) lv_textarea_set_max_length(dialogInput, 96);
}

static void mapRefreshEvent(lv_event_t* event) {
  (void)event;
  const uint32_t now = millis();
  if (lastMapRefreshMs && now - lastMapRefreshMs < 2000) return;
  bool connected = false;
  if (!mapDownloadsAllowed(&connected) || !connected) {
    toast("Wi-Fi disponible requis pour actualiser les tuiles", 2600);
    return;
  }
  lastMapRefreshMs = now;
  renderMapViewport(true);
}

static void serviceMapPage() {
  if(activePage==Page::Map)Tab5MapOverlay::refresh();
  // Async search results wait until the map itself owns keyboard focus.
  // They must not cover a new dialog while its keyboard remains active.
  if(mapSearchResultsReady&&activePage==Page::Map&&keyboardScope()==mapPage){mapSearchResultsReady=false;showMapSearchResults();}
  if (mapOpenPending) {
    mapWheel.cancel();
    mapOpenPending = false;
    if (activePage != Page::Map || !sdReady || !Tab5Map.ready()) return;
    Tab5DebugSerial.println("[MAP][OPEN] première vue hors callback LVGL");
    renderMapViewport(false);
    // Empêche le changement Wi-Fi initial de lancer immédiatement une seconde
    // génération de la première vue.
    mapLastDownloadsAllowed = mapDownloadsAllowed(&mapLastWifiConnected);
    Tab5DebugSerial.println("[MAP][OPEN] première vue planifiée : OK");
    return;
  }

  Tab5MapWheel::View nextWheelView;
  const bool wheelEnabled=activePage==Page::Map&&keyboardScope()==mapPage&&!keyboardVisible&&!mapDragging&&!Tab5MapPrint::busy()&&!Tab5Jpeg::busy()&&!mapJpegPreparing&&!mapJpegEncoding;
  if(mapWheel.take(millis(),{mapLatitude,mapLongitude,mapZoom},MAP_VIEW_W,MAP_VIEW_H,Tab5Map.styleMinZoom(Tab5Map.selectedStyle()),
      std::min<unsigned>(MAP_MAX_ZOOM,Tab5Map.styleViewMaxZoom(Tab5Map.selectedStyle())),wheelEnabled,nextWheelView)){
    mapLatitude=nextWheelView.latitude;mapLongitude=nextWheelView.longitude;mapZoom=nextWheelView.zoom;
    markMapViewDirty();renderMapViewport(false);
  }

  // La tuile a déjà été dessinée à la boucle précédente. Le rafraîchissement
  // LVGL a donc lieu avant cette écriture microSD, ce qui rend l'affichage
  // nettement plus réactif sans autoriser une seconde tâche à toucher à SD.
  bool sdWorkedThisLoop = false;
  const bool storageBusy=Tab5FileOperation::busy()||Tab5Jpeg::busy()||Tab5Print::busy()||Tab5Vault::busy()||Tab5Firmware::busy()||Tab5Python::running()||tab5ExecutableBusy();
  if (mapPendingStoreValid && !storageBusy &&
      (!mapStoreAttempts || uint32_t(millis()-mapStoreLastAttempt)>=1000)) {
    mapStoreLastAttempt=millis();++mapStoreAttempts;
    const bool stored=sdReady&&Tab5Map.storeTile(mapPendingStore);
    if(stored||mapStoreAttempts>=3){
      if(!stored)Tab5DebugSerial.printf("[MAP] cache abandonne apres %u essais z%u/%u/%u\n",mapStoreAttempts,
        unsigned(mapPendingStore.zoom),unsigned(mapPendingStore.x),unsigned(mapPendingStore.y));
      Tab5Map.release(mapPendingStore);mapPendingStoreValid=false;
      if (activePage != Page::Map) Tab5Map.clearMemoryCache();
    }
    sdWorkedThisLoop = true;
  }

  if (!sdWorkedThisLoop && !mapDownloadOverlay) pumpMapRasterJob();
  Tab5MapTileResult result;
  bool changed = false;
  int handled = 0;
  while (!mapPendingStoreValid && (!mapImportRunning || !storageBusy) && handled < 1 && Tab5Map.poll(result)) {
    ++handled;
    if(consumeMapImportResult(result)){Tab5Map.release(result);continue;}
    Tab5MapPrint::tileResult(result);
    if (!result.success) {
      mapLastHttpError = result.httpCode;
      mapLastTlsError = result.tlsCode;
      Tab5DebugSerial.printf("[MAP] échec z%u/%u/%u, HTTP=%d\n",
                    static_cast<unsigned>(result.zoom),
                    static_cast<unsigned>(result.x),
                    static_cast<unsigned>(result.y), result.httpCode);
    }
    if (activePage == Page::Map &&
        result.generation == mapGeneration && result.zoom == mapSourceZoom() && !Tab5MapPrint::busy()) {
      if (mapTilesPending) --mapTilesPending;
      if (result.success && drawMapTile(result.zoom, result.x, result.y,
                                        result.data, result.size)) {
        if (mapTilesReady < mapTilesNeeded) ++mapTilesReady;
      } else if (result.httpCode != -107) {
        ++mapTilesFailed;
      }
      changed = true;
    }
    if (result.success) {
      mapPendingStore = result;
      mapPendingStoreValid = true;mapStoreAttempts=0;
      result.data = nullptr;
      result.size = 0;
    }
    Tab5Map.release(result);
  }
  serviceMapImport(storageBusy || mapPendingStoreValid);
  if (changed) {
    updateMapLabels();
  }

  Tab5MapSearchResult searchResult;
  while (Tab5Map.pollSearch(searchResult)) {
    if (searchResult.requestId == mapSearchRequestId) {
      mapSearchPending = false;
      mapSearchLocal=searchResult.local;mapGazetteerAvailable=searchResult.gazetteerAvailable;
      mapSearchLocalError=searchResult.localError;mapSearchTruncated=searchResult.truncated;
      mapSearchCount = min(searchResult.count,
                           static_cast<uint8_t>(TAB5_MAP_MAX_SEARCH_RESULTS));
      if (mapSearchItems)
        memset(mapSearchItems, 0,
               TAB5_MAP_MAX_SEARCH_RESULTS * sizeof(Tab5MapSearchItem));
      else
        mapSearchCount = 0;
      if (mapSearchItems && searchResult.items && mapSearchCount)
        memcpy(mapSearchItems, searchResult.items,
               mapSearchCount * sizeof(Tab5MapSearchItem));
      if (searchResult.success) {
        mapSearchResultsReady=activePage==Page::Map;
      } else {
        Tab5DebugSerial.printf("[MAP] recherche échouée, HTTP=%d\n",
                      searchResult.httpCode);
        if (activePage == Page::Map)
          toast(searchResult.local?"Recherche locale indisponible":"Le service de recherche ne répond pas", 2800);
      }
      updateMapLabels();
    }
    Tab5Map.releaseSearch(searchResult);
  }

  bool connected = false;
  const bool allowed = mapDownloadsAllowed(&connected);
  const bool setupWasPending = mapNetworkSetupPending;
  mapNetworkSetupPending = tab5NetworkMediaPending();
  if (activePage == Page::Map && !mapDownloadOverlay &&
      ((connected && !mapLastWifiConnected) ||
       (allowed && !mapLastDownloadsAllowed) || (setupWasPending && !mapNetworkSetupPending))) {
    renderMapViewport(false, true);
  }
  mapLastWifiConnected = connected;
  mapLastDownloadsAllowed = allowed;


}

static String lastMapExport, mapJpegDestination;
static unsigned mapJpegFallbacks=0;
static lv_obj_t *mapExportProgress=nullptr,*mapExportProgressText=nullptr,*mapExportProgressBar=nullptr,*mapExportProgressPercent=nullptr;
static void closeMapExportProgress(){
  if(mapExportProgress&&lv_obj_is_valid(mapExportProgress))lv_obj_delete(mapExportProgress);
  mapExportProgress=mapExportProgressText=mapExportProgressBar=mapExportProgressPercent=nullptr;
}
static void showMapExportProgress(){
  closeMapExportProgress();mapExportProgress=lv_obj_create(lv_layer_top());
  lv_obj_set_size(mapExportProgress,580,96);lv_obj_align(mapExportProgress,LV_ALIGN_BOTTOM_MID,0,-12);
  lv_obj_set_style_bg_color(mapExportProgress,C_SURFACE(),0);lv_obj_set_style_bg_opa(mapExportProgress,LV_OPA_COVER,0);
  lv_obj_set_style_border_color(mapExportProgress,C_BORDER(),0);lv_obj_set_style_border_width(mapExportProgress,1,0);
  lv_obj_set_style_pad_all(mapExportProgress,12,0);lv_obj_remove_flag(mapExportProgress,LV_OBJ_FLAG_SCROLLABLE);
  mapExportProgressText=lv_label_create(mapExportProgress);lv_obj_set_size(mapExportProgressText,468,30);
  lv_label_set_long_mode(mapExportProgressText,LV_LABEL_LONG_DOT);lv_label_set_text(mapExportProgressText,T("Capture de la carte affichée…"));
  lv_obj_set_style_text_font(mapExportProgressText,uiFont(),0);lv_obj_set_style_text_color(mapExportProgressText,C_TEXT(),0);
  mapExportProgressBar=lv_bar_create(mapExportProgress);lv_obj_set_size(mapExportProgressBar,400,12);lv_obj_set_pos(mapExportProgressBar,0,50);
  lv_obj_set_style_bg_color(mapExportProgressBar,C_BORDER(),LV_PART_MAIN);lv_obj_set_style_bg_color(mapExportProgressBar,C_ACTIVE(),LV_PART_INDICATOR);
  mapExportProgressPercent=lv_label_create(mapExportProgress);lv_obj_set_size(mapExportProgressPercent,64,30);lv_obj_set_pos(mapExportProgressPercent,410,38);
  lv_obj_set_style_text_font(mapExportProgressPercent,uiFont(),0);lv_obj_set_style_text_color(mapExportProgressPercent,C_TEXT(),0);lv_label_set_text(mapExportProgressPercent,"0 %");
  auto* cancel=makeIconButton(mapExportProgress,LV_SYMBOL_CLOSE,52,52);lv_obj_align(cancel,LV_ALIGN_RIGHT_MID,0,0);
  lv_obj_add_event_cb(cancel,[](lv_event_t*){if(mapJpegPreparing)Tab5MapPrint::cancel();if(mapJpegEncoding)Tab5Jpeg::cancel();},LV_EVENT_CLICKED,nullptr);
}
static void mapExportEvent(lv_event_t*) {
  if (!sdReady || Tab5MapPrint::busy() || Tab5Jpeg::busy() || mapJpegPreparing || mapJpegEncoding) {
    toast("Export de carte déjà en cours"); return;
  }
  if(!mapCanvas||!mapCanvas->getBuffer()){toast("Carte indisponible");return;}
  if ((!tab5FS.exists("/Picture") && !tab5FS.mkdir("/Picture")) ||
      (!tab5FS.exists("/Picture/Maps") && !tab5FS.mkdir("/Picture/Maps"))) {
    tab5StorageFault("dossier JPEG","/Picture/Maps");toast(tab5StorageErrorText()); return;
  }
  mapJpegDestination="";
  for(unsigned n=1;n<10000;++n){char path[64];snprintf(path,sizeof(path),"/Picture/Maps/map-%04u.jpg",n);
    if(!tab5FS.exists(path) && !tab5FS.exists(String(path)+".tmp")){mapJpegDestination=path;break;}}
  if(!mapJpegDestination.length()){toast("Aucun nom de fichier disponible");return;}
  ++mapGeneration;Tab5Map.beginGeneration(mapGeneration);mapRasterIndex=0;
  mapJpegPreparing=Tab5MapPrint::beginCapture(static_cast<const uint16_t*>(mapCanvas->getBuffer()),MAP_VIEW_W,MAP_VIEW_H,mapZoom,mapGeneration,Tab5Map.attribution());
  if(mapJpegPreparing)showMapExportProgress();else toast(Tab5MapPrint::status(),3500);
}
static void serviceMapJpeg() {
  if(mapJpegPreparing&&!Tab5MapPrint::busy()){
    mapJpegPreparing=false;String raw=Tab5MapPrint::takePath();
    if(!raw.length()){closeMapExportProgress();toast(Tab5MapPrint::status(),3500);return;}
    mapJpegFallbacks=Tab5MapPrint::fallbackTileCount();
    mapJpegEncoding=Tab5Jpeg::start(raw,mapJpegDestination);
    if(!mapJpegEncoding){tab5FS.remove(raw);toast(Tab5Jpeg::error().c_str(),3500);}
  }
  if(mapJpegEncoding&&!Tab5Jpeg::busy()){
    mapJpegEncoding=false;String result=Tab5Jpeg::result();
    if(result.length()){lastMapExport=result;String message=String(T("JPEG enregistré : "))+result;if(mapJpegFallbacks)message+="\n"+String(T("Certaines tuiles sont moins détaillées."));toast(message.c_str(),6000);}
    else toast(Tab5Jpeg::error().c_str(),4500);
  }
  if(!mapJpegPreparing&&!mapJpegEncoding){closeMapExportProgress();return;}
  if(mapExportProgress&&lv_obj_is_valid(mapExportProgress)){
    const unsigned percent=mapJpegPreparing?Tab5MapPrint::progress()/2:50+Tab5Jpeg::progress()/2;
    setLabelIfChanged(mapExportProgressText,T(mapJpegPreparing?"Capture de la carte affichée…":"Encodage JPEG…"));
    String value=String(percent)+" %";setLabelIfChanged(mapExportProgressPercent,value.c_str());lv_bar_set_value(mapExportProgressBar,percent,LV_ANIM_OFF);
  }
}

static void createMapPage() {
  mapPage = lv_obj_create(pageHost);
  lv_obj_set_size(mapPage, WORK_W, PAGE_H);Tab5Shell::width(mapPage,0);
  lv_obj_set_pos(mapPage, 0, 0);
  lv_obj_set_style_border_width(mapPage, 0, 0);
  lv_obj_set_style_radius(mapPage, 0, 0);
  lv_obj_set_style_pad_all(mapPage, 0, 0);
  lv_obj_set_style_bg_color(mapPage, C_BG(), 0);Tab5ThemeLive::bind(mapPage,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(mapPage, LV_OPA_COVER, 0);
  lv_obj_remove_flag(mapPage, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* toolbar = lv_obj_create(mapPage);
  lv_obj_set_size(toolbar, MAP_VIEW_W, 56);Tab5Shell::width(toolbar,-16);
  lv_obj_set_pos(toolbar, 8, 8);
  lv_obj_set_style_radius(toolbar, 12, 0);
  lv_obj_set_style_bg_color(toolbar, C_SURFACE(), 0);
  lv_obj_set_style_bg_opa(toolbar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(toolbar, C_BORDER(), 0);
  lv_obj_set_style_pad_all(toolbar, 0, 0);
  lv_obj_remove_flag(toolbar, LV_OBJ_FLAG_SCROLLABLE);

  mapZoomSlider = lv_slider_create(toolbar);
  lv_obj_set_size(mapZoomSlider, 170, 24);
  lv_obj_set_pos(mapZoomSlider, 12, 16);
  styleSettingsSlider(mapZoomSlider);
  lv_slider_set_range(mapZoomSlider, MAP_MIN_ZOOM, MAP_MAX_ZOOM);
  lv_slider_set_value(mapZoomSlider, mapZoom, LV_ANIM_OFF);
  lv_obj_add_event_cb(mapZoomSlider, mapZoomSliderEvent, LV_EVENT_ALL,
                      nullptr);
  mapZoomValueLabel = lv_label_create(toolbar);
  lv_obj_set_size(mapZoomValueLabel, 42, 26);
  lv_obj_set_pos(mapZoomValueLabel, 188, 15);
  lv_obj_set_style_text_font(mapZoomValueLabel, uiFont(), 0);
  lv_obj_set_style_text_color(mapZoomValueLabel, C_TEXT(), 0);
  lv_obj_set_style_text_align(mapZoomValueLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_t* home = makeIconButton(toolbar, LV_SYMBOL_HOME, 50, 44);
  lv_obj_set_pos(home, 236, 6);
  lv_obj_add_event_cb(home, mapHomeEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* savePlace = makeIconButton(toolbar, LV_SYMBOL_SAVE, 50, 44);
  lv_obj_set_pos(savePlace, 292, 6);
  lv_obj_add_event_cb(savePlace, mapSavePlaceEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* places = makeIconButton(toolbar, LV_SYMBOL_LIST, 50, 44);
  lv_obj_set_pos(places, 348, 6);
  lv_obj_add_event_cb(places, mapPlacesEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* search = makeSearchButton(toolbar,50,44);
  Tab5Focus.describe(search,T("Rechercher un lieu"));
  lv_obj_set_pos(search, 404, 6);
  lv_obj_add_event_cb(search, mapSearchEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* styles = makeIconButton(toolbar, LV_SYMBOL_IMAGE, 50, 44);
  lv_obj_set_pos(styles, 460, 6);
  lv_obj_add_event_cb(styles, mapStylesEvent, LV_EVENT_CLICKED, nullptr);
  Tab5Focus.describe(styles, T("Réglages de la carte"));
  lv_obj_t* refresh = makeIconButton(toolbar, LV_SYMBOL_REFRESH, 50, 44);
  lv_obj_set_pos(refresh, 516, 6);
  lv_obj_add_event_cb(refresh, mapRefreshEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* download = makeIconButton(toolbar, LV_SYMBOL_DOWNLOAD, 50, 44);
  lv_obj_set_pos(download, 572, 6);
  lv_obj_add_event_cb(download, mapDownloadEvent, LV_EVENT_CLICKED, nullptr);
  Tab5Focus.describe(download,T("Télécharger la zone"));

  lv_obj_t* jpeg = makeButton(toolbar, "JPG", 50, 44);
  lv_obj_set_pos(jpeg, 628, 6);
  lv_obj_set_style_pad_all(jpeg, 2, 0);
  Tab5Focus.describe(jpeg, T("Exporter la carte en JPEG"));
  lv_obj_add_event_cb(jpeg, mapExportEvent, LV_EVENT_CLICKED, nullptr);

  lv_obj_t* printMap = makeIconButton(toolbar, TAB5_SYMBOL_PRINT, 50, 44);
  lv_obj_set_pos(printMap, 684, 6);
  Tab5Focus.describe(printMap, T("Imprimer la carte affichée"));
  lv_obj_add_event_cb(printMap, [](lv_event_t*) {
    Tab5Tools.openMapPrint();
  }, LV_EVENT_CLICKED, nullptr);
  mapInfoButton = makeIconButton(toolbar, "", 50, 44);
  lv_obj_set_pos(mapInfoButton, 740, 6);
  lv_obj_t* infoGlyph = lv_obj_get_child(mapInfoButton, 0);
  lv_label_set_text(infoGlyph,"i"); lv_obj_set_size(infoGlyph,28,28);
  lv_obj_set_style_text_align(infoGlyph,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_set_style_border_width(infoGlyph,1,0);
  lv_obj_set_style_border_color(infoGlyph,C_TEXT(),0);
  lv_obj_set_style_radius(infoGlyph,LV_RADIUS_CIRCLE,0);
  Tab5Focus.describe(mapInfoButton, T("Informations au centre · Appui long : choisir un point"));
  lv_obj_add_event_cb(mapInfoButton, mapInfoEvent, LV_EVENT_CLICKED, nullptr);
  mapInfoLabel = lv_label_create(toolbar);
  lv_obj_set_size(mapInfoLabel, 270, 34);
  lv_obj_set_pos(mapInfoLabel, 636, 11);
  lv_label_set_long_mode(mapInfoLabel, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(mapInfoLabel, uiFont(), 0);
  lv_obj_set_style_text_color(mapInfoLabel, C_TEXT(), 0);
  mapStatusLabel = lv_label_create(toolbar);
  lv_obj_set_size(mapStatusLabel, MAP_VIEW_W - 804, 30);
  lv_obj_set_pos(mapStatusLabel, 800, 14);
  lv_label_set_long_mode(mapStatusLabel, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(mapStatusLabel, uiFont(), 0);
  lv_obj_set_style_text_align(mapStatusLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_color(mapStatusLabel, C_MUTED(), 0);
  syncMapZoomControl();

  lv_obj_t* viewport = lv_obj_create(mapPage);
  lv_obj_set_size(viewport, MAP_VIEW_W, MAP_VIEW_H);Tab5Shell::width(viewport,-16);
  lv_obj_set_pos(viewport, 8, 72);
  lv_obj_set_style_radius(viewport, 12, 0);
  lv_obj_set_style_border_color(viewport, C_BORDER(), 0);
  lv_obj_set_style_pad_all(viewport, 0, 0);
  lv_obj_set_style_bg_color(viewport, lv_color_hex(0x1E2226), 0);
  lv_obj_set_style_bg_opa(viewport, LV_OPA_COVER, 0);
  lv_obj_remove_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);

  mapImage = lv_image_create(viewport);
  lv_obj_set_pos(mapImage, 0, 0);
  lv_obj_set_size(mapImage, MAP_VIEW_W, MAP_VIEW_H);Tab5Shell::width(mapImage,-16);
  lv_obj_add_flag(mapImage,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_USER_1|LV_OBJ_FLAG_USER_2));
  Tab5Focus.describe(mapImage,T("Carte · Flèches : déplacer · +/− : zoom · I : informations · Début : domicile · Échap : commandes"));
  lv_obj_add_event_cb(mapImage, mapTouchEvent, LV_EVENT_ALL, nullptr);
  Tab5MapOverlay::attach(mapImage);

  // Les coordonnées restent intégralement visibles dans une pastille dédiée,
  // au lieu de réduire la place réservée à l'état de chargement du bandeau.
  lv_obj_set_parent(mapInfoLabel, viewport);
  lv_obj_set_size(mapInfoLabel, 300, 34);
  lv_obj_set_style_text_color(mapInfoLabel, lv_color_white(), 0);
  lv_obj_set_style_bg_color(mapInfoLabel, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(mapInfoLabel, LV_OPA_70, 0);
  lv_obj_set_style_pad_left(mapInfoLabel, 8, 0);
  lv_obj_set_style_pad_right(mapInfoLabel, 8, 0);
  lv_obj_set_style_pad_top(mapInfoLabel, 4, 0);
  lv_obj_set_style_pad_bottom(mapInfoLabel, 4, 0);
  lv_obj_align(mapInfoLabel, LV_ALIGN_BOTTOM_LEFT, 8, -8);
  lv_obj_remove_flag(mapInfoLabel, LV_OBJ_FLAG_CLICKABLE);

  mapAttributionLabel = lv_label_create(viewport);
  updateMapAttribution();
  lv_obj_set_style_text_font(mapAttributionLabel, uiFont(), 0);
  lv_obj_set_style_text_color(mapAttributionLabel, lv_color_white(), 0);
  lv_obj_set_style_bg_color(mapAttributionLabel, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(mapAttributionLabel, LV_OPA_70, 0);
  lv_obj_set_style_pad_left(mapAttributionLabel, 8, 0);
  lv_obj_set_style_pad_right(mapAttributionLabel, 8, 0);
  lv_obj_set_style_pad_top(mapAttributionLabel, 4, 0);
  lv_obj_set_style_pad_bottom(mapAttributionLabel, 4, 0);
  lv_obj_align(mapAttributionLabel, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
  lv_obj_remove_flag(mapAttributionLabel, LV_OBJ_FLAG_CLICKABLE);

  updateMapLabels();
}

static void closeSystemInfo() { Tab5SystemInfoView::close(); }

static void showSystemInfo() {
  if(Tab5SystemInfoView::scope())return;
  setKeyboardVisible(false);Tab5Focus.remember(keyboardScope());Tab5Focus.clear();
  Tab5SystemInfoView::Hooks hooks;
  hooks.translate=T;hooks.font=[](){return uiFont();};
  hooks.refresh=[](){systemInfoRefreshRequested=true;systemInfoDetailsRequested=true;refreshSystemInfo();};
  hooks.retestSd=[](){tab5StorageRequestRecovery();toast("Test SD demandé — attendre la fin des tâches et arrêter les médias",5000);};
  hooks.closed=[](){systemInfoOverlay=systemInfoBody=nullptr;systemInfoRefreshRequested=systemInfoDetailsRequested=false;Tab5Focus.restore(keyboardScope());};
  Tab5SystemInfoView::open(hooks);systemInfoOverlay=Tab5SystemInfoView::scope();systemInfoBody=Tab5SystemInfoView::body();
}

static void updateSystemOverview() {
  Tab5SystemInfoView::Snapshot s;
  const auto memory=Tab5MemoryLayout::sample();
  auto& internal=s.meters[Tab5SystemInfoView::Internal];internal.total=memory.hp.total;internal.used=memory.hp.used();
  snprintf(internal.note,sizeof(internal.note),T("Libres : %u Kio · hors cache et réservations"),unsigned(memory.hp.free/1024));
  auto& external=s.meters[Tab5SystemInfoView::External];external.total=memory.psram.total;external.used=memory.psram.used();
  snprintf(external.note,sizeof(external.note),T("Libres : %.1f Mio · mémoire externe"),double(memory.psram.free)/(1024*1024));
  const auto& footprint=Tab5FirmwareFootprint::snapshot();
  auto& firmware=s.meters[Tab5SystemInfoView::Firmware];
  firmware.total=footprint.valid?footprint.partitionBytes:0;firmware.used=footprint.imageBytes;
  if(footprint.valid)snprintf(firmware.note,sizeof(firmware.note),T("Partition active · reste : %u Kio"),unsigned((firmware.total-firmware.used)/1024));
  else snprintf(firmware.note,sizeof(firmware.note),"%s",T("Indisponible"));
  const size_t hpNonHeap=memory.hpPhysicalBytes>memory.l2CacheBytes+memory.hp.total?memory.hpPhysicalBytes-memory.l2CacheBytes-memory.hp.total:0;
  snprintf(s.memoryLayout,sizeof(s.memoryLayout),T("SRAM HP physique : %u Kio · cache L2 : %u Kio · code, données fixes et réservations : %u Kio\nSPM/TCM : %u Kio physiques · tas : %u/%u octets libres\nSRAM LP : %u Kio physiques · tas : %u/%u octets libres\nLe tas est la mémoire disponible pour les allocations. Le firmware est stocké en flash."),
      unsigned(memory.hpPhysicalBytes/1024),unsigned(memory.l2CacheBytes/1024),unsigned(hpNonHeap/1024),
      unsigned(memory.tcmPhysicalBytes/1024),unsigned(memory.tcm.free),unsigned(memory.tcm.total),
      unsigned(memory.lpPhysicalBytes/1024),unsigned(memory.lp.free),unsigned(memory.lp.total));
  snprintf(s.heading,sizeof(s.heading),T("Informations système · Tab5OS %s"),APP_VERSION);
  const auto compat=Tab5Compatibility::snapshot();
  snprintf(s.hardware,sizeof(s.hardware),T("ESP32-P4 · révision %u · %u MHz\n%s\nAllumé depuis %u min · démarrage %lu ms"),unsigned(ESP.getChipRevision()),unsigned(ESP.getCpuFreqMHz()),T(Tab5Compatibility::displayName(compat.display)),unsigned(millis()/60000),static_cast<unsigned long>(bootStatus.totalMs()));
  const auto& battery=Tab5BatteryGuard::status();
  if(battery.sample.valid)snprintf(s.power,sizeof(s.power),T("Batterie : %.2f V · %+ld mA\n%s"),battery.sample.millivolts/1000.0,static_cast<long>(battery.sample.milliamps),T(battery.sample.charging?"charge signalée":"charge non signalée"));
  else snprintf(s.power,sizeof(s.power),"%s",T(settings.batteryInstalled?"Batterie : mesure indisponible":"Alimentation USB · sans batterie"));
  const auto mesh=Tab5Meshtastic::badge();
  snprintf(s.connections,sizeof(s.connections),T("Wi-Fi : %s\nMeshtastic : %s\nCarte SD : %s\nUSB : %s"),T(Tab5Audio.wifiConnected()?"Connecté":Tab5Audio.wifiEnabled()?"Déconnecté":"désactivé"),T(mesh.active?"actif":mesh.ready?"prêt":"arrêté"),T(sdReady?"Disponible":"absente"),T(Tab5UsbPrinter::ready()?"Imprimante USB IPP":Tab5UsbStorage::status()));
  const auto present=tab5DisplayPresentStats();const auto cache=tab5DisplayCacheStats();
  snprintf(s.display,sizeof(s.display),T("Double tampon : %s\nAttentes expirées : %lu · erreurs : %lu\nReprises écran : %u"),T(present.active?"actif":"indisponible"),static_cast<unsigned long>(present.timeouts),static_cast<unsigned long>(present.copyErrors+present.publishErrors+cache.cacheErrors),unsigned(cache.recoveries));
  Tab5SystemInfoView::update(s);
}

static void refreshSystemInfo() {
  if (!Tab5SystemInfoView::scope() || !systemInfoRefreshRequested) return;
  systemInfoRefreshRequested=false;systemInfoUpdatedAt=millis();
  updateSystemOverview();
  if(!Tab5SystemInfoView::wantsDetails()||!systemInfoDetailsRequested)return;
  systemInfoDetailsRequested=false;
  const float psramMiB = static_cast<float>(
      heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) / (1024.0f * 1024.0f);
  const bool regular = smoothFontReady;
  const bool bold = sdReady && tab5FS.exists(FONT_BOLD_FILE);
  const bool italic = sdReady && tab5FS.exists(FONT_ITALIC_FILE);
  const bool boldItalic = sdReady && tab5FS.exists(FONT_BOLDITALIC_FILE);
  char base[640];
  snprintf(base, sizeof(base), T(
      "Tab5OS v%s · puce %u · SD %s · PSRAM libre %.1f Mio · %s"),
      APP_VERSION, static_cast<unsigned>(ESP.getChipRevision()),
      sdReady ? "OK" : T("absente"), static_cast<double>(psramMiB),
      T(THEMES[settings.themeIndex].name));
  String text(base);
  const auto compatibility = Tab5Compatibility::snapshot();
  char hardware[512];
  snprintf(hardware, sizeof(hardware), T("\nÉcran / tactile : %s"),
      T(Tab5Compatibility::displayName(compatibility.display)));
  text += hardware;
  if (compatibility.chipRevisionRangeKnown) {
    snprintf(hardware, sizeof(hardware), T("\nRévisions P4 ciblées par ce firmware : %u.%u à %u.%u"),
        unsigned(compatibility.minimumChipRevision / 100), unsigned(compatibility.minimumChipRevision % 100),
        unsigned(compatibility.maximumChipRevision / 100), unsigned(compatibility.maximumChipRevision % 100));
    text += hardware;
  }
  char remoteVersion[40] = "—";
  if (Tab5Compatibility::validVersion(compatibility.coprocessor))
    snprintf(remoteVersion, sizeof(remoteVersion), "%lu.%lu.%lu", (unsigned long)compatibility.coprocessor.major,
        (unsigned long)compatibility.coprocessor.minor, (unsigned long)compatibility.coprocessor.patch);
  snprintf(hardware, sizeof(hardware), T("\nWi-Fi : hôte %lu.%lu.%lu · C6 %s\n%s\nTransport C6 : %s · erreurs %lu"),
      (unsigned long)compatibility.host.major, (unsigned long)compatibility.host.minor,
      (unsigned long)compatibility.host.patch, remoteVersion,
      T(Tab5Compatibility::firmwareMessage(compatibility.firmware)),
      T(Tab5Compatibility::transportMessage(compatibility.transport)),
      (unsigned long)compatibility.transportFailures);
  text += hardware;
  const auto& battery = Tab5BatteryGuard::status();
  if (settings.batteryInstalled || battery.sample.valid) {
    char power[360];
    snprintf(power, sizeof(power), T("\nMesure batterie : %.2f V · %+ld mA · %s"),
      battery.sample.millivolts/1000.0, static_cast<long>(battery.sample.milliamps),
      T(!battery.sample.valid ? "mesure invalide ou pack non détecté" :
        battery.sample.charging ? "charge signalée" : "charge non signalée"));
    text += power;
  } else text += T("\nAlimentation USB · sans batterie");
  char protection[160];
  snprintf(protection, sizeof(protection), T("\nProtection batterie : %s"),
      T(battery.countdown ? "arrêt programmé" : battery.sample.valid ? "surveillance active" : "en attente d’une mesure valide"));
  text += protection;
  const auto memory = Tab5MemoryLayout::sample(true);
  char mem[512];
  snprintf(mem,sizeof(mem),T("\nPSRAM : %.1f Mio libres / %.1f Mio disponibles au total"),
      double(memory.psram.free)/(1024*1024),double(memory.psram.total)/(1024*1024));
  text+=mem;
  snprintf(mem,sizeof(mem),T("\nTas SRAM HP : %u/%u Kio libres · bloc maximal : %u Kio\nTas SPM/TCM : %u/%u octets libres · bloc maximal : %u octets\nTas SRAM LP : %u/%u octets libres · bloc maximal : %u octets\nAgrégat interne HP+SPM/TCM+LP : %u/%u Kio libres"),
      unsigned(memory.hp.free/1024),unsigned(memory.hp.total/1024),unsigned(memory.hp.largestFree/1024),
      unsigned(memory.tcm.free),unsigned(memory.tcm.total),unsigned(memory.tcm.largestFree),
      unsigned(memory.lp.free),unsigned(memory.lp.total),unsigned(memory.lp.largestFree),
      unsigned(memory.internal.free/1024),unsigned(memory.internal.total/1024));
  text+=mem;
  snprintf(mem,sizeof(mem),T("\nCaches libérés : interne %u Kio · PSRAM %u Kio\nRefus mémoire %u · pile UI min %u o"),
      unsigned(Tab5MemoryManager.reclaimedInternal()/1024),unsigned(Tab5MemoryManager.reclaimedPsram()/1024),Tab5MemoryManager.denied(),
      unsigned(uxTaskGetStackHighWaterMark(nullptr)));
  text += mem;
  snprintf(mem,sizeof(mem),T("\nTampons TLS : %s"),tab5TlsMemoryExternal()?"PSRAM":T("RAM interne (repli SDK)"));
  text += mem;
#if defined(CONFIG_CACHE_L2_CACHE_SIZE)
  snprintf(mem,sizeof(mem),T("\nSRAM HP physique : 768 Kio, dont %u Kio de cache matériel"),
      unsigned(CONFIG_CACHE_L2_CACHE_SIZE/1024));
  text += mem;
#endif
  const auto displaySignals=tab5DisplaySignalsStats();
  if(displaySignals.active){
    snprintf(mem,sizeof(mem),T("\nDSI : observation active · manques de données : %lu"),
        static_cast<unsigned long>(displaySignals.underruns));
    text+=mem;
    if(displaySignals.underruns){
      snprintf(mem,sizeof(mem),T("\nDernier manque de données : %llu s après démarrage"),
          static_cast<unsigned long long>(displaySignals.lastUnderrunUs/1000000));
      text+=mem;
    }
  }else text+=T("\nDSI : observation indisponible");
  const auto displayCache=tab5DisplayCacheStats();
  snprintf(mem,sizeof(mem),T("\nÉcran : %llu Kio synchronisés / %llu Kio demandés\nActualisations ciblées %u · replis %u · erreurs %u"),
      static_cast<unsigned long long>(displayCache.syncedBytes/1024),
      static_cast<unsigned long long>(displayCache.requestedBytes/1024),
      unsigned(displayCache.filteredFrames),unsigned(displayCache.fallbackFrames),
      unsigned(displayCache.cacheErrors));
  text += mem;
  snprintf(mem,sizeof(mem),T("\nReprise écran : %u/%u · code %ld%s"),
      unsigned(displayCache.recoveries),unsigned(displayCache.recoveryAttempts),
      static_cast<long>(displayCache.lastCacheError),
      displayCache.pendingRecovery?T(" · en attente"):"");
  text+=mem;
  snprintf(mem,sizeof(mem),T("\nI2C audio : erreurs lecture %u · écriture %u"),
      unsigned(Tab5AudioDevice.headphoneReadErrors()),unsigned(Tab5AudioDevice.headphoneRouteErrors()));
  text+=mem;
  snprintf(mem,sizeof(mem),T("\nDernier reset : %s · allumé depuis %u min"),T(resetReasonName(esp_reset_reason())),unsigned(millis()/60000));
  text += mem;
  const char* previousContext=tab5DiagnosticPrevious();
  if(previousContext && *previousContext){text+=T("\nAvant le reset (contexte) : ");text+=previousContext;}
  snprintf(mem,sizeof(mem),T("\nBoot %lu ms | %s %lu ms | tas %s"),
      (unsigned long)bootStatus.totalMs(),T(bootStatus.slowestName()),
      (unsigned long)bootStatus.slowestMs(),bootStatus.healthy()?"OK":T("à vérifier"));
  text += mem;
  snprintf(mem,sizeof(mem),T("\nLangue %lu ms · clavier %lu ms · USB %lu ms"),
      (unsigned long)bootStatus.duration(Tab5BootStatus::Language),
      (unsigned long)bootStatus.duration(Tab5BootStatus::Keyboard),
      (unsigned long)bootStatus.duration(Tab5BootStatus::Usb));
  text += mem;
  snprintf(mem,sizeof(mem),T("\nSD : %s | reprises %u/3 | recuperations %u%s"),
      T(tab5StorageErrorText()),tab5StorageRecoveryAttempts(),tab5StorageRecoveries(),
      tab5StorageRecoveryExhausted()?T(" - retest manuel nécessaire"):"");
  text += mem;
  const auto displayBus=tab5DisplayBandwidthStats();
  snprintf(mem,sizeof(mem),T("\nPriorité lecture écran : %u → %u · %s\nChiffrement flash : %s"),
      unsigned(displayBus.initialDisplayRead),unsigned(displayBus.currentDisplayRead),
      T(displayBus.verified?"vérifiée":displayBus.supported?"à vérifier":"indisponible"),
      T(displayBus.flashEncrypted?"activé":"désactivé"));
  text+=mem;
  snprintf(mem,sizeof(mem),T("\nPython : %s · démarrages VM : %lu"),
      T(Tab5Python::running()?"actif":"arrêté"),static_cast<unsigned long>(Tab5Python::vmStartCount()));
  text+=mem;
  const auto presentation = tab5DisplayPresentStats();
  snprintf(mem,sizeof(mem),T("\nDouble tampon : %s · images %u/%u · bascules %lu"),
      T(presentation.active?"actif":"indisponible"),unsigned(presentation.front),unsigned(presentation.back),
      static_cast<unsigned long>(presentation.flips));
  text+=mem;
  snprintf(mem,sizeof(mem),T("\nÉcran : attentes expirées %lu · erreurs %lu/%lu · code %ld"),
      static_cast<unsigned long>(presentation.timeouts),static_cast<unsigned long>(presentation.copyErrors),
      static_cast<unsigned long>(presentation.publishErrors),static_cast<long>(presentation.lastError));
  text+=mem;
  static Tab5DisplayActivity::Stats previousActivity;
  const auto currentActivity = Tab5DisplayActivity::snapshot();
  const auto activity = Tab5DisplayActivity::difference(currentActivity, previousActivity);
  snprintf(mem,sizeof(mem),T("\nRendu depuis le précédent relevé : %lu images · %lu zones · %llu pixels"),
      static_cast<unsigned long>(activity.frames),static_cast<unsigned long>(activity.flushAreas),
      static_cast<unsigned long long>(activity.flushedPixels));
  text+=mem;
  text+=T("\nDemandes de redessin par service (zones pouvant se chevaucher) :");
  for(unsigned i=0;i<Tab5DisplayActivity::sourceCount;++i) {
    if(!activity.invalidations[i])continue;
    snprintf(mem,sizeof(mem),"\n  %s : %lu",T(Tab5DisplayActivity::sourceName(static_cast<Tab5DisplayActivity::Source>(i))),
        static_cast<unsigned long>(activity.invalidations[i]));
    text+=mem;
  }
  snprintf(mem,sizeof(mem),T("\nGrandes zones : %lu · dernière à %lu ms · %lu pixels · %s"),
      static_cast<unsigned long>(currentActivity.largeInvalidations),static_cast<unsigned long>(currentActivity.lastLargeInvalidationMs),
      static_cast<unsigned long>(currentActivity.lastLargeInvalidationPixels),T(Tab5DisplayActivity::sourceName(currentActivity.lastLargeSource)));
  text+=mem;
  snprintf(mem,sizeof(mem),T("\nGrandes images : %lu · dernière à %lu ms · maximum %lu pixels"),
      static_cast<unsigned long>(currentActivity.largeFrames),static_cast<unsigned long>(currentActivity.lastLargeFrameMs),static_cast<unsigned long>(currentActivity.maxFramePixels));
  text+=mem;
  previousActivity=currentActivity;
  const auto timing=Tab5UiDiagnostics::snapshot();
  const auto& uiTiming=timing.phases[Tab5UiDiagnostics::Loop];
  snprintf(mem,sizeof(mem),T("\nLatence UI : max %lu ms · intervalle max %lu ms · cycles ≥20 ms : %lu"),
      static_cast<unsigned long>(uiTiming.maxUs/1000),static_cast<unsigned long>(timing.maxGapUs/1000),
      static_cast<unsigned long>(uiTiming.slow));
  text+=mem;
  text+=T("\nDurées écoulées, pas une mesure CPU. Activer dans Réglages pour enregistrer.");
  for(unsigned i=0;i<Tab5UiDiagnostics::Count;++i) {
    if(!timing.phases[i].slow)continue;
    snprintf(mem,sizeof(mem),"\n  %s : %lu ms",T(Tab5UiDiagnostics::phaseName(i)),static_cast<unsigned long>(timing.phases[i].maxUs/1000));
    text+=mem;
  }
  if(Tab5UiDiagnostics::recent(mem,sizeof(mem),true)){
    text+=T("\nAvant le reset (contexte) : ");text+='\n';text+=mem;
  }
  if(settings.batteryInstalled && (!battery.sample.valid || battery.sample.millivolts<6000))
    text+=T("\nTension basse ou pack non détecté : vérifier contacts / protection / chargeur.");
  if (!regular || !bold || !italic || !boldItalic) {
    text += T("\nPolices manquantes : ");
    bool first = true;
    if (!regular) { text += "font.ttf"; first = false; }
    if (!bold) { if (!first) text += ", "; text += "bold.ttf"; first = false; }
    if (!italic) { if (!first) text += ", "; text += "italic.ttf"; first = false; }
    if (!boldItalic) { if (!first) text += ", "; text += "bolditalic.ttf"; }
  }
  Tab5SystemInfoView::details(text.c_str());

}

static void showSettingsSection(uint8_t section) {
  if (section >= SETTINGS_SECTION_COUNT) section = 0;
  activeSettingsSection = section;
  if(section==3&&!settingsStorageMounted&&settingsSectionPanels[3]) {
    Tab5Tools.mountStorage(settingsSectionPanels[3]);settingsStorageMounted=true;
    Tab5UiStyle::tree(settingsSectionPanels[3]);
  }
  for (uint8_t i = 0; i < SETTINGS_SECTION_COUNT; ++i) {
    if (settingsSectionPanels[i]) {
      if (i == section)
        lv_obj_remove_flag(settingsSectionPanels[i], LV_OBJ_FLAG_HIDDEN);
      else
        lv_obj_add_flag(settingsSectionPanels[i], LV_OBJ_FLAG_HIDDEN);
    }
    if (settingsSectionButtons[i]) {
      if (i == section)
        lv_obj_add_state(settingsSectionButtons[i], LV_STATE_CHECKED);
      else
        lv_obj_remove_state(settingsSectionButtons[i], LV_STATE_CHECKED);
    }
  }
}

static void settingsSectionEvent(lv_event_t* e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
  const uint8_t section = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(
      lv_event_get_user_data(e)));
  showSettingsSection(section);
}

static void updateFileShareUi(uint32_t now, bool force = false) {
  if (!fileShareStatusLabel) return;
  static uint32_t last = 0;
  if (!force && now - last < 500U) return;
  last = now;
  const bool active = tab5FileShareActive();
  const bool busy = tab5FileShareBusy();
  char status[512];
  if (active) {
    if(tab5FileShareAccessPoint())
      snprintf(status,sizeof(status),T("Réseau : %s · mot de passe : %s\nNavigateur : %s · code : %06lu\nLecture et écriture · %lu min restantes"),
          tab5FileShareSsid(),tab5FileSharePassword(),tab5FileShareUrl(),
          static_cast<unsigned long>(tab5FileShareCode()),
          static_cast<unsigned long>((tab5FileShareSecondsRemaining()+59U)/60U));
    else snprintf(status,sizeof(status),T("Navigateur : %s · code : %06lu\nLecture et écriture · %lu min restantes"),
          tab5FileShareUrl(),static_cast<unsigned long>(tab5FileShareCode()),
          static_cast<unsigned long>((tab5FileShareSecondsRemaining()+59U)/60U));
  } else if(*tab5FileShareError()) snprintf(status,sizeof(status),"%s",T(tab5FileShareError()));
  else if(busy) snprintf(status,sizeof(status),"%s",T("Préparation ou fermeture du partage…"));
  else status[0]=0;
  setLabelIfChanged(fileShareStatusLabel,status);
  if(*status)lv_obj_remove_flag(fileShareStatusLabel,LV_OBJ_FLAG_HIDDEN);
  else lv_obj_add_flag(fileShareStatusLabel,LV_OBJ_FLAG_HIDDEN);
  setTopActionVisible(fileShareLanButton,!active&&!busy);
  setTopActionVisible(fileShareDirectButton,!active&&!busy);
  setTopActionVisible(fileShareStopButton,active);
  setButtonEnabled(fileShareStopButton,active);
}

static void startFileShare(bool direct) {
  if(!tab5FileShareStart(direct,Tab5Locale.selectedId().startsWith("fr")))
    toast(*tab5FileShareError()?tab5FileShareError():"Impossible d’activer le partage",5000);
  updateFileShareUi(millis(),true);
}

static bool wifiChangeAllowed() {
  if(!tab5FileShareBusy())return true;
  toast("Arrêter le partage de fichiers avant de modifier le Wi-Fi",4000);
  return false;
}

#include "Tab5ShellUi.h"
#include "Tab5GamepadSettings.inc"
#include "Tab5SettingsLayout.h"
#include "Tab5SerialSettings.inc"

static void refreshDefaultFontButtons(){
  const char* paths[]={settings.interfaceFontPath,settings.documentFontPath};
  for(unsigned i=0;i<2;++i)if(defaultFontButtons[i]){
    const char* base=strrchr(paths[i],'/');
    lv_label_set_text(lv_obj_get_child(defaultFontButtons[i],0),*paths[i]?(base?base+1:paths[i]):T("Police standard"));
  }
}
static void chooseDefaultFont(lv_event_t* event){
  const auto role=reinterpret_cast<uintptr_t>(lv_event_get_user_data(event));
  setKeyboardVisible(false);
  Tab5FilePicker::open(FONT_DIR,".ttf",false,[](const char* path,void* context){
    if(!path)return;
    const auto result=Tab5FontDefaults::inspect(path);
    if(result!=Tab5FontDefaults::Result::Ok){toast(Tab5FontDefaults::message(result),4500);return;}
    const bool documents=reinterpret_cast<uintptr_t>(context)!=0;
    const auto budget=Tab5FontDefaults::checkSelectionBudget(path,documents?settings.interfaceFontPath:settings.documentFontPath);
    if(budget!=Tab5FontDefaults::Result::Ok){toast(Tab5FontDefaults::message(budget),4500);return;}
    auto* selected=documents?settings.documentFontPath:settings.interfaceFontPath;
    snprintf(selected,Tab5FontDefaults::PathCapacity,"%s",path);
    markSettingsDirty();refreshDefaultFontButtons();toast("Redémarrez pour appliquer les polices",4500);
  },reinterpret_cast<void*>(role));
}
static void resetDefaultFont(lv_event_t* event){
  auto* selected=reinterpret_cast<uintptr_t>(lv_event_get_user_data(event))?settings.documentFontPath:settings.interfaceFontPath;
  selected[0]=0;markSettingsDirty();refreshDefaultFontButtons();toast("Redémarrez pour appliquer les polices",4500);
}

#include "Tab5DefaultFontUi.inc"

static void createSettingsPage() {
  Tab5Mouse::configure(settings.mouse);
  Tab5Mouse::pageScroll([](int steps){writing::KeyEvent e;e.key=steps>0?writing::Key::PageUp:writing::Key::PageDown;if(Tab5PythonReader::visible())Tab5PythonReader::key(e);else if(readerActive){if(steps>0)readerGoPrevPage();else readerGoNextPage();}});
  settingsPage = lv_obj_create(pageHost);
  lv_obj_set_size(settingsPage, WORK_W, PAGE_H);Tab5Shell::width(settingsPage,0);
  lv_obj_set_pos(settingsPage, 0, 0);
  lv_obj_set_style_border_width(settingsPage, 0, 0);
  lv_obj_set_style_radius(settingsPage, 0, 0);
  lv_obj_set_style_pad_all(settingsPage, 0, 0);
  lv_obj_set_style_bg_color(settingsPage, C_BG(), 0);Tab5ThemeLive::bind(settingsPage,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(settingsPage, LV_OPA_COVER, 0);
  lv_obj_remove_flag(settingsPage, LV_OBJ_FLAG_SCROLLABLE);

  const int margin = 16;
  const int gap = 14;
  const int tabsY = 8;
  const int tabsH = 48;
  const int tabW = (WORK_W - margin * 2 - gap * (SETTINGS_SECTION_COUNT-1)) / SETTINGS_SECTION_COUNT;
  const char* sectionNames[SETTINGS_SECTION_COUNT] = {
      "Interface", "Système", "Langue", "Stockage"};
  for (uint8_t i = 0; i < SETTINGS_SECTION_COUNT; ++i) {
    settingsSectionButtons[i] = makeButton(
        settingsPage, sectionNames[i], tabW, tabsH);
    lv_obj_set_pos(settingsSectionButtons[i], margin + i * (tabW + gap),
                   tabsY);
    lv_obj_add_flag(settingsSectionButtons[i], LV_OBJ_FLAG_CHECKABLE);
    // Fixed, transparent tabs: the active tab is marked by a thin underline.
    lv_obj_set_style_radius(settingsSectionButtons[i],0,0);
    lv_obj_set_style_bg_opa(settingsSectionButtons[i],LV_OPA_TRANSP,0);
    lv_obj_set_style_bg_opa(settingsSectionButtons[i],LV_OPA_TRANSP,LV_STATE_CHECKED);
    lv_obj_set_style_border_width(settingsSectionButtons[i],0,0);
    lv_obj_set_style_border_width(settingsSectionButtons[i],2,LV_STATE_CHECKED);
    lv_obj_set_style_border_side(settingsSectionButtons[i],LV_BORDER_SIDE_BOTTOM,LV_STATE_CHECKED);
    lv_obj_set_style_border_color(settingsSectionButtons[i],C_TEXT(),LV_STATE_CHECKED);
    lv_obj_add_event_cb(settingsSectionButtons[i], settingsSectionEvent,
                        LV_EVENT_CLICKED,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(i)));

    settingsSectionPanels[i] = lv_obj_create(settingsPage);
    lv_obj_remove_style_all(settingsSectionPanels[i]);
    lv_obj_set_size(settingsSectionPanels[i], WORK_W, PAGE_H - 70);Tab5Shell::width(settingsSectionPanels[i],0);
    lv_obj_set_pos(settingsSectionPanels[i], 0, 70);
    lv_obj_remove_flag(settingsSectionPanels[i], LV_OBJ_FLAG_SCROLLABLE);
  }

  auto* separator=lv_obj_create(settingsPage);lv_obj_remove_style_all(separator);
  lv_obj_set_pos(separator,16,62);lv_obj_set_size(separator,WORK_W-32,1);Tab5Shell::width(separator,-32);
  lv_obj_set_style_bg_opa(separator,LV_OPA_COVER,0);lv_obj_set_style_bg_color(separator,C_BORDER(),0);
  lv_obj_remove_flag(separator,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE));
  lv_obj_add_flag(settingsSectionPanels[3],LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(settingsSectionPanels[3],LV_DIR_VER);
  const int cardW = (WORK_W - margin * 2 - gap) / 2;
  lv_obj_add_flag(settingsSectionPanels[0],LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(settingsSectionPanels[0],LV_DIR_VER);
  lv_obj_add_flag(settingsSectionPanels[1],LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(settingsSectionPanels[1],LV_DIR_VER);

  lv_obj_t* displayCard = makeSettingsCard(
      settingsSectionPanels[0], margin, 0, cardW, 452, "Affichage");
  lv_obj_t* editCard = makeSettingsCard(
      settingsSectionPanels[0], margin + cardW + gap, 0, cardW, 310,
      "Édition");

  auto* mouseCard=makeSettingsCard(settingsSectionPanels[0],margin,466,WORK_W-margin*2,224,"Souris");
  auto* mouseSensitivityCaption=makeSettingsText(mouseCard,"Sensibilité",20,66,220,false);
  auto* sensitivity=lv_slider_create(mouseCard);lv_obj_set_pos(sensitivity,260,70);lv_obj_set_size(sensitivity,230,20);
  styleSettingsSlider(sensitivity);lv_slider_set_range(sensitivity,25,300);lv_slider_set_value(sensitivity,settings.mouse.speed,LV_ANIM_OFF);
  auto* speedLabel=makeSettingsText(mouseCard,"",500,62,100,true);lv_label_set_text_fmt(speedLabel,"%u %%",settings.mouse.speed);
  lv_obj_add_event_cb(sensitivity,[](lv_event_t* e){settings.mouse.speed=lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));Tab5Mouse::configure(settings.mouse);lv_label_set_text_fmt((lv_obj_t*)lv_event_get_user_data(e),"%u %%",settings.mouse.speed);markSettingsDirty();},LV_EVENT_VALUE_CHANGED,speedLabel);
  auto* mouseScrollCaption=makeSettingsText(mouseCard,"Défilement",20,124,220,false);
  auto* scrollSpeed=lv_slider_create(mouseCard);lv_obj_set_pos(scrollSpeed,260,128);lv_obj_set_size(scrollSpeed,230,20);
  styleSettingsSlider(scrollSpeed);lv_slider_set_range(scrollSpeed,1,12);lv_slider_set_value(scrollSpeed,settings.mouse.scroll,LV_ANIM_OFF);
  auto* scrollLabel=makeSettingsText(mouseCard,"",500,120,100,true);lv_label_set_text_fmt(scrollLabel,"%u",settings.mouse.scroll);
  lv_obj_add_event_cb(scrollSpeed,[](lv_event_t* e){settings.mouse.scroll=lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));Tab5Mouse::configure(settings.mouse);lv_label_set_text_fmt((lv_obj_t*)lv_event_get_user_data(e),"%u",settings.mouse.scroll);markSettingsDirty();},LV_EVENT_VALUE_CHANGED,scrollLabel);
  auto* mousePrimaryCaption=makeSettingsText(mouseCard,"Bouton principal droit",620,64,330,false);
  auto* handed=lv_switch_create(mouseCard);styleSettingsSwitch(handed);lv_obj_set_pos(handed,980,55);if(settings.mouse.leftHanded)lv_obj_add_state(handed,LV_STATE_CHECKED);
  lv_obj_add_event_cb(handed,[](lv_event_t* e){settings.mouse.leftHanded=lv_obj_has_state((lv_obj_t*)lv_event_get_target(e),LV_STATE_CHECKED);Tab5Mouse::configure(settings.mouse);markSettingsDirty();},LV_EVENT_VALUE_CHANGED,nullptr);
  auto* mouseNaturalCaption=makeSettingsText(mouseCard,"Défilement naturel",620,122,330,false);
  auto* natural=lv_switch_create(mouseCard);styleSettingsSwitch(natural);lv_obj_set_pos(natural,980,113);if(settings.mouse.natural)lv_obj_add_state(natural,LV_STATE_CHECKED);
  lv_obj_add_event_cb(natural,[](lv_event_t* e){settings.mouse.natural=lv_obj_has_state((lv_obj_t*)lv_event_get_target(e),LV_STATE_CHECKED);Tab5Mouse::configure(settings.mouse);markSettingsDirty();},LV_EVENT_VALUE_CHANGED,nullptr);
  createGamepadSettings(settingsSectionPanels[0],704);
  lv_obj_t* systemCard = makeSettingsCard(
      settingsSectionPanels[1], margin, 0, WORK_W - margin * 2, 184,
      "Système");
  lv_obj_t* energyCard = makeSettingsCard(
      settingsSectionPanels[1], margin, 198, WORK_W - margin * 2, 190, "Énergie");
  auto* energySleepCaption=makeSettingsText(energyCard, "Écran seul", 20, 64, 210, false);
  lv_obj_t* sleepChoice = lv_dropdown_create(energyCard);
  lv_obj_set_size(sleepChoice, 240, 46);
  lv_obj_set_pos(sleepChoice, 230, 50);
  lv_dropdown_set_options(sleepChoice, T("Jamais\n1 minute\n2 minutes\n5 minutes\n10 minutes"));
  lv_dropdown_set_selected(sleepChoice, settings.screenSleepIndex);
  styleSettingsDropdown(sleepChoice);
  lv_obj_add_event_cb(sleepChoice, [](lv_event_t* e) {
    settings.screenSleepIndex = lv_dropdown_get_selected(static_cast<lv_obj_t*>(lv_event_get_target(e)));
    screenIdle.wake(millis());
    markSettingsDirty();
  }, LV_EVENT_VALUE_CHANGED, nullptr);
  auto* energyWifiCaption=makeSettingsText(energyCard, "Économie Wi-Fi au repos", 540, 64, 360, false);
  lv_obj_t* wifiSave = lv_switch_create(energyCard);
  styleSettingsSwitch(wifiSave);
  lv_obj_set_pos(wifiSave, 990, 55);
  if (settings.wifiPowerSave) lv_obj_add_state(wifiSave, LV_STATE_CHECKED);
  lv_obj_add_event_cb(wifiSave, [](lv_event_t* e) {
    if(!wifiChangeAllowed()) {
      auto* sw=static_cast<lv_obj_t*>(lv_event_get_target(e));
      if(settings.wifiPowerSave)lv_obj_add_state(sw,LV_STATE_CHECKED);
      else lv_obj_remove_state(sw,LV_STATE_CHECKED);
      return;
    }
    settings.wifiPowerSave = lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(e)), LV_STATE_CHECKED);
    Tab5Audio.setWifiPowerSave(settings.wifiPowerSave);
    markSettingsDirty();
  }, LV_EVENT_VALUE_CHANGED, nullptr);

  auto* energyBatteryCaption=makeSettingsText(energyCard, "Batterie installée", 20, 125, 350, false);
  lv_obj_t* batterySwitch = lv_switch_create(energyCard);
  styleSettingsSwitch(batterySwitch);
  lv_obj_set_pos(batterySwitch, 390, 116);
  if (settings.batteryInstalled) lv_obj_add_state(batterySwitch, LV_STATE_CHECKED);
  lv_obj_add_event_cb(batterySwitch, [](lv_event_t* e) {
    settings.batteryInstalled = lv_obj_has_state(static_cast<lv_obj_t*>(lv_event_get_target(e)), LV_STATE_CHECKED);
    markSettingsDirty();
    updateBatteryLabel(true);
  }, LV_EVENT_VALUE_CHANGED, nullptr);

  auto* shareCard = makeSettingsCard(settingsSectionPanels[1], margin, 402,
      WORK_W - margin * 2, 280, "Partage");
  fileShareLanButton = makeButton(shareCard, "Réseau actuel", 320, 50);
  lv_obj_set_pos(fileShareLanButton,20,58);
  lv_obj_add_event_cb(fileShareLanButton,[](lv_event_t*){startFileShare(false);},LV_EVENT_CLICKED,nullptr);
  fileShareDirectButton = makeButton(shareCard, "Réseau direct", 320, 50);
  lv_obj_set_pos(fileShareDirectButton,366,58);
  lv_obj_add_event_cb(fileShareDirectButton,[](lv_event_t*){startFileShare(true);},LV_EVENT_CLICKED,nullptr);
  fileShareStopButton = makeButton(shareCard, "Arrêter", 320, 50);
  lv_obj_set_pos(fileShareStopButton,712,58);
  lv_obj_add_event_cb(fileShareStopButton,[](lv_event_t*){tab5FileShareStop();updateFileShareUi(millis(),true);},LV_EVENT_CLICKED,nullptr);
  fileShareStatusLabel=makeSettingsText(shareCard,"",20,124,1080,false);
  lv_label_set_long_mode(fileShareStatusLabel,LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_line_space(fileShareStatusLabel,5,0);
  updateFileShareUi(millis(),true);

  lv_obj_t* regionCard = makeSettingsCard(
      settingsSectionPanels[2], margin, 0, WORK_W - margin * 2, 250,
      "Langue, clavier et fichiers");Tab5Shell::width(regionCard,-margin*2);

  auto* displayBrightnessCaption=makeSettingsText(displayCard, "Luminosité", 20, 62, 250, false);
  brightnessValue = makeSettingsText(displayCard, "", cardW - 116, 62, 82, true);
  lv_obj_set_style_text_align(brightnessValue, LV_TEXT_ALIGN_RIGHT, 0);
  brightnessSlider = lv_slider_create(displayCard);
  lv_obj_set_size(brightnessSlider, cardW - 40, 22);
  lv_obj_set_pos(brightnessSlider, 20, 101);
  styleSettingsSlider(brightnessSlider);
  lv_slider_set_range(brightnessSlider, 20, 255);
  lv_slider_set_value(brightnessSlider, settings.brightness, LV_ANIM_OFF);
  lv_obj_add_event_cb(brightnessSlider, brightnessEvent, LV_EVENT_VALUE_CHANGED, nullptr);
  char b1[20];
  snprintf(b1, sizeof(b1), "%u", settings.brightness);
  lv_label_set_text(brightnessValue, b1);

  auto* fontOptions=makeButton(displayCard,"Polices et texte…",cardW-40,48);
  lv_obj_add_event_cb(fontOptions,[](lv_event_t*){showDefaultFontPopup();},LV_EVENT_CLICKED,nullptr);

  auto* displayThemeCaption=makeSettingsText(displayCard, "Thème", 20, 255, 110, false);
  themeDropdown=lv_dropdown_create(displayCard);
  lv_obj_set_pos(themeDropdown,142,242);
  lv_obj_set_size(themeDropdown,cardW-292,46);
  styleSettingsDropdown(themeDropdown);
  refreshThemeChoices();
  Tab5Focus.describe(themeDropdown,T("Thème"));
  lv_obj_add_event_cb(themeDropdown,themeChoiceEvent,LV_EVENT_VALUE_CHANGED,nullptr);
  lv_obj_add_event_cb(themeDropdown,[](lv_event_t*){reloadThemeCatalogue();Tab5UiStyle::control(themeDropdown);},LV_EVENT_READY,nullptr);
  auto* themeEdit=makeButton(displayCard,"Couleurs",112,44);
  lv_obj_set_pos(themeEdit,cardW-132,242);
  Tab5Focus.describe(themeEdit,T("Personnaliser le thème"));
  lv_obj_add_event_cb(themeEdit,[](lv_event_t*){setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Theme);},LV_EVENT_CLICKED,nullptr);
  auto* displayRadiusCaption=makeSettingsText(displayCard,"Arrondi",20,322,110,false);
  auto* radius=lv_slider_create(displayCard);lv_obj_set_pos(radius,154,330);lv_obj_set_size(radius,cardW-238,18);
  styleSettingsSlider(radius);lv_slider_set_range(radius,0,24);lv_slider_set_value(radius,settings.cornerRadius,LV_ANIM_OFF);
  Tab5Focus.describe(radius,T("Arrondi des éléments"));
  auto* radiusValue=makeSettingsText(displayCard,"",cardW-65,318,45,true);lv_label_set_text_fmt(radiusValue,"%u",settings.cornerRadius);
  lv_obj_add_event_cb(radius,[](lv_event_t* e){
    settings.cornerRadius=lv_slider_get_value(lv_event_get_current_target_obj(e));
    lv_label_set_text_fmt(static_cast<lv_obj_t*>(lv_event_get_user_data(e)),"%u",settings.cornerRadius);
    applyUiCorners();
  },LV_EVENT_VALUE_CHANGED,radiusValue);
  auto* homeOptions=makeButton(displayCard,"Accueil et bandeau",cardW-40,50);lv_obj_set_pos(homeOptions,20,370);
  lv_obj_add_event_cb(homeOptions,[](lv_event_t*){showSidebarSettings();},LV_EVENT_CLICKED,nullptr);

  auto* orientationCaption=makeSettingsText(displayCard,"Rotation de l’écran à 180°",20,430,cardW-130,false);
  auto* orientationSwitch=lv_switch_create(displayCard);styleSettingsSwitch(orientationSwitch);
  if(settings.screenUpsideDown)lv_obj_add_state(orientationSwitch,LV_STATE_CHECKED);
  Tab5Focus.describe(orientationSwitch,T("Rotation de l’écran à 180°"));
  lv_obj_add_event_cb(orientationSwitch,[](lv_event_t* e){
    settings.screenUpsideDown=lv_obj_has_state(lv_event_get_current_target_obj(e),LV_STATE_CHECKED);
    displayOrientationPending=true;markSettingsDirty();
  },LV_EVENT_VALUE_CHANGED,nullptr);

  auto* editAutosaveCaption=makeSettingsText(editCard, "Autosauvegarde", 20, 68, 330, false);
  autosaveSwitch = lv_switch_create(editCard);
  styleSettingsSwitch(autosaveSwitch);
  lv_obj_set_pos(autosaveSwitch, cardW - 94, 57);
  if (settings.autosave) lv_obj_add_state(autosaveSwitch, LV_STATE_CHECKED);
  lv_obj_add_event_cb(autosaveSwitch, autosaveEvent, LV_EVENT_VALUE_CHANGED, nullptr);

  auto* editDelayCaption=makeSettingsText(editCard, "Délai autosauvegarde", 20, 138, 330, false);
  autosaveDelayValue = makeSettingsText(editCard, "", cardW - 116, 138, 82, true);
  lv_obj_set_style_text_align(autosaveDelayValue, LV_TEXT_ALIGN_RIGHT, 0);
  autosaveDelaySlider = lv_slider_create(editCard);
  lv_obj_set_size(autosaveDelaySlider, cardW - 40, 22);
  lv_obj_set_pos(autosaveDelaySlider, 20, 178);
  styleSettingsSlider(autosaveDelaySlider);
  lv_slider_set_range(autosaveDelaySlider, 2, 30);
  lv_slider_set_value(autosaveDelaySlider, settings.autosaveDelaySec, LV_ANIM_OFF);
  lv_obj_add_event_cb(autosaveDelaySlider, autosaveDelayEvent, LV_EVENT_VALUE_CHANGED, nullptr);
  char b2[20];
  snprintf(b2, sizeof(b2), "%u s", settings.autosaveDelaySec);
  lv_label_set_text(autosaveDelayValue, b2);

  auto* keysButton = makeButton(editCard, "Clés de chiffrement", cardW - 40, 48);
  lv_obj_set_pos(keysButton, 20, 230);
  lv_obj_add_event_cb(keysButton, [](lv_event_t*) { setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Keys); }, LV_EVENT_CLICKED, nullptr);
  auto* secretsButton=makeButton(editCard,"Coffre de mots de passe",cardW-40,48);
  lv_obj_add_event_cb(secretsButton,[](lv_event_t*){setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Secrets);},LV_EVENT_CLICKED,nullptr);

  lv_obj_t* clockPanel = lv_obj_create(systemCard);
  lv_obj_set_size(clockPanel, 550, 116);
  lv_obj_set_pos(clockPanel, 20, 46);
  lv_obj_set_style_bg_color(clockPanel, C_BG(), 0);Tab5ThemeLive::bind(clockPanel,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(clockPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(clockPanel, C_BORDER(), 0);Tab5ThemeLive::bind(clockPanel,LV_STYLE_BORDER_COLOR,0,3);
  lv_obj_set_style_border_width(clockPanel, 1, 0);
  lv_obj_set_style_radius(clockPanel, 10, 0);
  lv_obj_set_style_pad_all(clockPanel, 0, 0);
  lv_obj_remove_flag(clockPanel, LV_OBJ_FLAG_SCROLLABLE);

  auto* clockTitle=makeSettingsText(clockPanel, "HORLOGE", 14, 17, 100, false);
  rtcValue = makeSettingsText(clockPanel, "", 126, 17, 330, true);
  lv_label_set_long_mode(rtcValue, LV_LABEL_LONG_CLIP);

  timeSetButton = makeIconButton(clockPanel, LV_SYMBOL_EDIT, 58, 44);
  lv_obj_set_pos(timeSetButton, 476, 6);
  lv_obj_add_event_cb(timeSetButton, clockSetEvent, LV_EVENT_CLICKED, nullptr);

  String zoneOptions;
  zoneOptions.reserve(TIME_ZONE_COUNT * 30);
  for (size_t i = 0; i < TIME_ZONE_COUNT; ++i) {
    if (i) zoneOptions += '\n';
    zoneOptions += T(TIME_ZONES[i].label);
  }
  auto* clockManualCaption=makeSettingsText(clockPanel, "Manuel", 14, 72, 78, false);
  ntpSwitch = lv_switch_create(clockPanel);
  styleSettingsSwitch(ntpSwitch);
  lv_obj_set_pos(ntpSwitch, 96, 65);
  if (settings.ntpEnabled) lv_obj_add_state(ntpSwitch, LV_STATE_CHECKED);
  lv_obj_add_event_cb(ntpSwitch, ntpEvent, LV_EVENT_VALUE_CHANGED, nullptr);
  auto* clockNtpCaption=makeSettingsText(clockPanel, "NTP", 181, 72, 55, false);

  timeZoneDropdown = lv_dropdown_create(clockPanel);
  lv_obj_set_size(timeZoneDropdown, 294, 46);
  lv_obj_set_pos(timeZoneDropdown, 240, 61);
  lv_dropdown_set_options(timeZoneDropdown, zoneOptions.c_str());
  lv_dropdown_set_selected(timeZoneDropdown, settings.timeZoneIndex);
  styleSettingsDropdown(timeZoneDropdown);
  lv_obj_add_event_cb(timeZoneDropdown, timeZoneEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);

  refreshTimeSettingsUi();
  updateClockLabels(true);

  lv_obj_t* wifiPanel = lv_obj_create(systemCard);
  lv_obj_set_size(wifiPanel, 540, 116);
  lv_obj_set_pos(wifiPanel, 590, 46);
  lv_obj_set_style_bg_color(wifiPanel, C_BG(), 0);Tab5ThemeLive::bind(wifiPanel,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(wifiPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(wifiPanel, C_BORDER(), 0);Tab5ThemeLive::bind(wifiPanel,LV_STYLE_BORDER_COLOR,0,3);
  lv_obj_set_style_border_width(wifiPanel, 1, 0);
  lv_obj_set_style_radius(wifiPanel, 10, 0);
  lv_obj_set_style_pad_all(wifiPanel, 0, 0);
  lv_obj_remove_flag(wifiPanel, LV_OBJ_FLAG_SCROLLABLE);

  auto* wifiTitle=makeSettingsText(wifiPanel, "WI-FI", 16, 44, 70, false);
  wifiPowerSwitch = lv_switch_create(wifiPanel);
  styleSettingsSwitch(wifiPowerSwitch);
  lv_obj_set_pos(wifiPowerSwitch, 94, 33);
  if (Tab5Audio.wifiEnabled()) lv_obj_add_state(wifiPowerSwitch, LV_STATE_CHECKED);
  lv_obj_add_event_cb(wifiPowerSwitch, wifiPowerEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);

  wifiSettingsStatus = makeSettingsText(wifiPanel, "", 272, 44, 250, true);
  lv_label_set_long_mode(wifiSettingsStatus, LV_LABEL_LONG_DOT);

  lv_obj_t* wifiConfigure = makeIconButton(wifiPanel, LV_SYMBOL_SETTINGS, 58, 44);
  lv_obj_set_pos(wifiConfigure, 194, 29);
  lv_obj_add_event_cb(wifiConfigure, wifiSettingsEvent,
                      LV_EVENT_CLICKED, nullptr);

  auto* diagnostics=makeButton(systemCard,"Informations système",286,36);
  lv_obj_set_pos(diagnostics,792,4);
  lv_obj_add_event_cb(diagnostics,[](lv_event_t*){showSystemInfo();},LV_EVENT_CLICKED,nullptr);
  auto* latencyCaption=makeSettingsText(systemCard,"Mesurer les latences",20,210,250,false);
  latencySwitch=lv_switch_create(systemCard);
  styleSettingsSwitch(latencySwitch);
  if(Tab5UiDiagnostics::enabled())lv_obj_add_state(latencySwitch,LV_STATE_CHECKED);
  lv_obj_add_event_cb(latencySwitch,[](lv_event_t* e){
    auto* toggle=static_cast<lv_obj_t*>(lv_event_get_target(e));
    const bool ok=Tab5UiDiagnostics::enable(lv_obj_has_state(toggle,LV_STATE_CHECKED));
    if(Tab5UiDiagnostics::enabled())lv_obj_add_state(toggle,LV_STATE_CHECKED);
    else lv_obj_remove_state(toggle,LV_STATE_CHECKED);
    if(!ok)toast("Mémoire insuffisante");
  },LV_EVENT_VALUE_CHANGED,nullptr);
  auto* help=makeButton(systemCard,"?",44,36);
  lv_obj_set_pos(help,1098,4);
  Tab5Focus.describe(help,T("Aide aux raccourcis · F1"));
  lv_obj_add_event_cb(help,[](lv_event_t*){showKeyboardHelp();},LV_EVENT_CLICKED,nullptr);
  refreshWifiSettingsUi(true);

  String languageOptions;
  for (size_t i = 0; i < Tab5Locale.count(); ++i) {
    if (i) languageOptions += '\n';
    languageOptions += Tab5Locale.name(i);
  }
  auto* regionLangCaption=makeSettingsText(regionCard, "Langue", 20, 64, 210, false);
  languageDropdown = lv_dropdown_create(regionCard);
  lv_obj_set_size(languageDropdown, 530, 46);
  lv_obj_set_pos(languageDropdown, 250, 52);
  lv_dropdown_set_options(languageDropdown, languageOptions.c_str());
  lv_dropdown_set_selected(languageDropdown, Tab5Locale.selectedIndex());
  styleSettingsDropdown(languageDropdown);
  lv_obj_add_event_cb(languageDropdown,[](lv_event_t*){
    Tab5Locale.discoverLanguages();String options;
    for(size_t i=0;i<Tab5Locale.count();++i){if(i)options+='\n';options+=Tab5Locale.name(i);}
    lv_dropdown_set_options(languageDropdown,options.c_str());lv_dropdown_set_selected(languageDropdown,Tab5Locale.selectedIndex());
    Tab5UiStyle::control(languageDropdown);
  },LV_EVENT_READY,nullptr);
  lv_obj_add_event_cb(languageDropdown, languageEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_t* regionFallback=nullptr;
  if(Tab5Locale.fontFallbackActive())
    regionFallback=makeSettingsText(regionCard,"English is temporary: install the UI font and restart.",800,52,310,false);

  String keyboardOptions;
  for (size_t i = 0; i < Tab5Keyboards.count(); ++i) {
    if (i) keyboardOptions += '\n';
    keyboardOptions += Tab5Keyboards.name(i);
  }
  auto* regionKeyboardCaption=makeSettingsText(regionCard, "Clavier", 20, 128, 210, false);
  keyboardLayoutDropdown = lv_dropdown_create(regionCard);
  lv_obj_set_size(keyboardLayoutDropdown, 530, 46);
  lv_obj_set_pos(keyboardLayoutDropdown, 250, 116);
  lv_dropdown_set_options(keyboardLayoutDropdown, keyboardOptions.c_str());
  lv_dropdown_set_selected(keyboardLayoutDropdown,
                           Tab5Keyboards.selectedIndex());
  styleSettingsDropdown(keyboardLayoutDropdown);
  lv_obj_add_event_cb(keyboardLayoutDropdown,[](lv_event_t*){
    Tab5Keyboards.discoverLayouts();String options;
    for(size_t i=0;i<Tab5Keyboards.count();++i){if(i)options+='\n';options+=Tab5Keyboards.name(i);}
    lv_dropdown_set_options(keyboardLayoutDropdown,options.c_str());lv_dropdown_set_selected(keyboardLayoutDropdown,Tab5Keyboards.selectedIndex());
    Tab5UiStyle::control(keyboardLayoutDropdown);
  },LV_EVENT_READY,nullptr);
  lv_obj_add_event_cb(keyboardLayoutDropdown, keyboardLayoutEvent,
                      LV_EVENT_VALUE_CHANGED, nullptr);

  auto* regionDocsCaption=makeSettingsText(regionCard, "Dossier Documents", 20, 192, 210, false);
  documentsRootButton=makeButton(regionCard,settings.documentsRoot.c_str(),530,46);
  lv_obj_set_pos(documentsRootButton,250,180);auto* rootLabel=lv_obj_get_child(documentsRootButton,0);
  lv_obj_set_width(rootLabel,494);lv_label_set_long_mode(rootLabel,LV_LABEL_LONG_DOT);
  Tab5Focus.describe(documentsRootButton,T("Choisir ce dossier"));
  lv_obj_add_event_cb(documentsRootButton,documentsRootEvent,LV_EVENT_CLICKED,nullptr);
  // Compact, translated-content geometry. Every original event handler and
  // widget survives; only containers and spacing change. Flex rows follow a
  // dock resize without rebuilding values, selection, focus or live services.
  {
    namespace L=Tab5SettingsLayout;
    for(unsigned i=0;i<3;++i)L::panel(settingsSectionPanels[i]);
    L::row(displayCard,{{displayBrightnessCaption,142},{brightnessSlider,0,1},{brightnessValue,55}});
    L::row(displayCard,{{fontOptions,0,1}});
    L::row(displayCard,{{displayThemeCaption,100},{themeDropdown,0,1},{themeEdit,112}});
    L::row(displayCard,{{displayRadiusCaption,100},{radius,0,1},{radiusValue,44}});
    L::row(displayCard,{{homeOptions,0,1}});
    L::row(displayCard,{{orientationCaption,0,1},{orientationSwitch,74}});
    L::row(editCard,{{editAutosaveCaption,0,1},{autosaveSwitch,74}});
    L::row(editCard,{{editDelayCaption,240},{autosaveDelaySlider,0,1},{autosaveDelayValue,70}});
    L::row(editCard,{{keysButton,0,1}});
    L::row(editCard,{{secretsButton,0,1}});

    L::row(mouseCard,{{mouseSensitivityCaption,180},{sensitivity,0,1},{speedLabel,88}});
    L::row(mouseCard,{{mouseScrollCaption,180},{scrollSpeed,0,1},{scrollLabel,88}});
    L::row(mouseCard,{{mousePrimaryCaption,0,1},{handed,74}});
    L::row(mouseCard,{{mouseNaturalCaption,0,1},{natural,74}});
    auto* gamepadCard=lv_obj_get_parent(settingsPadLabel);
    lv_obj_set_width(settingsPadLabel,lv_pct(100));lv_obj_set_height(settingsPadLabel,LV_SIZE_CONTENT);
    L::place(gamepadCard,{settingsPadConfigure,lv_pct(100)});L::place(gamepadCard,{settingsPadCancel,lv_pct(100)});

    L::row(systemCard,{{clockTitle,120},{rtcValue,0,1},{timeSetButton,48}});
    L::row(systemCard,{{clockManualCaption,84},{ntpSwitch,74},{clockNtpCaption,48}});
    L::row(systemCard,{{timeZoneDropdown,0,1}});
    L::row(systemCard,{{diagnostics,0,1},{help,48}});
    L::row(systemCard,{{latencyCaption,0,1},{latencySwitch,74}});
    auto* networkCard=makeSettingsCard(settingsSectionPanels[1],0,0,cardW,160,"Connexions");
    L::row(networkCard,{{wifiTitle,0,1},{wifiPowerSwitch,74},{wifiConfigure,48}});
    L::row(networkCard,{{wifiSettingsStatus,0,1}});
    auto* meshSettings=makeButton(networkCard,"Meshtastic",300,48);
    lv_obj_add_event_cb(meshSettings,[](lv_event_t*){setKeyboardVisible(false);Tab5Meshtastic::openSettings(settingsPage);},LV_EVENT_CLICKED,nullptr);
    L::row(networkCard,{{meshSettings,0,1}});L::action(meshSettings);
    lv_obj_delete(clockPanel);lv_obj_delete(wifiPanel);
    L::row(energyCard,{{energySleepCaption,0,1},{sleepChoice,210}});
    L::row(energyCard,{{energyWifiCaption,0,1},{wifiSave,74}});
    L::row(energyCard,{{energyBatteryCaption,0,1},{batterySwitch,74}});
    auto* shareActions=L::row(shareCard,{{fileShareLanButton,0,1},{fileShareDirectButton,0,1}});
    lv_obj_move_to_index(shareActions,1);L::place(shareCard,{fileShareStopButton,lv_pct(100)});
    lv_obj_set_width(fileShareStatusLabel,lv_pct(100));
    L::columns(settingsSectionPanels[0],{displayCard,gamepadCard},{editCard,mouseCard});
    auto* serialCard=createSerialSettings(settingsSectionPanels[1]);
    L::columns(settingsSectionPanels[1],{systemCard,energyCard,shareCard},{networkCard,serialCard});
    L::card(networkCard);

    auto* languageRow=L::row(regionCard,{{regionLangCaption,200},{languageDropdown,0,1}});
    if(regionFallback)L::place(languageRow,{regionFallback,300});
    L::row(regionCard,{{regionKeyboardCaption,200},{keyboardLayoutDropdown,0,1}});
    L::row(regionCard,{{regionDocsCaption,200},{documentsRootButton,0,1}});
    lv_obj_set_width(rootLabel,lv_pct(100));
    for(auto* card:{displayCard,editCard,mouseCard,gamepadCard,systemCard,energyCard,shareCard,regionCard})L::card(card);
    for(auto* slider:{brightnessSlider,autosaveDelaySlider,radius,sensitivity,scrollSpeed})L::slider(slider);
    for(auto* action:{fontOptions,themeEdit,homeOptions,keysButton,secretsButton,timeSetButton,wifiConfigure,diagnostics,help,
                     sleepChoice,themeDropdown,timeZoneDropdown,languageDropdown,keyboardLayoutDropdown,documentsRootButton,
                     fileShareLanButton,fileShareDirectButton,fileShareStopButton,settingsPadConfigure,settingsPadCancel})L::action(action);
  }
  settingsStorageMounted=false;
  Tab5UiStyle::tree(settingsPage);
  showSettingsSection(activeSettingsSection);
}





static void createKeyboard() {
  keyboardPanel = lv_obj_create(root);
  lv_obj_set_size(keyboardPanel, WORK_W, keyboardHeight());Tab5Shell::width(keyboardPanel,0);
  lv_obj_align(keyboardPanel, LV_ALIGN_BOTTOM_RIGHT, 0, -1);
  lv_obj_set_style_pad_all(keyboardPanel, 2, 0);
  lv_obj_set_style_pad_row(keyboardPanel, 1, 0);
  lv_obj_set_style_border_width(keyboardPanel, 1, 0);
  lv_obj_set_style_border_side(keyboardPanel, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_color(keyboardPanel, C_BORDER(), 0);Tab5ThemeLive::bind(keyboardPanel,LV_STYLE_BORDER_COLOR,0,3);
  lv_obj_set_style_radius(keyboardPanel, 0, 0);
  lv_obj_set_style_bg_opa(keyboardPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(keyboardPanel, C_SURFACE(), 0);Tab5ThemeLive::bind(keyboardPanel,LV_STYLE_BG_COLOR,0,1);
  lv_obj_set_flex_flow(keyboardPanel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(keyboardPanel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_remove_flag(keyboardPanel, LV_OBJ_FLAG_SCROLLABLE);
  rebuildKeyboard();
  lv_obj_add_flag(keyboardPanel, LV_OBJ_FLAG_HIDDEN);
}

static void createUi() {
  sidebarItems=sidebarAdd=sidebarCountLabel=nullptr;sidebarPicking=sidebarRenderedPicker=false;
  Tab5Shell::load();WORK_W=SCREEN_W-Tab5Shell::choices.dockWidth();MAP_VIEW_W=WORK_W-16;Tab5Shell::workspaceWidth=WORK_W;
  root = lv_screen_active();
  lv_obj_set_style_pad_all(root, 0, 0);
  lv_obj_set_style_bg_color(root, C_BG(), 0);Tab5ThemeLive::bind(root,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(root, C_TEXT(), 0);Tab5ThemeLive::bind(root,LV_STYLE_TEXT_COLOR,0,4);
  lv_obj_set_style_text_font(root, uiFont(), 0);
  lv_obj_set_style_text_letter_space(root, 0, 0);
#if LVGL_VERSION_MAJOR > 9 || (LVGL_VERSION_MAJOR == 9 && LVGL_VERSION_MINOR >= 4)
  lv_obj_set_style_text_outline_stroke_width(root, 0, 0);
#endif
  lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);

  createTopBar();
  createDock();

  pageHost = lv_obj_create(root);
  lv_obj_set_size(pageHost, WORK_W, PAGE_H);Tab5Shell::width(pageHost,0);
  lv_obj_set_pos(pageHost, SCREEN_W-WORK_W, TOP_H);
  lv_obj_set_style_border_width(pageHost, 0, 0);
  lv_obj_set_style_radius(pageHost, 0, 0);
  lv_obj_set_style_pad_all(pageHost, 0, 0);
  lv_obj_set_style_bg_color(pageHost, C_BG(), 0);Tab5ThemeLive::bind(pageHost,LV_STYLE_BG_COLOR,0,0);
  lv_obj_set_style_bg_opa(pageHost, LV_OPA_COVER, 0);
  lv_obj_remove_flag(pageHost, LV_OBJ_FLAG_SCROLLABLE);

  Tab5ToolsHooks toolsHooks;
  Tab5PythonUI::setLanguage(Tab5Locale.effectiveId().c_str());
  toolsHooks.translate=[](const char* text){return T(text);};
  toolsHooks.font=[](){return uiFont();};
  toolsHooks.richFont=richFontFor;
  toolsHooks.imageLoad=[](const char* path,int w,int h,lv_image_dsc_t* d,void** handle)->bool {
    File input=tab5FS.open(path,FILE_READ);int iw=0,ih=0;
    bool safe=input&&!input.isDirectory()&&input.size()<=4*1024*1024&&sniffImageFile(input,iw,ih)&&iw>0&&ih>0&&iw<=8192&&ih<=8192&&static_cast<uint64_t>(iw)*ih<=16000000;
    input.close();if(!safe)return false;M5Canvas* canvas=nullptr;String error;
    if(!loadSdImageCanvas(String(path),w,h,canvas,*d,iw,ih,error))return false;*handle=canvas;return true;
  };
  toolsHooks.imageFree=[](void* handle){auto* canvas=static_cast<M5Canvas*>(handle);canvas->deleteSprite();delete canvas;};
  toolsHooks.color=[](unsigned role)->uint32_t {const auto& p=currentTheme();switch(role){case 0:return p.bg;case 1:return p.raised;case 2:return p.text;case 3:return p.muted;default:return p.active;}};
  toolsHooks.keyboard=[](bool on,lv_obj_t* target){if(target){keyboardUiNavigation=false;keyboardDockFocus=false;}setKeyboardVisible(on,target);};
  toolsHooks.themePalette=[](uint32_t* colors){copyThemePalette(currentTheme(),colors);};
  toolsHooks.themeCount=[]()->size_t{return THEME_COUNT;};
  toolsHooks.themeName=[](size_t i)->const char*{return i<THEME_COUNT?THEMES[i].name:"";};
  toolsHooks.themeId=[](size_t i)->const char*{return i>=5&&i<THEME_COUNT?customThemeIds[i-5]:"";};
  toolsHooks.themeRead=[](size_t i,uint32_t* colors)->bool{if(i>=THEME_COUNT||!colors)return false;copyThemePalette(THEMES[i],colors);return true;};
  toolsHooks.themeReload=[](){reloadThemeCatalogue();};
  toolsHooks.themeApply=[](const char* id){for(unsigned i=5;i<THEME_COUNT;++i)if(!strcmp(id,customThemeIds[i-5])){settings.themeIndex=i;refreshThemeChoices();applySelectedTheme();break;}};
  toolsHooks.themesChanged=[](){reloadThemeCatalogue();applySelectedTheme();};
  toolsHooks.browse=[](const char* directory){const String path=directory;showPage(Page::Files);fileActiveRight=true;clearFilePreview();openFileContentDirectory(path);fileRequestTreeReveal(path);};
  toolsHooks.sdMounted=[](){return sdReady;};
  toolsHooks.sdAction=storageSdAction;
  toolsHooks.editSource=[](const char* path){
    setKeyboardVisible(false);
    if(path&&*path){if(Tab5Writing.open(path))showPage(Page::Writing);}
    else if(Tab5Writing.development())showPage(Page::Writing);
    else if(Tab5Writing.createCode("/MicroPython"))showPage(Page::Writing);
  };
  Tab5NativeView::hooks([](const char* text){return T(text);},[](){return uiFont();},[](){return Tab5Audio.volume();},[](uint8_t volume){settings.audioVolume=volume;Tab5Audio.setVolume(volume);});
  toolsHooks.runExecutable=[](const char* path,const char* argument){return tab5ExecutableRun(path,argument,Tab5Locale.effectiveId().c_str());};
  toolsHooks.openNativeFile=[](Tab5NativeFileApp app,const char* path){
    if(!path||!*path)return;const String target(path);setKeyboardVisible(false);
    switch(app){
      case Tab5NativeFileApp::TextEditor:loadDocumentFrom(target,true);break;
      case Tab5NativeFileApp::Reader:startReader(target);break;
      case Tab5NativeFileApp::ImageViewer:startImageViewer(target);break;
      case Tab5NativeFileApp::Drawing:openImageEditor(path);break;
      case Tab5NativeFileApp::Audio:openAudioPath(target);break;
      case Tab5NativeFileApp::Video:startVideoViewerSource(target,baseNameOf(target));break;
      case Tab5NativeFileApp::Dos:Tab5Tools.launchDosProgram(path);break;
      default:break;
    }
  };
  toolsHooks.beginMapPrint=[](unsigned paper,unsigned dpi,bool landscape,unsigned percent){if(!sdReady||activePage!=Page::Map||mapJpegPreparing||mapJpegEncoding)return false;++mapGeneration;Tab5Map.beginGeneration(mapGeneration);mapRasterIndex=0;return Tab5MapPrint::begin(mapWorldX(mapLongitude,mapZoom),mapWorldY(mapLatitude,mapZoom),mapZoom,Tab5Map.styleSourceZoom(Tab5Map.selectedStyle(),MAP_MAX_ZOOM),mapGeneration,paper,dpi,landscape,Tab5Map.attribution(),percent);};
  Tab5Tools.begin(pageHost,toolsHooks);
  Tab5Meshtastic::Hooks meshHooks;meshHooks.translate=toolsHooks.translate;meshHooks.font=toolsHooks.font;meshHooks.color=toolsHooks.color;
  meshHooks.keyboard=[](lv_obj_t* target,bool explicitRequest){setKeyboardVisible(explicitRequest||!writing::usbConnected(),target);};
  meshHooks.closeKeyboard=[](){setKeyboardVisible(false);};
  meshHooks.storageAvailable=[](){return sdReady;};
  meshHooks.openMap=[](uint32_t node,double latitude,double longitude){
    if(!sdReady||!Tab5Map.ready()){toast(sdReady?"Moteur cartographique indisponible":"Carte SD indisponible — Cartes non ouverte",3200);return;}
    setKeyboardVisible(false);showPage(Page::Map);if(activePage!=Page::Map)return;
    auto options=Tab5Meshtastic::options();options.showOnMap=true;Tab5Meshtastic::setOptions(options);
    mapLatitude=latitude;mapLongitude=longitude;mapZoom=15;
    Tab5MapSearchItem point;point.node=node;point.latitude=latitude;point.longitude=longitude;point.kind=Tab5MapMarkerKind::Mesh;
    snprintf(point.name,sizeof(point.name),"!%08lx",static_cast<unsigned long>(node));
    Tab5Map.selectSearchMarker(point);markMapViewDirty();mapOpenPending=true;
  };
  Tab5Meshtastic::begin(meshHooks);
  Tab5ImageEditor::Hooks imageHooks;imageHooks.translate=toolsHooks.translate;imageHooks.font=toolsHooks.font;imageHooks.keyboard=toolsHooks.keyboard;
  imageHooks.closed=[](){if(activePage==Page::ImageEditor)showPage(Page::Files);};
  Tab5ImageEditor::begin(pageHost,imageHooks);
  Tab5VaultPrompt::Hooks vaultPromptHooks;vaultPromptHooks.translate=toolsHooks.translate;vaultPromptHooks.font=toolsHooks.font;vaultPromptHooks.color=toolsHooks.color;vaultPromptHooks.keyboard=toolsHooks.keyboard;
  Tab5VaultPrompt::begin(vaultPromptHooks);
  Tab5Alarm::Hooks alarmHooks;
  alarmHooks.translate=toolsHooks.translate;alarmHooks.font=toolsHooks.font;alarmHooks.color=toolsHooks.color;
  alarmHooks.wake=[](){screenIdle.wake(millis());};
  alarmHooks.quiet=[](){
    setKeyboardVisible(false);
    if(videoViewerActive)stopVideoViewer();
    if(tab5ExecutableBusy())tab5ExecutableStop();
    Tab5AudioSnapshot snapshot;if(Tab5Audio.snapshot(snapshot)&&snapshot.recording)Tab5Audio.stopRecording();
  };
  alarmHooks.prepareSleep=[]()->const char* {
    if(Tab5FileOperation::busy()||Tab5ImageEditor::busy()||Tab5Python::running()||tab5ExecutableBusy()||Tab5Terminal::busy()||Tab5Sdr::busy()||Tab5Meshtastic::busy()||videoViewerActive||Tab5Firmware::busy()||Tab5Print::busy()||Tab5Print::estimating()||Tab5MapPrint::busy()||Tab5Jpeg::busy()||Tab5Vault::busy()||Tab5KeyStore::busy()||tab5FileShareActive()||tab5FileShareBusy())return "Arrêtez les applications et les transferts avant la veille prolongée";
    Tab5AudioSnapshot snapshot;
    if(!Tab5Audio.snapshot(snapshot)||snapshot.recording||snapshot.playing||snapshot.paused||!Tab5Audio.storageIdle()||Tab5AudioDevice.owner()!=Tab5AudioClient::None)return "Arrêtez la lecture ou l’enregistrement avant la veille prolongée";
    if(Tab5UsbStorage::mounted()||Tab5UsbStorage::pending())return "Éjectez le stockage USB avant la veille prolongée";
    if(documentDirty||Tab5ImageEditor::dirty()||!Tab5Writing.prepareSleep())return "Enregistrez les documents et attendez la fin des écritures";
    if(settingsDirty){if(!saveSettings())return "Enregistrement des réglages impossible";settingsDirty=false;}
    if(readerProgressDirty){saveReaderProgressNow();if(readerProgressDirty)return "Enregistrement de la lecture impossible";}
    audioRememberScroll();audioRememberBrowser();
    if(!Tab5MediaSession::flush(millis()))return Tab5MediaSession::saving()
        ? "Opération en cours : patienter ou annuler" : "Enregistrement des réglages impossible";
    if(snapshot.wifiEnabled){Tab5Audio.setWifiEnabled(false);return "Arrêt du Wi-Fi en cours : réessayez dans un instant";}
    Tab5AppCatalogue::endScan();homeAppsScanning=false;homeAppsDirty=true;fileTree.cancel();fileSearcher.cancel();fileListDir.close();cancelFolderPreview();Tab5Map.releaseStorageHandles();
    return nullptr;
  };
  Tab5Alarm::begin(alarmHooks);
  Tab5PythonReader::Hooks pythonReaderHooks;
  pythonReaderHooks.translate=toolsHooks.translate;
  pythonReaderHooks.color=toolsHooks.color;
  pythonReaderHooks.uiFont=toolsHooks.font;
  pythonReaderHooks.font=[](unsigned pixels)->const lv_font_t* {
#if LV_USE_TINY_TTF
    if(ensureReaderFont()&&readerTtfFont){resizeConfiguredFont(readerTtfFont,readerStockFont,pixels);return readerTtfFont;}
#endif
    if(pixels<=20)return &lv_font_montserrat_20;
    if(pixels<=22)return &lv_font_montserrat_22;
    if(pixels<=24)return &lv_font_montserrat_24;
    if(pixels<=28)return &lv_font_montserrat_28;
    return &lv_font_montserrat_32;
  };
  pythonReaderHooks.enterPortrait=[](){
    setKeyboardVisible(false);Tab5Focus.clear();
    M5.Display.setRotation(Tab5DisplayOrientation::rotation(true, settings.screenUpsideDown));
    lv_display_set_resolution(lvDisplay,M5.Display.width(),M5.Display.height());
  };
  pythonReaderHooks.leavePortrait=[](){
    Tab5Focus.clear();M5.Display.setRotation(Tab5DisplayOrientation::rotation(false, settings.screenUpsideDown));
    lv_display_set_resolution(lvDisplay,SCREEN_W,SCREEN_H);lv_screen_load(root);lv_obj_invalidate(root);
  };
  pythonReaderHooks.background=[](){showPage(Page::Home);showPythonLibrary(true);};
  Tab5PythonReader::begin(pythonReaderHooks);

  createHomePage();
  createFilePage();
  createEditorPage();
  WritingHooks writingHooks;
  writingHooks.imageLoad=[](const char* path,int w,int h,lv_image_dsc_t* d,void** handle)->bool {
    File input=tab5FS.open(path,FILE_READ);int iw=0,ih=0;
    bool safe=input && !input.isDirectory() && input.size()<=4*1024*1024 && sniffImageFile(input,iw,ih) && iw>0 && ih>0 && iw<=8192 && ih<=8192 && static_cast<uint64_t>(iw)*ih<=16000000;
    if(input)input.close();if(!safe)return false;
    M5Canvas* canvas=nullptr;String error;
    if(!loadSdImageCanvas(String(path),w,h,canvas,*d,iw,ih,error))return false;
    *handle=canvas;return true;
  };
  writingHooks.imageFree=[](void* handle){auto* canvas=static_cast<M5Canvas*>(handle);canvas->deleteSprite();delete canvas;};
  writingHooks.keyboard = [](bool on, lv_obj_t* target) { setKeyboardVisible(on, target); };
  writingHooks.keyboardOnFocus = []() { return !writing::usbConnected(); };
  writingHooks.notice = [](const char* message) { toast(message, 3000); };
  writingHooks.font = richFontFor;
  writingHooks.translate = [](const char* text) { return T(text); };
  writingHooks.help = []() { showKeyboardHelp(); };
  writingHooks.print = [](const char* path) { Tab5Writing.leaveEditing();setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Print,path); };
  writingHooks.run=[](const char* path){
    if(Tab5Python::runFile(path))return true;
    toast(Tab5Python::status(),4000);return false;
  };
  writingHooks.stop=[](){Tab5PythonReader::close();Tab5PythonUI::close();Tab5Python::stop();};
  writingHooks.resume=[](){resumePythonApp();};
  writingHooks.libraries=[](){setKeyboardVisible(false);Tab5Tools.openPythonApp(tab5FS.exists("/MicroPython/libraries.py")?"/MicroPython/libraries.py":"/MicroPython/bibliotheques.py");};
  writingHooks.console=[](char* destination,size_t capacity){return Tab5Python::copyOutput(destination,capacity);};
  writingHooks.clearConsole=[](){Tab5Python::clearOutput();};
  writingHooks.running=[](){return Tab5Python::running();};
  writingHooks.color = [](WritingColor role) -> uint32_t {
    const auto& p = currentTheme();
    switch (role) {
      case WritingColor::Background: return p.bg;
      case WritingColor::Surface: return p.surface;
      case WritingColor::Text: return p.text;
      case WritingColor::Muted: return p.muted;
      case WritingColor::Accent: return p.cursor;
      case WritingColor::Selection: return p.selected;
      case WritingColor::Reference: return p.raised;
    }
    return p.bg;
  };
  Tab5Writing.begin(pageHost, writingHooks);
  createPreviewPage();
  createAudioPage();
  createMapPage();
  createSettingsPage();
  createKeyboard();
  Tab5Focus.begin(lv_layer_top(),uiFont());
  Tab5Focus.inputChanged([](lv_obj_t* target) {
    if (keyboardTarget == target) return;
    const bool navigation=keyboardUiNavigation, dockFocus=keyboardDockFocus;
    setKeyboardVisible(keyboardVisible,target);
    keyboardUiNavigation=navigation;keyboardDockFocus=dockFocus;
  });
  Tab5Focus.theme(C_SELECTED(), C_TEXT(), C_SURFACE());

  applyEditorFont();
  updateDocumentTitle();
  updateEditorStatus(true);
  savedTextHash = hashText(lv_textarea_get_text(editor));
  resetHistory();
  updateClockLabels(true);
  showPage(Page::Home);
}

// -----------------------------------------------------------------------------
// Arduino setup / loop
// -----------------------------------------------------------------------------

const char* resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "mise sous tension";
    case ESP_RST_EXT: return "reset externe";
    case ESP_RST_SW: return "logiciel";
    case ESP_RST_PANIC: return "panic/exceptions";
    case ESP_RST_INT_WDT: return "watchdog interruption";
    case ESP_RST_TASK_WDT: return "watchdog tâche";
    case ESP_RST_WDT: return "watchdog général";
    case ESP_RST_DEEPSLEEP: return "sortie veille profonde";
    case ESP_RST_BROWNOUT: return "baisse de tension";
    case ESP_RST_SDIO: return "SDIO";
    default: return "inconnue";
  }
}

static void printDisplayDiagnostics() {
  tab5DisplayPresentDumpConfiguration([](const char* bytes, size_t count, void*) {
    Tab5DebugSerial.write(reinterpret_cast<const uint8_t*>(bytes), count);
  });
  const auto present = tab5DisplayPresentStats();
  const auto signals = tab5DisplaySignalsStats();
  const auto activity = Tab5DisplayActivity::snapshot();
  const auto persistence = Tab5MediaSession::snapshot();
  Tab5DebugSerial.printf("[DISPLAY transfer] nativeRects=%lu nativePixels=%llu maxBlitUs=%lu maxRefreshGapUs=%lu\n",
      static_cast<unsigned long>(present.nativeBlitRects), static_cast<unsigned long long>(present.nativeBlitPixels),
      static_cast<unsigned long>(present.maxNativeBlitUs), static_cast<unsigned long>(present.maxRefreshGapUs));
  Tab5DebugSerial.printf("[MEDIA PREFS] started=%lu completed=%lu failed=%lu deferred=%lu pending=%u lastUs=%lu maxUs=%lu intervalUs=%llu..%llu dsiObserved=%u underrunsDuringWrites=%lu\n",
      static_cast<unsigned long>(persistence.started), static_cast<unsigned long>(persistence.completed),
      static_cast<unsigned long>(persistence.failed), static_cast<unsigned long>(persistence.deferred),
      unsigned(Tab5MediaSession::saving()), static_cast<unsigned long>(persistence.lastWriteUs),
      static_cast<unsigned long>(persistence.maxWriteUs), static_cast<unsigned long long>(persistence.lastBeginUs),
      static_cast<unsigned long long>(persistence.lastEndUs), unsigned(persistence.dsiObserved),
      static_cast<unsigned long>(persistence.underrunsDuringWrites));
  Tab5DebugSerial.printf("[DISPLAY] ms=%lu page=%u active=%u scans=%lu submissions=%lu flips=%lu pending=%u timeouts=%lu copyErrors=%lu publishErrors=%lu lastError=%ld\n",
      static_cast<unsigned long>(millis()), unsigned(activePage), unsigned(present.active),
      static_cast<unsigned long>(present.refreshEvents), static_cast<unsigned long>(present.submissions),
      static_cast<unsigned long>(present.flips), unsigned(present.pending),
      static_cast<unsigned long>(present.timeouts), static_cast<unsigned long>(present.copyErrors),
      static_cast<unsigned long>(present.publishErrors), static_cast<long>(present.lastError));
  Tab5DebugSerial.printf("[DSI] observed=%u irqFlags=0x%lx interrupts=%lu underruns=%lu lastUnderrunUs=%llu status=0x%08lx\n",
      unsigned(signals.active), static_cast<unsigned long>(signals.allocatedFlags),
      static_cast<unsigned long>(signals.interrupts),
      static_cast<unsigned long>(signals.underruns), static_cast<unsigned long long>(signals.lastUnderrunUs),
      static_cast<unsigned long>(signals.lastStatus));
  Tab5DebugSerial.printf("[LVGL] observed=%u frames=%lu areas=%lu pixels=%llu lastFlushMs=%lu largeFrames=%lu lastLargeFrameMs=%lu lastLargeInvalidationMs=%lu source=%s pixels=%lu\n",
      unsigned(activity.attached), static_cast<unsigned long>(activity.frames),
      static_cast<unsigned long>(activity.flushAreas), static_cast<unsigned long long>(activity.flushedPixels),
      static_cast<unsigned long>(activity.lastFlushMs), static_cast<unsigned long>(activity.largeFrames),
      static_cast<unsigned long>(activity.lastLargeFrameMs), static_cast<unsigned long>(activity.lastLargeInvalidationMs),
      Tab5DisplayActivity::sourceName(activity.lastLargeSource), static_cast<unsigned long>(activity.lastLargeInvalidationPixels));
  for (unsigned i = 0; i < Tab5DisplayActivity::sourceCount; ++i) {
    if (activity.invalidations[i])
      Tab5DebugSerial.printf("[LVGL source] %s=%lu\n",
          Tab5DisplayActivity::sourceName(static_cast<Tab5DisplayActivity::Source>(i)),
          static_cast<unsigned long>(activity.invalidations[i]));
  }
}

// Cache callbacks are registered once at boot, after the UI exists.
static void registerResourceCaches() {
  // The OS retires only explicitly reloadable background resources. Each
  // callback still protects live playback, documents, printing and rendering.
  using Tab5Resources::MemoryDomain;
  Tab5MemoryManager.addCache(Tab5Resources::Owner::Writing,"rendu editeur masque",[](void*)->size_t {
    return Tab5Writing.reclaimHiddenLayout();
  },nullptr,{MemoryDomain::Any});
  Tab5MemoryManager.addCache(Tab5Resources::Owner::Writing,"hidden syntax indexes",[](void*)->size_t {
    return Tab5Writing.reclaimHiddenIndexes();
  },nullptr,{MemoryDomain::Psram});
  Tab5MemoryManager.addCache(Tab5Resources::Owner::Video,"video inactive",[](void*)->size_t {
    if(activePage==Page::Audio&&audioView==AudioView::Videos)return 0;
    return Tab5Video.reclaimIdle();
  },nullptr,{MemoryDomain::Any});
  Tab5MemoryManager.addCache(Tab5Resources::Owner::Audio,"catalogues medias",[](void*)->size_t {
    if(activePage==Page::Audio)return 0;
    return Tab5Audio.releaseIdleCatalogues();
  },nullptr,{MemoryDomain::Psram});
  Tab5MemoryManager.addCache(Tab5Resources::Owner::Audio,"tache audio inactive",[](void*)->size_t {
    return Tab5Audio.reclaimIdleWorker();
  },nullptr,{MemoryDomain::Internal});
  Tab5MemoryManager.addCache(Tab5Resources::Owner::Files,"liste fichiers",[](void*)->size_t {
    if(activePage==Page::Files||!fileList)return 0;
    const auto before=tab5MemoryTotals();
    fileTree.releaseMetadata();fileDirectoryModel.clear();fileSearchNeedsRestart=fileContentMode==FileContentMode::Search;fileSearcher.cancel();fileListDir.close();cancelFolderPreview();freeFilePreviewImage();selectedFileBtn=nullptr;lv_obj_clean(fileList);
    const auto after=tab5MemoryTotals();
    return (after.psram>before.psram?after.psram-before.psram:0)+
           (after.internal>before.internal?after.internal-before.internal:0);
  },nullptr,{MemoryDomain::Any});
  Tab5MemoryManager.addCache(Tab5Resources::Owner::Audio,"liste medias",[](void*)->size_t {
    return releaseHiddenMediaList();
  },nullptr,{MemoryDomain::Psram});
  Tab5MemoryManager.addCache(Tab5Resources::Owner::Map,"tuiles rechargeables",[](void*)->size_t {
    if(activePage==Page::Map||Tab5MapPrint::busy()||mapJpegPreparing||mapJpegEncoding)return 0;
    const size_t bytes=Tab5Map.memoryCacheBytes();
    Tab5Map.clearMemoryCache();return bytes;
  },nullptr,{MemoryDomain::Psram});
}

void setup() {
  const uint32_t bootStarted=millis();
  Tab5MemoryManager.begin();
  Tab5DebugSerial.begin(115200);
  delay(20);
  tab5DiagnosticsBegin();
  Tab5UiDiagnostics::begin();
  if(*tab5DiagnosticPrevious())Tab5DebugSerial.printf("[RESET] %s\n",tab5DiagnosticPrevious());
  Tab5DebugSerial.printf("Tab5OS Arduino v%s\n", APP_VERSION);
  Tab5DebugSerial.printf("Arduino-ESP32 %s\n", ESP_ARDUINO_VERSION_STR);
  Tab5DebugSerial.printf("ESP-IDF %s\n", esp_get_idf_version());
  const esp_reset_reason_t resetReason = esp_reset_reason();
  Tab5DebugSerial.printf("Dernier reset: %s (%d)\n", resetReasonName(resetReason),
                static_cast<int>(resetReason));
  tab5NetDiagnosticsBegin();
  tab5NetworkGateBegin();
#if defined(M5UNIFIED_VERSION_MAJOR) && defined(M5UNIFIED_VERSION_MINOR) && \
    defined(M5UNIFIED_VERSION_PATCH)
  Tab5DebugSerial.printf("M5Unified %d.%d.%d (horloge I2S P4 corrigée)\n",
                M5UNIFIED_VERSION_MAJOR, M5UNIFIED_VERSION_MINOR,
                M5UNIFIED_VERSION_PATCH);
#endif

  // Arduino has already initialised PSRAM. Apply the data/TLS allocation
  // policy before M5, USB, Wi-Fi and app services create their persistent data.
  tab5ResourcesBegin();
  Tab5CryptoRandom::begin();
  const bool tlsExternal=tab5TlsMemoryBegin();
  Tab5DebugSerial.printf("[MEM] TLS: %s; objets ordinaires: PSRAM prioritaire\n",
      tlsExternal?"allocateur PSRAM actif":"allocateur interne SDK conserve");
  auto cfg = M5.config();
  M5.begin(cfg);
  Tab5Compatibility::begin();
  M5.Display.setRotation(Tab5DisplayOrientation::rotation(false, settings.screenUpsideDown));
  M5.Display.setBrightness(settings.brightness);
  Tab5DebugSerial.printf("[DISPLAY] synchronisation cache : %s\n",
      tab5DisplayCacheInit()?"zones modifiees uniquement":"plage complete (repli)");
  bootStatus.begin(bootStarted);
  bootStatus.start(Tab5BootStatus::Hardware);
  bootStatus.finish();
  bootStatus.start(Tab5BootStatus::Storage);

  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  Tab5DebugSerial.printf("microSD SPI : CS=%d SCK=%d MOSI=%d MISO=%d @ %lu Hz\n",
                SD_SPI_CS_PIN, SD_SPI_SCK_PIN, SD_SPI_MOSI_PIN,
                SD_SPI_MISO_PIN, static_cast<unsigned long>(SD_SPI_HZ));
  sdReady = SD.begin(SD_SPI_CS_PIN, SPI, SD_SPI_HZ, "/sd", 24);
  // Only during boot, before clients hold open files. No LDO manipulation,
  // no run-time remount under active readers/writers, no formatting.
  if (!sdReady) {
    SD.end();SPI.end();
    pinMode(SD_SPI_CS_PIN,OUTPUT);digitalWrite(SD_SPI_CS_PIN,HIGH);
    SPI.begin(SD_SPI_SCK_PIN,SD_SPI_MISO_PIN,SD_SPI_MOSI_PIN,SD_SPI_CS_PIN);
    Tab5DebugSerial.println("[SD] montage initial echoue, nouvel essai SPI a 10 MHz");
    sdReady = SD.begin(SD_SPI_CS_PIN, SPI, 10000000U, "/sd", 24);
  }
  Tab5DebugSerial.printf("PSRAM: %u Kio total, %u Kio libres\n",
                static_cast<unsigned>(heap_caps_get_total_size(MALLOC_CAP_SPIRAM) / 1024),
                static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
  Tab5DebugSerial.printf("microSD: %s\n", sdReady ? "OK" : "absente");

  tab5SdSetAvailable(sdReady);
  if(sdReady){File rootCheck=tab5FS.open("/",FILE_READ);sdReady=rootCheck&&rootCheck.isDirectory();rootCheck.close();}
  tab5SdSetAvailable(sdReady);
  if(sdReady)sdReady=migrateSystemDirectory();
  tab5SdSetAvailable(sdReady);
  tab5StorageBootResult(sdReady,millis());
  Tab5SystemResources::start(sdReady);
  bootStatus.finish(sdReady);
  bootStatus.start(Tab5BootStatus::Settings);
  if (sdReady) {
    ensureDirectories();
    if(!loadSettingsSources()){
      systemMigrationFailed=true;sdReady=false;Tab5SystemResources::cancel();
      tab5SdSetAvailable(false);tab5StorageFault("lecture réglages",SETTINGS_FILE,EIO);
    }
    M5.Display.setBrightness(settings.brightness);
    M5.Display.setRotation(Tab5DisplayOrientation::rotation(false,settings.screenUpsideDown));
  }

  Tab5MediaSession::begin(settings.audioVolume,sdReady&&settingsReadComplete);
  settings.audioVolume=Tab5MediaSession::volume();
  audioRestoreBrowser();
  if (!isExistingDirectory(settings.documentsRoot)) settings.documentsRoot = "/";
  currentDir = settings.documentsRoot;
  if(settings.themeIndex>=THEME_COUNT)settings.themeIndex=0;
  selectAppliedTheme();
  Tab5Mouse::palette(appliedTheme.pointer,appliedTheme.pointerOutline);
  // Document folders are browsed on demand; boot never enumerates their tree.
  bootStatus.finish();
  bootStatus.start(Tab5BootStatus::Language);
  const bool localeReady=Tab5Locale.begin(sdReady);
  bootStatus.finish(localeReady);
  bootStatus.start(Tab5BootStatus::Keyboard);
  const bool keyboardReady=Tab5Keyboards.begin(sdReady);
  bootStatus.finish(keyboardReady);
  bootStatus.start(Tab5BootStatus::Usb);
  writing::usbFrench(Tab5Keyboards.selectedId().startsWith("fr"));
  writing::usbMapping(Tab5Keyboards.physical().data(),Tab5Keyboards.physical().size());
  const bool usbReady=writing::usbBegin();  // Global HID host: available from the home screen.
  Tab5DebugSerial.printf("Langue UI: %s | clavier: %s | Documents: %s\n",
                Tab5Locale.selectedId().c_str(),
                Tab5Keyboards.selectedId().c_str(),
                settings.documentsRoot.c_str());

  // M5Unified garde la propriété du codec et de l'I2S du Tab5.
  bootStatus.finish(usbReady);
  bootStatus.start(Tab5BootStatus::Audio);
  const bool audioReady = Tab5Audio.begin(sdReady, settings.audioVolume);
  Tab5DebugSerial.printf("Audio M5Unified: %s\n", audioReady ? "OK" : "échec");
  bootStatus.finish(audioReady);
  bootStatus.start(Tab5BootStatus::Video);
  const BaseType_t uiCore = xPortGetCoreID();
#if defined(CONFIG_FREERTOS_UNICORE) && CONFIG_FREERTOS_UNICORE
  const BaseType_t videoCore = uiCore;
#else
  const BaseType_t videoCore = uiCore == 0 ? 1 : 0;
#endif
  const bool videoReady = Tab5Video.begin(sdReady, videoCore);
  Tab5DebugSerial.printf("Vidéo MJPEG P4: %s\n", videoReady ? "OK" : "échec");
  bootStatus.finish(videoReady);
  bootStatus.start(Tab5BootStatus::Maps);
  const bool mapUiStorageReady = allocateMapUiStorage();
  Tab5DebugSerial.printf("[MAP] états UI en PSRAM: %s\n",
                mapUiStorageReady ? "OK" : "incomplets");
  const bool mapReady = Tab5Map.begin(sdReady, uiCore);
  if (mapReady) restoreMapOpeningView();
  clampMapView();
  Tab5DebugSerial.printf("Cartes: %s | %s | accueil %.5f, %.5f z%u | %u lieux | "
                "%u paquet(s) SD | raster\n",
                mapReady ? "OK" : "échec",
                Tab5Map.styleName(Tab5Map.selectedStyle()).c_str(),
                mapLatitude, mapLongitude,
                static_cast<unsigned>(mapZoom),
                static_cast<unsigned>(Tab5Map.placeCount()),
                static_cast<unsigned>(Tab5Map.offlinePackCount()));

  // Le RTC reste la source du mode manuel. En mode NTP, le fuseau choisi est
  // appliqué et chaque synchronisation Internet actualise aussi le RTC.
  bootStatus.finish(mapUiStorageReady && mapReady);
  bootStatus.start(Tab5BootStatus::Clock);
  initRtc();
  bootStatus.finish();
  bootStatus.start(Tab5BootStatus::Graphics);

  if (!initLvgl()) {
    bootStatus.finish(false);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(20, 20);
    M5.Display.println("Graphics initialization failed");
    while (true) delay(1000);
  }

  bootStatus.finish();
  bootStatus.start(Tab5BootStatus::Fonts);
  smoothFontReady = loadSmoothFonts();
  Tab5Locale.applyFontAvailability(smoothFontReady && Tab5Cjk.availableFor(Tab5Locale.selectedId()));
  Tab5DebugSerial.printf("Police TTF: %s\n", smoothFontReady ? "OK" : "fallback bitmap");

  // Monochrome base theme: guarantees that widgets created later cannot inherit
  // LVGL's colored primary/secondary palette. Our custom greys are applied on top.
#if LV_USE_THEME_MONO
  lv_theme_t* darkTheme = lv_theme_mono_init(lvDisplay, currentTheme().bg < 0x808080, uiFont());
  if (darkTheme) lv_display_set_theme(lvDisplay, darkTheme);
#else
  lv_display_set_theme(lvDisplay, nullptr);
#endif

  bootStatus.finish(smoothFontReady);
  bootStatus.start(Tab5BootStatus::Interface);
  createUi();
  registerResourceCaches();
  bootStatus.finish();
  bootStatus.start(Tab5BootStatus::Integrity);
  const bool heapOk=heap_caps_check_integrity_all(true);
  bootStatus.finish(heapOk);
  if(!heapOk){
    Tab5DebugSerial.println("[BOOT] Memory integrity check failed; startup stopped");
    // Keep the direct diagnostic screen. Do not allocate LVGL objects on a damaged heap.
    while(true)delay(1000);
  }
  bootStatus.ready();
  Tab5DebugSerial.printf("[DISPLAY] double buffering: %s\n", tab5DisplayPresentInit() ? "active" : "unavailable");
  lv_obj_invalidate(root);
  Tab5DisplayRefresh::service(lvDisplay);
  lv_timer_handler();

  screenIdle.wake(millis());
  Tab5Audio.setWifiPowerSave(settings.wifiPowerSave);
  tab5LogMemory("UI prete");
  Tab5DebugSerial.println("[MEM] envoyer m pour un bilan memoire sans changer l'etat");

  if (systemMigrationFailed) {
    toast("Migration vers System incomplète : relancez la récupération SD",6000);
  } else if (!sdReady) {
    toast("Carte SD non detectee", 4000);
  } else {
    // Do not scan the SD directory during boot; Files refreshes when opened.
    if (!smoothFontReady) toast("Ajoutez /System/font.ttf pour le texte lisse", 5500);
    else if(configuredFontWarning)toast("Police personnalisée indisponible : police standard utilisée",5500);
  }
}

static void serviceScreenEnergy(uint32_t now) {
  bool pressed = M5.Touch.getCount() > 0 && M5.Touch.getDetail(0).isPressed();
  screenWakeTouch = screenIdle.touch(pressed, now);
  const auto level = screenIdle.update(now, SCREEN_TIMEOUTS[settings.screenSleepIndex],
                                      videoViewerActive||Tab5KeyStore::busy());
  Tab5KeyStore::service(level == Tab5ScreenIdle::Level::Off);
  if(level == Tab5ScreenIdle::Level::Off){Tab5Tools.clearSecrets();Tab5VaultPrompt::close();}
  const uint8_t target = level == Tab5ScreenIdle::Level::Off ? 0 :
      (level == Tab5ScreenIdle::Level::Dim ? std::max<uint8_t>(10, settings.brightness / 4) : settings.brightness);
  if (target != appliedBrightness) {
    M5.Display.setBrightness(target);
    appliedBrightness = target;
  }
}

static bool keyboardObjectVisible(lv_obj_t* object) {
  if(!object||!lv_obj_is_valid(object))return false;
  auto* screen=lv_obj_get_screen(object);
  if(screen!=lv_screen_active()&&screen!=lv_layer_top())return false;
  for(auto* p=object;p;p=lv_obj_get_parent(p))if(lv_obj_has_flag(p,LV_OBJ_FLAG_HIDDEN))return false;
  return true;
}
static lv_obj_t* keyboardScope() {
  if(Tab5ContextMenu::scope())return Tab5ContextMenu::scope();
  if(keyboardObjectVisible(keyboardHelpOverlay))return keyboardHelpOverlay;
  if(Tab5VaultPrompt::scope())return Tab5VaultPrompt::scope();
  if(Tab5GitView::modalScope())return Tab5GitView::modalScope();
  if(Tab5ImageEditor::modalScope())return Tab5ImageEditor::modalScope();
  if(keyboardObjectVisible(sidebarSettingsOverlay))return sidebarSettingsOverlay;
  if(Tab5FilePicker::scope())return Tab5FilePicker::scope();
  if(defaultFontPopup)return defaultFontPopup;
  if(fileDeleteOverlay&&lv_obj_is_valid(fileDeleteOverlay))return fileDeleteOverlay;
  if(fileOperationOverlay&&lv_obj_is_valid(fileOperationOverlay))return fileOperationOverlay;
  if(systemInfoOverlay&&lv_obj_is_valid(systemInfoOverlay))return systemInfoOverlay;
  lv_obj_t* overlays[]={keyboardHelpOverlay,dialogOverlay,filePopupOverlay,wifiOverlay,mapExportProgress,mapPlacesDeleteOverlay,mapPlacesOverlay,mapStylesOverlay,mapSearchOverlay,mapDownloadOverlay};
  for(auto* o:overlays)if(keyboardObjectVisible(o))return o;
  if(lv_screen_active()!=root)return lv_screen_active();
  if(Tab5Meshtastic::scope())return Tab5Meshtastic::scope();
  if(Tab5Tools.modalScope())return Tab5Tools.modalScope();
  if(Tab5Tools.scope())return Tab5Tools.scope();
  switch(activePage){case Page::Home:return homePage;case Page::Files:return filesPage;case Page::Editor:return editorPage;
    case Page::Writing:return Tab5Writing.navigationScope();case Page::Preview:return previewPage;case Page::Audio:return audioPage;
    case Page::Map:return mapPage;case Page::Settings:return settingsPage;case Page::ImageEditor:return Tab5ImageEditor::scope();}
  return pageHost;
}
static void showContextMenu(lv_obj_t* hit,int x,int y) {
  if(Tab5ContextMenu::scope()){Tab5ContextMenu::close();return;}
  if(!hit||!lv_obj_is_valid(hit)||Tab5NativeView::visible())return;
  auto* scope=keyboardScope();
  if(!scope||!Tab5Focus.contains(scope,hit))return;
  if(Tab5ContextMenu::text(hit,x,y))return;
  if(activePage==Page::Writing&&Tab5Writing.contextMenu(hit,x,y))return;
  if(activePage==Page::Files&&scope==filesPage)fileContextMenuAt(hit,x,y);
}
static void closeKeyboardHelp() {
  Tab5Focus.clear();
  auto* old = keyboardHelpOverlay;
  keyboardHelpOverlay = keyboardHelpBody = keyboardHelpText = keyboardHelpTitle = nullptr;
  keyboardHelpTabs.clear();
  if (old) lv_obj_delete(old);
  Tab5Focus.restore(keyboardDockFocus ? dock : keyboardScope());
}
static void selectHelpSection(size_t index) {
  if (!keyboardHelpBody || index >= tab5HelpCount()) return;
  lv_label_set_text(keyboardHelpTitle, tab5HelpTitle(index));
  lv_label_set_text(keyboardHelpText, tab5HelpText(index));
  lv_obj_scroll_to_y(keyboardHelpBody, 0, LV_ANIM_OFF);
  for (size_t i = 0; i < keyboardHelpTabs.size(); ++i) {
    if (i == index) lv_obj_add_state(keyboardHelpTabs[i], LV_STATE_CHECKED);
    else lv_obj_remove_state(keyboardHelpTabs[i], LV_STATE_CHECKED);
  }
}
static void showKeyboardHelp() {
  if (keyboardHelpOverlay) { closeKeyboardHelp(); return; }
  if (activePage == Page::Writing) Tab5Writing.leaveEditing();
  setKeyboardVisible(false);
  Tab5Focus.remember(keyboardDockFocus ? dock : keyboardScope());
  keyboardHelpOverlay = lv_obj_create(lv_layer_top());
  lv_obj_add_event_cb(keyboardHelpOverlay, [](lv_event_t* e) {
    if (lv_event_get_target_obj(e) == keyboardHelpOverlay) {
      keyboardHelpOverlay = keyboardHelpBody = keyboardHelpText = keyboardHelpTitle = nullptr;
      keyboardHelpTabs.clear();
    }
  }, LV_EVENT_DELETE, nullptr);
  const int helpWidth = std::min(1160, static_cast<int>(lv_display_get_horizontal_resolution(lvDisplay)) - 32);
  const int helpHeight = std::min(656, static_cast<int>(lv_display_get_vertical_resolution(lvDisplay)) - 32);
  const int sectionWidth = helpWidth >= 1000 ? 278 : 196;
  const int bodyWidth = helpWidth - sectionWidth - 48;
  const int contentHeight = helpHeight - 90;
  lv_obj_set_size(keyboardHelpOverlay, helpWidth, helpHeight);
  lv_obj_center(keyboardHelpOverlay);
  lv_obj_set_style_bg_color(keyboardHelpOverlay, C_SURFACE(), 0);
  lv_obj_set_style_text_font(keyboardHelpOverlay, uiFont(), 0);
  lv_obj_set_style_text_color(keyboardHelpOverlay, C_TEXT(), 0);
  lv_obj_set_style_border_color(keyboardHelpOverlay, C_BORDER(), 0);
  lv_obj_set_style_pad_all(keyboardHelpOverlay, 16, 0);
  lv_obj_remove_flag(keyboardHelpOverlay, LV_OBJ_FLAG_SCROLLABLE);
  auto* heading = lv_label_create(keyboardHelpOverlay);
  lv_label_set_text(heading, T("Aide"));
  auto* close = makeIconButton(keyboardHelpOverlay, LV_SYMBOL_CLOSE, 42, 40);
  lv_obj_align(close, LV_ALIGN_TOP_RIGHT, 0, -6);
  lv_obj_add_event_cb(close, [](lv_event_t*) { closeKeyboardHelp(); }, LV_EVENT_CLICKED, nullptr);
  Tab5Focus.describe(close, T("Fermer l’aide · F1 / Échap"));
  auto* sections = lv_obj_create(keyboardHelpOverlay);
  lv_obj_set_pos(sections, 0, 50);
  lv_obj_set_size(sections, sectionWidth, contentHeight);
  lv_obj_set_style_pad_all(sections, 6, 0);
  lv_obj_set_style_bg_color(sections, C_BG(), 0);
  lv_obj_set_style_border_width(sections, 0, 0);
  lv_obj_set_flex_flow(sections, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(sections, 6, 0);
  for (size_t i = 0; i < tab5HelpCount(); ++i) {
    auto* button = makeButton(sections, tab5HelpTitle(i), sectionWidth - 26, helpWidth >= 1000 ? 54 : 84);
    auto* label = lv_obj_get_child(button, 0);
    lv_obj_set_width(label, sectionWidth - 46);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_font(label, uiFont(), 0);
    lv_obj_update_layout(label);
    lv_obj_set_height(button, std::max(54, static_cast<int>(lv_obj_get_height(label)) + 16));
    lv_obj_add_event_cb(button, [](lv_event_t* e) {
      selectHelpSection(static_cast<size_t>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e))));
    }, LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
    keyboardHelpTabs.push_back(button);
  }
  keyboardHelpBody = lv_obj_create(keyboardHelpOverlay);
  lv_obj_set_pos(keyboardHelpBody, sectionWidth + 14, 50);
  lv_obj_set_size(keyboardHelpBody, bodyWidth, contentHeight);
  lv_obj_set_style_bg_color(keyboardHelpBody, C_BG(), 0);
  lv_obj_set_style_border_width(keyboardHelpBody, 0, 0);
  lv_obj_set_style_pad_all(keyboardHelpBody, 18, 0);
  lv_obj_set_flex_flow(keyboardHelpBody, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(keyboardHelpBody, 18, 0);
  lv_obj_add_flag(keyboardHelpBody, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_USER_1 | LV_OBJ_FLAG_USER_2));
  keyboardHelpTitle = lv_label_create(keyboardHelpBody);
  lv_obj_set_width(keyboardHelpTitle, bodyWidth - 48);
  lv_label_set_long_mode(keyboardHelpTitle, LV_LABEL_LONG_WRAP);
  keyboardHelpText = lv_label_create(keyboardHelpBody);Tab5TextInput::selectable(keyboardHelpText);
  lv_obj_set_width(keyboardHelpText, bodyWidth - 56);
  lv_label_set_long_mode(keyboardHelpText, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_line_space(keyboardHelpText, 5, 0);
  selectHelpSection(activePage == Page::Writing ? 1 : 0);
  Tab5Focus.focusObject(keyboardHelpTabs.empty() ? close : keyboardHelpTabs[0]);
}
static void keyboardToDock() {
  if(activePage==Page::Writing)Tab5Writing.leaveEditing();
  Tab5Focus.remember(keyboardScope());keyboardDockFocus=true;keyboardUiNavigation=true;
  setKeyboardVisible(false);Tab5Focus.restore(Tab5Shell::choices.dockWidth()?dock:topBar);
}
static void keyboardToApp() {
  Tab5Focus.remember(Tab5Shell::choices.dockWidth()?dock:topBar);keyboardDockFocus=false;keyboardUiNavigation=true;
  Tab5Focus.restore(keyboardScope());
}
static bool keyboardDialogScope(lv_obj_t* scope) {
  if(!scope)return false;
  lv_obj_t* dialogs[]={sidebarSettingsOverlay,fileDeleteOverlay,fileOperationOverlay,dialogOverlay,filePopupOverlay,wifiOverlay,mapExportProgress,mapPlacesDeleteOverlay,mapPlacesOverlay,mapStylesOverlay,mapSearchOverlay,mapDownloadOverlay};
  for(auto* dialog:dialogs)if(dialog==scope)return true;
  return false;
}
static void serviceKeyboardFocus() {
  static lv_obj_t* previous=nullptr;
  static bool previousWasModal=false;
  if(!root||!lv_obj_is_valid(root)||lv_screen_active()!=root)return;
  auto* scope=keyboardScope();
  if(scope==previous)return;
  const bool restorePage=previousWasModal;
  previous=scope;
  previousWasModal=keyboardDialogScope(scope)||scope==Tab5VaultPrompt::scope()||scope==Tab5GitView::modalScope()||scope==Tab5Tools.modalScope()||scope==Tab5Meshtastic::scope()||scope==Tab5FilePicker::scope()||scope==keyboardHelpOverlay||scope==systemInfoOverlay||scope==defaultFontPopup;
  if(!keyboardObjectVisible(scope)||Tab5Focus.contains(scope,Tab5Focus.focused()))return;
  if(previousWasModal||scope==Tab5Tools.scope()) {
    keyboardDockFocus=false;keyboardUiNavigation=true;
    Tab5Focus.focusInitial(scope);
  } else if(restorePage) {
    keyboardDockFocus=false;keyboardUiNavigation=true;
    Tab5Focus.restore(scope);
  }
}
static bool dispatchDialogKeyboard(const writing::KeyEvent& e) {
  auto* scope=keyboardScope();
  if(!keyboardDialogScope(scope))return false;
  using K=writing::Key;
  auto* focused=Tab5Focus.focused();
  if(!keyboardUiNavigation&&keyboardObjectVisible(keyboardTarget)&&
     Tab5Focus.contains(scope,keyboardTarget)&&lv_obj_check_type(keyboardTarget,&lv_textarea_class)&&
     !lv_obj_has_state(keyboardTarget,LV_STATE_DISABLED)&&!Tab5TextInput::readOnly(keyboardTarget)) {
    Tab5Focus.editObject(keyboardTarget);
     // Physical input now owns focus. Reapplying the touched target on each
     // key would undo Tab navigation and re-enter editing after every Escape.
     keyboardUiNavigation=true;keyboardDockFocus=false;
   }
  else if(!Tab5Focus.contains(scope,focused))Tab5Focus.focusInitial(scope);
  focused=Tab5Focus.focused();
  bool text=focused&&lv_obj_check_type(focused,&lv_textarea_class);
  const bool back=e.key==K::Escape||(e.key==K::Backspace&&!text&&!Tab5Focus.controlling()&&!e.ctrl&&!e.alt&&!e.meta);
  if(back) {
    // Escape cancels the modal input in one press. An expanded choice closes
    // first, while editing a textarea must not swallow the cancellation.
    if(e.key==K::Escape&&focused&&lv_obj_check_type(focused,&lv_dropdown_class)&&lv_dropdown_is_open(focused)){lv_dropdown_close(focused);Tab5Focus.finishEditing();return true;}
    if(scope==sidebarSettingsOverlay)closeSidebarSettings();
    else if(scope==fileDeleteOverlay)closeFileDeleteDialog();
    else if(scope==fileOperationOverlay)closeFileOperationDialog();
    else if(scope==dialogOverlay)closeTextDialog();
    else if(scope==filePopupOverlay)closeFilePopup();
    else if(scope==wifiOverlay)closeWifiPopup();
    else if(scope==mapPlacesDeleteOverlay)closeMapPlaceDeleteOverlay();
    else if(scope==mapPlacesOverlay)closeMapPlacesEvent(nullptr);
    else if(scope==mapStylesOverlay)closeMapStylesEvent(nullptr);
    else if(scope==mapSearchOverlay)closeMapSearchEvent(nullptr);
    else if(scope==mapDownloadOverlay){auto* old=mapDownloadOverlay;mapDownloadOverlay=nullptr;lv_obj_delete(old);}
    else if(scope==mapExportProgress){if(mapJpegPreparing)Tab5MapPrint::cancel();if(mapJpegEncoding)Tab5Jpeg::cancel();}
    keyboardDockFocus=false;Tab5Focus.restore(keyboardScope());
  } else if(e.key==K::Enter&&!e.ctrl&&!e.meta&&!e.alt&&text&&lv_textarea_get_one_line(focused)&&!lv_obj_has_state(focused,LV_STATE_DISABLED)){
    // READY may delete the input and replace the entire dialog.
    Tab5Focus.finishEditing();lv_obj_send_event(focused,LV_EVENT_READY,nullptr);
  } else Tab5Focus.key(e,scope);
  return true; // No application or desktop shortcut escapes this window.
}
static void dispatchKeyboard(const writing::KeyEvent& original) {
  Tab5Focus.keyboardMode();
  Tab5UiStyle::keyboardMode();
  using K=writing::Key;writing::KeyEvent e=original;
  if(Tab5ContextMenu::key(e))return;
  if(e.key==K::ContextMenu){
    auto* target=Tab5Focus.focused();if(!target)target=keyboardScope();
    if(target&&lv_obj_is_valid(target)){lv_area_t area;lv_obj_get_coords(target,&area);showContextMenu(target,area.x1+std::min(40,int(area.x2-area.x1)),area.y1+std::min(40,int(area.y2-area.y1)));}
    return;
  }
  if((e.ctrl||e.meta)&&!e.shift&&!e.alt&&e.key==K::Text&&(e.text[0]=='l'||e.text[0]=='L')){
    auto* target=Tab5Focus.focused();if(target&&Tab5Focus.contains(keyboardScope(),target)&&Tab5ContextMenu::clearOutput(target))return;
  }
  if(gamepadSettingsKey(e))return;
  auto* imeScope=keyboardScope();
  auto* imeTarget=Tab5Focus.focused();
  const bool writingInput=imeScope==Tab5Writing.object()&&Tab5Writing.editing()&&
      Tab5Writing.isInput(keyboardTarget);
  if(writingInput)imeTarget=keyboardTarget;
  else if(!imeTarget||!lv_obj_check_type(imeTarget,&lv_textarea_class))
    imeTarget=(!keyboardUiNavigation&&keyboardObjectVisible(keyboardTarget))?keyboardTarget:nullptr;
  if(imeTarget&&((!keyboardObjectVisible(imeTarget)&&!writingInput)||!Tab5Focus.contains(imeScope,imeTarget)||
     lv_obj_has_state(imeTarget,LV_STATE_DISABLED)||Tab5TextInput::readOnly(imeTarget)))imeTarget=nullptr;
  if(Tab5Alarm::visible()||Tab5NativeView::visible())imeTarget=nullptr;
  if(imePhysical(e,imeTarget))return;


  if(e.ctrl&&e.alt&&e.key==K::Text&&(e.text[0]=='h'||e.text[0]=='H'))e.key=K::F1;
  if(e.key==K::F1){
    if(keyboardHelpOverlay){closeKeyboardHelp();return;}
    if(Tab5NativeView::visible())Tab5NativeView::key(e);
    else if(Tab5PythonReader::visible())Tab5PythonReader::key(e);
    else showKeyboardHelp();return;
  }
  if(keyboardHelpOverlay){if(e.key==K::Escape||e.key==K::Backspace)closeKeyboardHelp();else if(e.key==K::PageUp||e.key==K::PageDown)lv_obj_scroll_by(keyboardHelpBody,0,e.key==K::PageUp?400:-400,LV_ANIM_OFF);else Tab5Focus.key(e,keyboardHelpOverlay);return;}
  if(Tab5Alarm::visible()){Tab5Alarm::key(e);return;}
  if(Tab5FilePicker::visible()){Tab5FilePicker::key(e);return;}
  if(defaultFontPopup){if(e.key==K::Escape)closeDefaultFontPopup();else Tab5Focus.key(e,defaultFontPopup);return;}
  if(Tab5VaultPrompt::key(e))return;
  if(Tab5GitView::key(e))return;
  if(Tab5NativeView::visible()){Tab5NativeView::key(e);return;}
  if(dispatchDialogKeyboard(e))return;
  if(Tab5Meshtastic::visible()){Tab5Meshtastic::key(e);return;}
  if(Tab5Tools.modalScope()){Tab5Tools.key(e);return;}
  if(Tab5ImageEditor::modalScope()){Tab5ImageEditor::key(e);return;}
  if(activePage==Page::Writing&&Tab5Writing.navigationScope()!=Tab5Writing.object()&&!Tab5Tools.scope()){Tab5Writing.key(e,Tab5Shell::choices.dockWidth()?dock:topBar);return;}
  auto* selectedText=Tab5Focus.focused();
  bool copying=(e.ctrl||e.meta)&&((e.key==K::Text&&(e.text[0]=='c'||e.text[0]=='C'))||e.key==K::Insert);
  if(selectedText&&(!copying||Tab5TextInput::hasSelection(selectedText))&&lv_obj_check_type(selectedText,&lv_label_class)&&Tab5TextInput::key(selectedText,e))return;
  if(systemInfoOverlay){
    if(e.key==K::Escape||e.key==K::Backspace)closeSystemInfo();
    else if(e.key==K::PageUp||e.key==K::PageDown)lv_obj_scroll_by(systemInfoBody,0,e.key==K::PageUp?400:-400,LV_ANIM_OFF);
    else Tab5Focus.key(e,systemInfoOverlay);
    return;
  }
  if(Tab5PythonReader::visible()&&Tab5PythonReader::key(e))return;
  if(e.ctrl&&e.alt&&e.key==K::Text&&(e.text[0]=='p'||e.text[0]=='P')&&lv_screen_active()==root){resumePythonApp();return;}
  bool toggle=e.key==K::Command || e.key==K::F6 || (e.meta&&e.key==K::Text&&e.text[0]=='/');
  if(toggle && lv_screen_active()==root) {
    if(keyboardDockFocus)keyboardToApp();else keyboardToDock();return;
  }
  if(e.alt&&e.key==K::Text&&e.text[0]>='1'&&e.text[0]<='7'&&lv_screen_active()==root) {
    static const Page pages[]={Page::Home,Page::Files,Page::Writing,Page::Preview,Page::Audio,Page::Map,Page::Settings};
    if(e.text[0]=='1')homeOpenMenu(0);else showPage(pages[e.text[0]-'1']);keyboardToApp();return;
  }
  const bool backKey=e.key==K::Escape||(e.key==K::Backspace&&!e.ctrl&&!e.alt&&!e.meta&&!Tab5Focus.controlling());
  if(readerActive && readerSeekSlider && Tab5Focus.focused()==readerSeekSlider && Tab5Focus.controlling()) {
    if(Tab5Focus.key(e,readerScreen))return;
  }
  if(readerActive&&backKey){stopReader();Tab5Focus.clear();return;}
  if(readerActive) {
    if(e.key==K::PageDown || (!Tab5Focus.focused()&&e.key==K::Right)){if(readerIsEpub)readerGoNextPage();else lv_obj_scroll_by(readerContent,0,-400,LV_ANIM_OFF);return;}
    if(e.key==K::PageUp || (!Tab5Focus.focused()&&e.key==K::Left)){if(readerIsEpub)readerGoPrevPage();else lv_obj_scroll_by(readerContent,0,400,LV_ANIM_OFF);return;}
    if(!Tab5Focus.focused()&&(e.key==K::Down||e.key==K::Up)){if(readerIsEpub){if(e.key==K::Down)readerGoNextPage();else readerGoPrevPage();}else lv_obj_scroll_by(readerContent,0,e.key==K::Down?-80:80,LV_ANIM_OFF);return;}
  }
  if(imageViewerScreen&&lv_screen_active()==imageViewerScreen&&backKey){stopImageViewer();Tab5Focus.clear();return;}
  if(videoViewerActive&&backKey){stopVideoViewer();Tab5Focus.clear();return;}
  if(lv_screen_active()!=root){Tab5Focus.key(e,lv_screen_active());return;}
  if(keyboardDockFocus){
    if(e.key==K::Escape||e.key==K::Right){keyboardToApp();return;}
    auto* navigation=Tab5Shell::choices.dockWidth()?dock:topBar;Tab5Focus.remember(navigation);Tab5Focus.key(e,navigation);
    if(!keyboardDockFocus){keyboardUiNavigation=true;Tab5Focus.restore(keyboardScope());}
    return;
  }
  auto* scope=keyboardScope();
  if(activePage==Page::ImageEditor&&scope==Tab5ImageEditor::scope()&&Tab5ImageEditor::key(e))return;
  if(activePage==Page::Audio&&scope==audioPage&&audioRows&&audioRows->key(e))return;
  if(scope==filesPage&&fileSelectionKey(e))return;
  if(e.key==K::F5){
    if(activePage==Page::Files&&scope==filesPage){refreshFileList();return;}
    if(activePage==Page::Home&&scope==homePage){homeAppsDirty=true;return;}
  }
  if((e.ctrl||e.meta)&&e.key==K::Text&&(e.text[0]=='f'||e.text[0]=='F')){
    if(activePage==Page::Files&&scope==filesPage){fileSearchEvent(nullptr);return;}
    if(activePage==Page::Audio&&scope==audioPage){audioSearchEvent(nullptr);return;}
  }
  if(scope==Tab5Tools.scope() && Tab5Tools.key(e))return;
  if(activePage==Page::Files && scope==filesPage && (e.ctrl||e.meta) && e.key==K::Text && e.text[0]=='p') {
    if(selectedPath.length()&&!selectedIsDir)Tab5Tools.open(Tab5Tool::Print,selectedPath.c_str());return;
  }
  if(activePage==Page::Writing && Tab5Focus.contains(Tab5Writing.object(),scope)) {
    if(Tab5Writing.key(e,dock)){if(Tab5Focus.contains(Tab5Shell::choices.dockWidth()?dock:topBar,Tab5Focus.focused())){keyboardDockFocus=true;keyboardUiNavigation=true;}Tab5Focus.remember(keyboardScope());return;}
    if(scope!=Tab5Writing.object())return; // Keep modal navigation inside the dialog.
    if(e.key==K::Escape||e.key==K::Left)keyboardToDock();
    return;
  }
  if(!Tab5Focus.controlling() && (keyboardUiNavigation || !keyboardObjectVisible(keyboardTarget))) {
    if(activePage==Page::Files&&scope==filesPage&&(e.ctrl||e.meta)&&e.key==K::Text){
      if(e.text[0]=='c'){copySelectedEvent(nullptr);return;}
      if(e.text[0]=='x'){cutSelectedEvent(nullptr);return;}
      if(e.text[0]=='v'){pasteClipboardEvent(nullptr);return;}
      if(e.text[0]=='h'){Tab5Files::showHidden=!Tab5Files::showHidden;settingsDirty=true;lastSettingsChangeMs=millis();refreshFileList();return;}
    }
    if(activePage==Page::Files && scope==filesPage && (e.key==K::Backspace||(e.alt&&e.key==K::Left))){upFolderEvent(nullptr);return;}
    if(activePage==Page::Files&&scope==filesPage&&e.key==K::Delete){deleteDocEvent(nullptr);return;}
    if(activePage==Page::Audio && scope==audioPage) {
      if(e.alt&&e.key==K::Left){audioPreviousEvent(nullptr);return;}
      if(e.alt&&e.key==K::Right){audioNextEvent(nullptr);return;}
      if(e.key==K::Text&&!e.ctrl&&!e.meta&&e.text[0]==' '){audioPlayEvent(nullptr);return;}
    }
    if(activePage==Page::Map && scope==mapPage && e.ctrl&&e.key==K::Text&&e.text[0]=='f'){mapSearchEvent(nullptr);return;}
  }
  const auto* focusedControl=Tab5Focus.focused();
  const bool textControl=focusedControl&&lv_obj_check_type(focusedControl,&lv_textarea_class);
  if(backKey&&!textControl&&!Tab5Focus.controlling()&&!(keyboardObjectVisible(keyboardTarget)&&!keyboardUiNavigation))e.key=K::Escape;
  if(e.key==K::Escape){
    if(Tab5Focus.key(e,scope))return;
    if(scope==homePage&&homePythonLibrary){showPythonLibrary(false);Tab5Focus.restore(homePage);return;}
    if(scope==mapExportProgress){if(mapJpegPreparing)Tab5MapPrint::cancel();if(mapJpegEncoding)Tab5Jpeg::cancel();return;}
    if(scope==fileDeleteOverlay){closeFileDeleteDialog();return;}
    if(scope==dialogOverlay){closeTextDialog();Tab5Focus.restore(keyboardScope());return;}
    if(scope==filePopupOverlay){closeFilePopup();Tab5Focus.restore(keyboardScope());return;}
    if(scope==wifiOverlay){closeWifiPopup();Tab5Focus.restore(keyboardScope());return;}
    if(scope==mapPlacesDeleteOverlay){closeMapPlaceDeleteOverlay();Tab5Focus.restore(keyboardScope());return;}
    if(scope==mapPlacesOverlay){closeMapPlacesEvent(nullptr);Tab5Focus.restore(keyboardScope());return;}
    if(scope==mapStylesOverlay){closeMapStylesEvent(nullptr);Tab5Focus.restore(keyboardScope());return;}
    if(scope==mapSearchOverlay){closeMapSearchEvent(nullptr);Tab5Focus.restore(keyboardScope());return;}
    if(scope==mapDownloadOverlay){lv_obj_delete(mapDownloadOverlay);mapDownloadOverlay=nullptr;Tab5Focus.restore(keyboardScope());return;}
    if(!keyboardUiNavigation&&keyboardObjectVisible(keyboardTarget)&&lv_obj_check_type(keyboardTarget,&lv_textarea_class)){
      keyboardUiNavigation=true;setKeyboardVisible(false);Tab5Focus.focusObject(keyboardTarget);return;
    }
    keyboardToDock();return;
  }
  if(e.key==K::Tab)keyboardUiNavigation=true;
  if(!keyboardUiNavigation&&keyboardObjectVisible(keyboardTarget)&&lv_obj_check_type(keyboardTarget,&lv_textarea_class)&&Tab5Focus.edit(keyboardTarget,e))return;
  if(Tab5Focus.key(e,scope,nullptr,lv_obj_get_parent(scope)==pageHost?(Tab5Shell::choices.dockWidth()?dock:topBar):nullptr)){keyboardUiNavigation=true;if(Tab5Focus.contains(Tab5Shell::choices.dockWidth()?dock:topBar,Tab5Focus.focused()))keyboardDockFocus=true;if(scope!=keyboardScope()&&!Tab5Focus.focused())Tab5Focus.restore(keyboardScope());Tab5Focus.remember(keyboardScope());return;}
  if(e.key==K::Left && lv_obj_get_parent(scope)==pageHost)keyboardToDock();
}

static bool probeSdWrite(int& failure) {
  failure=0;
  const auto fail=[&](const char* operation,const char* where,int error){
    failure=error?error:EIO;
    tab5StorageFault(operation,where,failure);
    return false;
  };
  String path;
  for(unsigned n=0;n<32;++n){
    path=String("/.tab5-sd-check-")+millis()+"-"+n;
    bool present=false;
    if(!tab5AtomicProbe(path.c_str(),present)){
      return fail("verification SD",path.c_str(),errno);
    }
    if(!present)break;
    path="";
  }
  if(!path.length())return fail("verification SD","/",EEXIST);
  static const uint8_t bytes[]={0x54,0x35,0x53,0x44};
  errno=0;File output=tab5FS.open(path,FILE_WRITE);
  if(!output)return fail("verification ecriture",path.c_str(),errno);
  errno=0;
  int error=output.write(bytes,sizeof(bytes))==sizeof(bytes)?0:(errno?errno:EIO);
  errno=0;output.flush();if(errno&&!error)error=errno;
  if(!error){
    errno=0;const size_t stored=output.size();
    if(errno||stored!=sizeof(bytes)||output.getWriteError())error=errno?errno:EIO;
  }
  errno=0;output.close();if(errno&&!error)error=errno;
  if(error){
    tab5FS.remove(path);return fail("verification ecriture",path.c_str(),error);
  }
  errno=0;File input=tab5FS.open(path,FILE_READ);
  if(!input)error=errno?errno:EIO;
  uint8_t readback[sizeof(bytes)]{};
  if(!error){
    errno=0;const size_t stored=input.size();
    if(errno||stored!=sizeof(bytes))error=errno?errno:EIO;
  }
  if(!error){
    errno=0;const size_t received=input.read(readback,sizeof(readback));
    if(errno||received!=sizeof(readback)||memcmp(bytes,readback,sizeof(bytes)))error=errno?errno:EIO;
  }
  errno=0;input.close();if(errno&&!error)error=errno;
  errno=0;const bool removed=tab5FS.remove(path);const int removeError=errno;
  if(error)return fail("verification lecture",path.c_str(),error);
  if(!removed)return fail("verification nettoyage",path.c_str(),removeError);
  return true;
}
static void serviceSdRecovery(uint32_t now) {
  if(!tab5StorageRecoveryDue(now))return;
  Tab5SystemResources::cancel();
  // Release UI-owned iterators before attempting the exclusive lease.
  Tab5AppCatalogue::endScan();homeAppsScanning=false;homeAppsDirty=true;
  if(activePage==Page::Files&&!fileSessionRestorePending)rememberFileSession();
  fileTree.cancel();fileSearcher.cancel();fileListDir.close();cancelFolderPreview();
  fileSelectAllDirectory.close();fileSelectAllSearch=false;
  Tab5FilePicker::close();
  bool recovered=false;
  {
    Tab5StorageRecovery exclusive;
    if(!exclusive||Tab5ImageEditor::busy()||!Tab5Audio.storageIdle()||!Tab5Writing.storageIdle()||Tab5MapPrint::busy())return;
    Tab5Map.releaseStorageHandles();
    if(tab5SdOpenFiles()!=0)return;
    tab5DiagnosticMark(3,"reconnexion SD","",1);
    tab5SdSetAvailable(false);
    SD.end();SPI.end();
    pinMode(SD_SPI_CS_PIN,OUTPUT);digitalWrite(SD_SPI_CS_PIN,HIGH);
    SPI.begin(SD_SPI_SCK_PIN,SD_SPI_MISO_PIN,SD_SPI_MOSI_PIN,SD_SPI_CS_PIN);
    sdReady=SD.begin(SD_SPI_CS_PIN,SPI,10000000U,"/sd",24,false);
    tab5SdSetAvailable(sdReady);
    if(sdReady){File check=tab5FS.open("/",FILE_READ);sdReady=check&&check.isDirectory();check.close();}
    if(sdReady)sdReady=migrateSystemDirectory();
    tab5SdSetAvailable(sdReady);
    if(sdReady&&!reloadSettingsAfterMigration()){
      sdReady=false;tab5SdSetAvailable(false);
    }
    int recoveryError=0;
    if(!sdReady){recoveryError=ENODEV;tab5StorageFault("reconnexion","/",recoveryError);}
    recovered=sdReady&&probeSdWrite(recoveryError);
    const bool retry=tab5StorageTransportError(recoveryError);
    tab5StorageRecoveryDone(millis(),recovered,retry);
    // Setters only invalidate catalogues. No reload while the lease is exclusive.
    Tab5Audio.setStorageAvailable(sdReady);
    Tab5Video.setStorageAvailable(sdReady);
    Tab5Map.setStorageAvailable(sdReady);
  }
  if(sdReady){ensureDirectories();Tab5SystemResources::start(true);}
  tab5DiagnosticMark(3,recovered?"SD reconnectee":"echec SD",tab5StorageErrorText(),0);
  tab5StorageLog();
  toast(systemMigrationFailed?"Migration vers System incomplète : relancez la récupération SD":recovered?"SD reconnectee : lecture et ecriture verifiees - reessayez l'enregistrement":tab5StorageErrorText(),5000);
  if(sdReady&&activePage==Page::Files)refreshFileList();
}

static const char* storageSdAction(bool eject) {
  if(!eject){tab5StorageRequestRecovery();return "Reconnexion SD demandée";}
  if(!sdReady)return "Carte SD éjectée : vous pouvez la retirer";
  if(documentDirty||Tab5ImageEditor::dirty()||Tab5ImageEditor::busy()||!Tab5Writing.prepareSleep()){
    return "Enregistrez les documents et attendez la fin des écritures";
  }
  if(settingsDirty){if(!saveSettings())return "Enregistrement des réglages impossible";settingsDirty=false;}
  if(readerProgressDirty){saveReaderProgressNow();if(readerProgressDirty)return "Enregistrement de la lecture impossible";}
  Tab5SystemResources::cancel();
  Tab5AppCatalogue::endScan();homeAppsScanning=false;homeAppsDirty=true;
  if(activePage==Page::Files&&!fileSessionRestorePending)rememberFileSession();
  fileTree.cancel();fileSearcher.cancel();fileListDir.close();cancelFolderPreview();Tab5FilePicker::close();
  fileSelectAllDirectory.close();fileSelectAllSearch=false;
  {
    Tab5StorageRecovery exclusive;
    if(!exclusive||Tab5ImageEditor::busy()||!Tab5Audio.storageIdle()||!Tab5Writing.storageIdle()||Tab5MapPrint::busy()){
      return "Carte SD occupée";
    }
    Tab5Map.releaseStorageHandles();
    if(tab5SdOpenFiles()!=0)return "Fermez les applications utilisant la microSD";
    tab5SdSetAvailable(false);SD.end();SPI.end();sdReady=false;
    tab5StorageEjected();
    Tab5Audio.setStorageAvailable(false);Tab5Video.setStorageAvailable(false);Tab5Map.setStorageAvailable(false);
  }
  clearFilePreview();if(activePage==Page::Files)refreshFileList();
  return "Carte SD éjectée : vous pouvez la retirer";
}

#include "Tab5BatteryUi.inc"

void loop() {
  Tab5UiDiagnostics::loopBegin();
  static bool mainMenuWasVisible=false;
  const bool mainMenuVisible=shellAtMainMenu();
  if(mainMenuVisible!=mainMenuWasVisible){mainMenuWasVisible=mainMenuVisible;shellTopLayout();}
  static bool usbEjectPrepared=false;
  if(Tab5UsbStorage::ejecting()&&!usbEjectPrepared){
    usbEjectPrepared=true;
    if(activePage==Page::Files&&!fileSessionRestorePending)rememberFileSession();
  fileTree.cancel();fileSearcher.cancel();fileListDir.close();cancelFolderPreview();
    Tab5FilePicker::close();
    if(Tab5UsbStorage::path(selectedPath.c_str()))clearFilePreview();
    if(Tab5UsbStorage::path(currentDir.c_str()))currentDir="/";
  }else if(!Tab5UsbStorage::ejecting())usbEjectPrepared=false;
  static bool pythonWasRunning=false;
  const bool pythonRunning=Tab5Python::running();
  if(pythonWasRunning&&!pythonRunning)tab5FsChanged();
  pythonWasRunning=pythonRunning;
  static bool executableWasRunning=false;
  const bool executableRunning=tab5ExecutableBusy();
  if(executableWasRunning&&!executableRunning)tab5FsChanged();
  executableWasRunning=executableRunning;
  static uint32_t usbRevision=0;
  if(usbRevision!=Tab5UsbStorage::revision()){usbRevision=Tab5UsbStorage::revision();if(activePage==Page::Files)refreshFileList();}
  uint32_t now = millis();

  if(tab5FileShareBusy() && Tab5Audio.storageIdle() && Tab5Writing.storageIdle() && !Tab5MapPrint::busy() && !fileSearcher.busy())
    tab5FileShareService();

  { Tab5UiDiagnostics::Scope inputTiming(Tab5UiDiagnostics::Input);
  M5.update();
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::System); serviceScreenEnergy(now); }
  if(lvTouch)lv_indev_read(lvTouch);
  if(Tab5Mouse::service()){screenIdle.wake(now);keyboardUiNavigation=false;keyboardDockFocus=false;}
  serviceDisplayOrientation();
  if(M5.Touch.getCount()>0&&M5.Touch.getDetail(0).wasPressed()){keyboardUiNavigation=false;keyboardDockFocus=false;Tab5UiStyle::pointerLeave();Tab5Focus.clear();}
  static bool pythonImage=false;
  if(pythonImage&&!imageViewerActive){Tab5PythonUI::imageFinished();pythonImage=false;}
  char pythonImagePath[272];
  if(Tab5PythonUI::imageRequest(pythonImagePath)){
    std::string canonical;
    if(!Tab5Files::canonical(pythonImagePath,canonical)||!isReadableImageFile(String(canonical.c_str())))Tab5PythonUI::imageFinished(-EINVAL);
    else {Tab5PythonReader::suspend();setKeyboardVisible(false);startImageViewer(String(canonical.c_str()));pythonImage=imageViewerActive;if(!pythonImage)Tab5PythonUI::imageFinished(-EIO);}
  }
  if(writing::usbGamepadActivity())screenIdle.wake(now);
  serviceGamepadSettings();
  serviceKeyboardFocus();
  imeService();
  writing::KeyEvent writingKey;
  for (int keyCount = 0; keyCount < 12 && writing::usbPoll(writingKey, now); ++keyCount) {
    const bool wasAsleep = screenIdle.level() == Tab5ScreenIdle::Level::Off;
    screenIdle.wake(now);
    if (!wasAsleep) dispatchKeyboard(writingKey);
  }
  static writing::GamepadNavigation gamepadNavigation;
  const bool gamepadNavigationEnabled = !Tab5NativeView::visible() &&
      !(gamepadSettingsVisible() && settingsPadStep >= 0) &&
      !(Tab5Tools.pythonForeground() && writing::gamepadPythonCaptured());
  for (unsigned n=0;n<12 && gamepadNavigation.poll(writing::usbGamepadButtons(),now,gamepadNavigationEnabled,writingKey);++n) {
    keyboardUiNavigation=true;
    dispatchKeyboard(writingKey);
  }
  } // Input dispatch
  updateResourceForeground();
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::System); tab5ResourcesService(); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Files); serviceSdRecovery(now); }
  {
    Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Files);
    const bool restoring=Tab5SystemResources::busy();
    Tab5SystemResources::service();
    if(restoring&&!Tab5SystemResources::busy()&&Tab5SystemResources::created())homeAppsDirty=true;
  }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Audio); Tab5Audio.service(); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Video); Tab5Video.service(); }
  Tab5MediaSession::service(now);
  serialSettingsService();
  // One byte per loop: diagnostic input must not starve UI processing.
  if (Tab5DebugSerial.available()) {
    const int command=Tab5DebugSerial.read();
    if(command=='m') {
      tab5DumpMemory([](const char* bytes,size_t count,void*){Tab5DebugSerial.write(reinterpret_cast<const uint8_t*>(bytes),count);},nullptr);
      tab5StorageLog();
    } else if(command=='p') {
      const bool accepted=Tab5UiDiagnostics::enable(!Tab5UiDiagnostics::enabled());
      if(!accepted)Tab5DebugSerial.println("[UI] profiling allocation failed");
      if(latencySwitch&&lv_obj_is_valid(latencySwitch)){
        if(Tab5UiDiagnostics::enabled())lv_obj_add_state(latencySwitch,LV_STATE_CHECKED);
        else lv_obj_remove_state(latencySwitch,LV_STATE_CHECKED);
      }
      Tab5DebugSerial.printf("[UI] profiling %s\n",Tab5UiDiagnostics::enabled()?"on":"off");
    } else if(command=='d') {
      printDisplayDiagnostics();
      const auto timing=Tab5UiDiagnostics::snapshot();
      Tab5DebugSerial.printf("[UI] wall time (not CPU), enabled=%u, max gap=%lu us\n",unsigned(timing.enabled),static_cast<unsigned long>(timing.maxGapUs));
      for(unsigned i=0;i<Tab5UiDiagnostics::Count;++i) {
        const auto& phase=timing.phases[i];if(!phase.calls)continue;
        Tab5DebugSerial.printf("[UI] %s: calls=%lu total=%llu us max=%lu us >=20ms=%lu\n",Tab5UiDiagnostics::phaseName(i),
            static_cast<unsigned long>(phase.calls),static_cast<unsigned long long>(phase.totalUs),
            static_cast<unsigned long>(phase.maxUs),static_cast<unsigned long>(phase.slow));
      }
      char events[640];
      if(Tab5UiDiagnostics::recent(events,sizeof(events)))Tab5DebugSerial.printf("[UI recent]\n%s\n",events);
      if(Tab5UiDiagnostics::recent(events,sizeof(events),true))Tab5DebugSerial.printf("[UI previous reset]\n%s\n",events);
    }
  }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Network); tab5NetDiagnosticsService(); }
  Tab5DisplayRefresh::service(lvDisplay);
  uint32_t displayUnderruns = 0;
  if (tab5DisplayTakeUnderrunEvent(millis(), &displayUnderruns)) {
    // Never log in the DSI IRQ: UART output can delay the next frame restart.
    const auto signals = tab5DisplaySignalsStats();
    Tab5DebugSerial.printf("[DSI underrun] delta=%lu total=%lu page=%u atUs=%llu\n",
        static_cast<unsigned long>(displayUnderruns),
        static_cast<unsigned long>(signals.underruns), unsigned(activePage),
        static_cast<unsigned long long>(signals.lastUnderrunUs));
  }
  uint32_t lvWait;
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Timers);
    lvWait = lv_timer_handler(); }
  if(tab5DisplayCacheNeedsRedraw())lv_obj_invalidate(lv_screen_active());
  readerBookIndexService();
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Map); serviceMapPage(); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Map); Tab5MapPrint::service(mapGeneration,activePage==Page::Map); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Map); serviceMapJpeg(); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Files); serviceFileOperation();serviceFileList();serviceFileSearch();serviceFolderPreview();serviceFileAutoRefresh(); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Writing); Tab5Writing.service(millis(), settings.autosave, static_cast<uint32_t>(settings.autosaveDelaySec) * 1000U); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::ClockPower); serviceBatteryProtection(millis()); }

  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::ClockPower); updateClockLabels(false); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Network); serviceNetworkRtc(); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Audio); updateTopAudioIndicator(false); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Python); updatePythonResumeState(); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Network); refreshWifiPopupList(false); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Python); Tab5PythonReader::service(Tab5Tools.pythonForeground()); }
  { Tab5UiDiagnostics::Scope appTiming(Tab5UiDiagnostics::Services);
    tab5ExecutableService();
    if(tab5ExecutableTakeLaunchError())Tab5Tools.open(Tab5Tool::Executable,tab5ExecutableSourcePath());
    Tab5FilePicker::service();Tab5Alarm::service();Tab5ImageEditor::service();Tab5Meshtastic::service();
  }
  {const auto badge=Tab5Meshtastic::badge();static uint32_t lastRevision=UINT32_MAX;
    if(badge.revision!=lastRevision){lastRevision=badge.revision;auto* object=Tab5MeshBadge::object();const bool wasVisible=object&&!lv_obj_has_flag(object,LV_OBJ_FLAG_HIDDEN);
      Tab5MeshBadge::update(badge.active,badge.ready,badge.unread,T(badge.unread?"Meshtastic · Nouveau message":"Meshtastic"));if(wasVisible!=badge.active)shellTopLayout();}}
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Tools); Tab5ContextMenu::service(); Tab5Tools.service(); Tab5GitView::service(); Tab5VaultPrompt::service(); }
  { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Home); serviceHomeApps(); }
  if (activePage == Page::Settings) { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::System); refreshWifiSettingsUi(false); updateFileShareUi(now); }
  if(Tab5SystemInfoView::scope()&&uint32_t(now-systemInfoUpdatedAt)>=2000){Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::System);systemInfoRefreshRequested=true;refreshSystemInfo();}
  if (activePage == Page::Audio) { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Audio); updateAudioUi(false); }
  if (videoViewerActive) { Tab5UiDiagnostics::Activity activity(Tab5DisplayActivity::Source::Video); updateVideoViewer(false); }

  { Tab5UiDiagnostics::Scope persistenceTiming(Tab5UiDiagnostics::Persistence);
  if (historyPending && now - lastHistoryEditMs >= 650) {
    pushHistorySnapshot(false);
  }

  if (activePage == Page::Preview && statsPending && now - lastEditMs > 300) {
    updateEditorStatus(true);
  }

  if (settingsDirty && sdReady && now - lastSettingsChangeMs >= 800) {
    settingsDirty = !saveSettings();
    if (settingsDirty) lastSettingsChangeMs = now;
  }

  // Avoid one FAT write per page turn. Save after a short pause, and always
  // force-save in stopReader().
  if (readerProgressDirty && readerActive && sdReady &&
      now - lastReaderProgressChangeMs >= 1200) {
    saveReaderProgressNow();
  }

  if (settings.autosave && documentDirty && currentPath.length() && sdReady) {
    uint32_t delayMs = static_cast<uint32_t>(settings.autosaveDelaySec) * 1000UL;
    if (now - lastEditMs >= delayMs && now - lastSaveMs >= delayMs) {
      saveDocumentTo(currentPath, false);
    }
  }

  } // Existing persistence policy remains unchanged.
  Tab5UiDiagnostics::loopEnd();

  // Keep touch responsive without spinning the P4 at several hundred loop iterations/s.
  const uint32_t maxWait = screenIdle.level() == Tab5ScreenIdle::Level::Off ? 30U : 8U;
  uint32_t sleepMs = std::min(lvWait, maxWait);
  if (sleepMs < 1) sleepMs = 1;
  delay(sleepMs);
}
