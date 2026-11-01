#include "Tab5VolumeFS.h"
#include "Tab5Localization.h"
#include "Tab5TranslationFormat.h"
#include "Tab5LanguageMetadata.h"
#include "Tab5LanguageReader.h"

#ifndef ARDUINOJSON_ENABLE_STD_STREAM
#define ARDUINOJSON_ENABLE_STD_STREAM 0
#endif
#ifndef ARDUINOJSON_ENABLE_STD_STRING
#define ARDUINOJSON_ENABLE_STD_STRING 0
#endif
#include "Tab5Json.h"
#include "Tab5JsonFile.h"
#include <Preferences.h>
#include <SD.h>
#include <algorithm>
#include <cstring>
#include "Tab5EnglishFallback.h"
#include "Tab5EnglishBitmap.h"

static constexpr char LANGUAGE_DIR[] = "/System/languages";
static constexpr char UI_PREFS[] = "tab5os-ui";
static constexpr char LANGUAGE_KEY[] = "language";
static constexpr char LANGUAGE_PATH_KEY[] = "language_path";
static constexpr size_t MAX_LANGUAGE_FILE = 512 * 1024;
static constexpr size_t MAX_TRANSLATIONS = 4096;
static constexpr size_t MAX_LANGUAGES = 64;

Tab5Localization Tab5Locale;

Tab5Localization::~Tab5Localization() {
  clearTranslations();
  if (_translationMutex) vSemaphoreDelete(_translationMutex);
}

void Tab5Localization::clearTranslations() {
  heap_caps_free(_translations);
  heap_caps_free(_translationText);
  _translations = nullptr;
  _translationText = nullptr;
  _translationCount = 0;
}

void Tab5Localization::addLanguage(const String& id, const String& name,
                                   const String& path) {
  if (!id.length() || id.length() > 63 || name.length() > 127) return;
  for (LanguageEntry& item : _languages) {
    if (item.id != id) continue;
    if (name.length()) item.name = name;
    if (path.length()) item.path = path;
    return;
  }
  if (_languages.size() >= MAX_LANGUAGES) return;
  LanguageEntry entry;
  entry.id = id;
  entry.name = name.length() ? name : id;
  entry.path = path;
  _languages.push_back(entry);
}

bool Tab5Localization::readLanguageMetadata(const String& path, String& id,
                                             String& name) const {
  File file = tab5FS.open(path.c_str(), FILE_READ);
  if (!file || file.size() > MAX_LANGUAGE_FILE) {
    if (file) file.close();
    return false;
  }
  JsonDocument document(tab5JsonAllocator());
  JsonDocument filter(tab5JsonAllocator());
  filter["id"] = true;
  filter["name"] = true;
  // Catalogue discovery must not parse every translated help page. Shipped
  // packs expose id/name before translations, so only their first KiB is read.
  // Custom packs with another member order retain the complete-parser path.
  char prefix[1026];
  const size_t read=file.read(reinterpret_cast<uint8_t*>(prefix),sizeof(prefix)-2);
  const size_t boundary=Tab5LanguageMetadata::prefixLength(prefix,read);
  if(boundary) {
    prefix[boundary]='}';prefix[boundary+1]='\0';
    const auto error=deserializeJson(document,prefix,DeserializationOption::Filter(filter));
    if(!error) {
      id=String(document["id"]|"");name=String(document["name"]|id.c_str());
      if(id.length()&&name.length()){file.close();return true;}
    }
    document.clear();
  }
  if(!file.seek(0)){file.close();return false;}
  Tab5JsonFileReader reader(file);
  const DeserializationError error = deserializeJson(
      document, reader, DeserializationOption::Filter(filter));
  file.close();
  if (error) return false;
  id = String(document["id"] | "");
  name = String(document["name"] | id.c_str());
  return id.length() && name.length();
}

void Tab5Localization::scanLanguages() {
  _catalogueScanned = true;
  _languages.clear();
  addLanguage("en-US", "English", "");
  addLanguage("fr-FR", "Français", "");
  if (_selectedId != "fr-FR" && _selectedId != "en-US")
    addLanguage(_selectedId, _selectedName, _selectedPath);
  if (!_sdAvailable || !tab5FS.exists(LANGUAGE_DIR)) return;
  File directory = tab5FS.open(LANGUAGE_DIR);
  if (!directory || !directory.isDirectory()) return;
  File entry = directory.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      String path = entry.name();
      if (!path.startsWith("/")) path = String(LANGUAGE_DIR) + "/" + path;
      String lower = path;
      lower.toLowerCase();
      if (lower.endsWith(".json")) {
        entry.close();
        String id;
        String name;
        if (readLanguageMetadata(path, id, name)) addLanguage(id, name, path);
        entry = directory.openNextFile();
        continue;
      }
    }
    entry.close();
    entry = directory.openNextFile();
  }
  directory.close();
}

void Tab5Localization::discoverLanguages() {
  if (!_catalogueScanned) scanLanguages();
}

bool Tab5Localization::loadLanguage(const LanguageEntry& language) {
  if (!language.path.length()) {
    if (_translationMutex) xSemaphoreTake(_translationMutex, portMAX_DELAY);
    clearTranslations();
    _selectedId = language.id;
    _selectedName = language.name;
    _selectedPath = "";
    if (_translationMutex) xSemaphoreGive(_translationMutex);
    return true;
  }
  File file = tab5FS.open(language.path.c_str(), FILE_READ);
  const size_t size=file?file.size():0;
  if (!file || file.isDirectory() || size > MAX_LANGUAGE_FILE) {
    if (file) file.close();
    return false;
  }
  // One bounded PSRAM read and a flat parser: no quadratic DOM/string pool.
  // All staging memory remains separate from the currently active dictionary.
  struct Buffer {
    void* data=nullptr;
    explicit Buffer(size_t bytes):data(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)){}
    ~Buffer(){heap_caps_free(data);}
  } input(size+1), table(MAX_TRANSLATIONS*sizeof(Tab5LanguageReader::Entry));
  if(!input.data||!table.data){file.close();return false;}
  size_t offset=0;
  auto* raw=static_cast<char*>(input.data);
  while(offset<size){
    const size_t remaining=size-offset,request=std::min(size_t(4096),remaining);
    const size_t read=file.read(reinterpret_cast<uint8_t*>(raw+offset),request);
    if(!read||read>request){file.close();return false;}
    offset+=read;
  }
  file.close();raw[size]=0;
  auto* parsed=static_cast<Tab5LanguageReader::Entry*>(table.data);
  Tab5LanguageReader::Pack pack;
  if(!Tab5LanguageReader::parse(raw,size,parsed,MAX_TRANSLATIONS,pack)||language.id!=pack.id)return false;
  const char* packName=pack.name;
  const size_t count=pack.count;
  size_t bytes=0;
  for(size_t i=0;i<count;++i)bytes+=strlen(parsed[i].source)+strlen(parsed[i].translated)+2;
  TranslationEntry* entries=nullptr;char* storage=nullptr;
  if(count){
    entries=static_cast<TranslationEntry*>(heap_caps_malloc(count*sizeof(TranslationEntry),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    storage=static_cast<char*>(heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    if(!entries||!storage){heap_caps_free(entries);heap_caps_free(storage);return false;}
    char* cursor=storage;
    for(size_t i=0;i<count;++i){
      const size_t sourceSize=strlen(parsed[i].source)+1;
      const size_t translatedSize=strlen(parsed[i].translated)+1;
      entries[i].source=cursor;memcpy(cursor,parsed[i].source,sourceSize);cursor+=sourceSize;
      entries[i].translated=cursor;memcpy(cursor,parsed[i].translated,translatedSize);cursor+=translatedSize;
    }
  }
  // Commit only after parsing and allocation succeed: a malformed SD pack
  // must not silently erase the currently selected language.
  if (_translationMutex) xSemaphoreTake(_translationMutex, portMAX_DELAY);
  clearTranslations();
  _translations = entries;
  _translationText = storage;
  _translationCount = count;
  _selectedId = language.id;
  _selectedName = packName;
  _selectedPath = language.path;
  if (_translationMutex) xSemaphoreGive(_translationMutex);
  return true;
}

bool Tab5Localization::saveSelection() const {
  Preferences preferences;
  if (!preferences.begin(UI_PREFS, false)) return false;
  const bool ok = preferences.putString(LANGUAGE_KEY, _selectedId) > 0;
  // Retain arbitrary filenames: a custom pack is not required to be named
  // after its language ID. Empty paths denote the built-in French fallback.
  if (ok) preferences.putString(LANGUAGE_PATH_KEY, _selectedPath);
  preferences.end();
  return ok;
}

bool Tab5Localization::begin(bool sdAvailable) {
  _sdAvailable = sdAvailable;
  _fontFallback = false;
  _catalogueScanned = false;
  if (!_translationMutex) _translationMutex = xSemaphoreCreateMutex();
  if (!_translationMutex) return false;
  String requested = "en-US";
  String requestedPath;
  Preferences preferences;
  if (preferences.begin(UI_PREFS, true)) {
    requested = preferences.getString(LANGUAGE_KEY, "en-US");
    requestedPath = preferences.getString(LANGUAGE_PATH_KEY, "");
    preferences.end();
  }
  _languages.clear();
  addLanguage("en-US", "English", "");
  addLanguage("fr-FR", "Français", "");
  if (_sdAvailable) {
    if (requestedPath.startsWith(String(LANGUAGE_DIR) + "/") &&
        requestedPath.indexOf("..") < 0) {
      LanguageEntry selected{requested, requested, requestedPath};
      if (loadLanguage(selected)) {
        addLanguage(_selectedId, _selectedName, _selectedPath);
        return true;
      }
    }
    // Normal installations need one pack read, with no scan of other packs.
    // Legacy arbitrary filenames are discovered below if this path fails.
    if (requested.length() && requested.length() <= 63 &&
        requested.indexOf('/') < 0 && requested.indexOf('\\') < 0 &&
        requested.indexOf("..") < 0) {
      const String path = String(LANGUAGE_DIR) + "/" + requested + ".json";
      if (path != requestedPath) {
        LanguageEntry selected{requested, requested, path};
        if (loadLanguage(selected)) {
          addLanguage(_selectedId, _selectedName, _selectedPath);
          return true;
        }
      }
    }
    scanLanguages();
    for (const LanguageEntry& language : _languages) {
      if (language.id == requested && loadLanguage(language)) return true;
    }
  }
  // Keep an explicitly selected French interface usable without its SD pack.
  const LanguageEntry fallback{requested=="fr-FR"?"fr-FR":"en-US",
      requested=="fr-FR"?"Français":"English", ""};
  loadLanguage(fallback);
  return true;
}

const char* Tab5Localization::text(const char* source) const {
  if (!source || !source[0]) return source ? source : "";
  size_t first = 0, end = _fontFallback ? 0 : _translationCount;
  while (first < end) {
    const size_t mid = first + (end - first) / 2;
    const int compared = strcmp(_translations[mid].source, source);
    if (!compared) return _translations[mid].translated;
    if (compared < 0) first = mid + 1;
    else end = mid;
  }
  if(_fontFallback || _selectedId!="fr-FR") {
    const auto* entries = _fontFallback ? TAB5_ENGLISH_BITMAP : TAB5_ENGLISH_FALLBACK;
    size_t first=0, end=_fontFallback ? sizeof(TAB5_ENGLISH_BITMAP)/sizeof(TAB5_ENGLISH_BITMAP[0]) : sizeof(TAB5_ENGLISH_FALLBACK)/sizeof(TAB5_ENGLISH_FALLBACK[0]);
    while(first<end){
      const size_t mid=first+(end-first)/2;
      const int compared=strcmp(entries[mid].source,source);
      if(!compared)return entries[mid].translated;
      if(compared<0)first=mid+1;else end=mid;
    }
  }
  return source;
}

String Tab5Localization::text(const String& source) const {
  return String(text(source.c_str()));
}

size_t Tab5Localization::copyText(const char* source, char* destination,
                                 size_t capacity) const {
  if (_translationMutex) xSemaphoreTake(_translationMutex, portMAX_DELAY);
  const char* translated = text(source);
  const size_t length = strlen(translated);
  if (destination && capacity) {
    const size_t copied = length < capacity - 1 ? length : capacity - 1;
    memcpy(destination, translated, copied);
    destination[copied] = '\0';
  }
  if (_translationMutex) xSemaphoreGive(_translationMutex);
  return length;
}

size_t Tab5Localization::count() const { return _languages.size(); }

String Tab5Localization::name(size_t index) const {
  if(index<_languages.size()) {
    const String& language=_languages[index].id;
    // The selector must be readable before an optional CJK face is loaded.
    if(language.startsWith("zh")&&(_fontFallback||!_selectedId.startsWith("zh")))return "Chinese (simplified) - zh-CN";
    if(language.startsWith("ja")&&(_fontFallback||!_selectedId.startsWith("ja")))return "Japanese - ja-JP";
    if(language.startsWith("ko")&&(_fontFallback||!_selectedId.startsWith("ko")))return "Korean - ko-KR";
  }
  if (_fontFallback && index < _languages.size()) {
    const String& language = _languages[index].id;
    if (language == "en-US") return "English";
    if (language == "fr-FR") return "French";
    if (language == "de-DE") return "German";
    if (language == "es-ES") return "Spanish";
    if (language == "it-IT") return "Italian";
    if (language == "nl-NL") return "Dutch";
    if (language == "pt-PT") return "Portuguese";
    if (language == "pt-BR") return "Portuguese (Brazil)";
    if (language == "ru-RU") return "Russian";
    if (language == "id-ID") return "Indonesian";
    if (language == "ar") return "Arabic";
    if (language == "zh-CN") return "Chinese (simplified)";
    if (language == "ja-JP") return "Japanese";
    if (language == "ko-KR") return "Korean";
    return "Language pack";
  }
  return index < _languages.size() ? _languages[index].name : String();
}

String Tab5Localization::id(size_t index) const {
  return index < _languages.size() ? _languages[index].id : String();
}

size_t Tab5Localization::selectedIndex() const {
  for (size_t i = 0; i < _languages.size(); ++i)
    if (_languages[i].id == _selectedId) return i;
  return 0;
}

String Tab5Localization::selectedId() const { return _selectedId; }
String Tab5Localization::selectedName() const { return _selectedName; }

void Tab5Localization::applyFontAvailability(bool unicodeFontAvailable) {
  if (_translationMutex) xSemaphoreTake(_translationMutex, portMAX_DELAY);
  _fontFallback = !unicodeFontAvailable;
  if (_translationMutex) xSemaphoreGive(_translationMutex);
}

bool Tab5Localization::select(size_t index) {
  if (index >= _languages.size()) return false;
  if (!loadLanguage(_languages[index])) return false;
  return saveSelection();
}
