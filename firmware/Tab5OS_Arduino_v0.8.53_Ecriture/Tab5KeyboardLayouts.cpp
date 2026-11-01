#include "Tab5VolumeFS.h"
#include "Tab5KeyboardLayouts.h"
#include "Tab5Localization.h"

#include "Tab5Json.h"
#include "Tab5JsonFile.h"
#include <Preferences.h>
#include <SD.h>
#include <utility>

static constexpr char KEYBOARD_FILE[] = "/System/keyboards.json";
static constexpr char INPUT_PREFS[] = "tab5os-input";
static constexpr char INPUT_ID_KEY[] = "kbd_id";
static constexpr char INPUT_CACHE_KEY[] = "kbd_blob";
static constexpr char INPUT_MANUAL_KEY[] = "kbd_manual";
static constexpr size_t MAX_CACHE_BYTES = 16 * 1024;
static constexpr size_t MAX_KEYBOARD_FILE = 512 * 1024;
static constexpr size_t MAX_LAYOUTS = 64;

Tab5KeyboardLayouts Tab5Keyboards;

static const char* actionToken(const char* action) {
  if (!action) return "";
  if (!strcmp(action, "backspace")) return "<BKSP>";
  if (!strcmp(action, "enter")) return "<ENTER>";
  if (!strcmp(action, "shift")) return "<SHIFT>";
  if (!strcmp(action, "symbols")) return "<SYM>";
  if (!strcmp(action, "letters")) return "<ABC>";
  if (!strcmp(action, "left")) return "<LEFT>";
  if (!strcmp(action, "right")) return "<RIGHT>";
  if (!strcmp(action, "space")) return " ";
  return "";
}

static void addSeed(std::vector<Tab5KeyDef>& row, const char* value,
                    uint8_t grow = 1, const char* accents = nullptr) {
  Tab5KeyDef key;
  key.value = value ? value : "";
  key.label = key.value;
  key.accents = accents ? accents : "";
  key.grow = grow ? grow : 1;
  row.push_back(std::move(key));
}

void Tab5KeyboardLayouts::setBuiltInEnglish(Layout& target) const {
  target = Layout();
  target.id = "en-US";
  target.name = "English (QWERTY)";
  for (uint8_t layer = 0; layer < 3; ++layer)
    for (uint8_t row = 0; row < 4; ++row)
      target.rows[layer][row].reserve(row == 3 ? 4 : 11);

  const char* lower[3][10] = {
      {"q","w","e","r","t","y","u","i","o","p"},
      {"a","s","d","f","g","h","j","k","l",nullptr},
      {"z","x","c","v","b","n","m",",",".","'"}};
  const char* upper[3][10] = {
      {"Q","W","E","R","T","Y","U","I","O","P"},
      {"A","S","D","F","G","H","J","K","L",nullptr},
      {"Z","X","C","V","B","N","M",",",".","'"}};
  for (uint8_t layer = 0; layer < 2; ++layer) {
    const char* (*letters)[10] = layer ? upper : lower;
    for (uint8_t column = 0; column < 10; ++column)
      addSeed(target.rows[layer][0], letters[0][column]);
    addSeed(target.rows[layer][0], "<BKSP>", 2);
    for (uint8_t column = 0; column < 9; ++column)
      addSeed(target.rows[layer][1], letters[1][column]);
    addSeed(target.rows[layer][1], "<ENTER>", 2);
    addSeed(target.rows[layer][2], "<SHIFT>", 2);
    for (uint8_t column = 0; column < 10; ++column)
      addSeed(target.rows[layer][2], letters[2][column]);
    addSeed(target.rows[layer][3], "<SYM>", 2);
    addSeed(target.rows[layer][3], " ", 7);
    addSeed(target.rows[layer][3], "<LEFT>", 2);
    addSeed(target.rows[layer][3], "<RIGHT>", 2);
  }

  const char* digits[] = {"1","2","3","4","5","6","7","8","9","0"};
  for (const char* value : digits) addSeed(target.rows[2][0], value);
  addSeed(target.rows[2][0], "<BKSP>", 2);
  const char* symbols2[] = {"@","#","$","&","*","-","_","/",":",";"};
  for (const char* value : symbols2) addSeed(target.rows[2][1], value);
  addSeed(target.rows[2][1], "<ENTER>", 2);
  const char* symbols3[] = {"(",")","[","]","{","}","+","=","!","?","\""};
  for (const char* value : symbols3) addSeed(target.rows[2][2], value);
  addSeed(target.rows[2][3], "<ABC>", 2);
  addSeed(target.rows[2][3], " ", 7);
  addSeed(target.rows[2][3], "<LEFT>", 2);
  addSeed(target.rows[2][3], "<RIGHT>", 2);
}

bool Tab5KeyboardLayouts::parseLayer(
    const JsonVariantConst& value, std::vector<Tab5KeyDef> target[4]) const {
  JsonArrayConst rows = value.as<JsonArrayConst>();
  if (rows.isNull() || rows.size() == 0 || rows.size() > 4) return false;
  uint8_t rowIndex = 0;
  for (JsonVariantConst rowValue : rows) {
    JsonArrayConst keys = rowValue.as<JsonArrayConst>();
    if (keys.isNull() || keys.size() == 0 || keys.size() > 14) return false;
    target[rowIndex].reserve(keys.size());
    for (JsonVariantConst keyValue : keys) {
      Tab5KeyDef key;
      if (keyValue.is<const char*>()) {
        key.value = keyValue.as<const char*>();
        key.label = key.value;
      } else {
        JsonObjectConst object = keyValue.as<JsonObjectConst>();
        if (object.isNull()) return false;
        const char* action = object["action"] | "";
        const char* token = actionToken(action);
        key.value = token[0] ? token : String(object["value"] | "");
        key.label = String(object["label"] | key.value.c_str());
        key.grow = constrain(static_cast<int>(object["width"] | 1), 1, 9);
        JsonVariantConst accents = object["accents"];
        if (accents.is<const char*>()) {
          key.accents = accents.as<const char*>();
        } else if (accents.is<JsonArrayConst>()) {
          for (JsonVariantConst accentValue : accents.as<JsonArrayConst>()) {
            const char* accent = accentValue.as<const char*>();
            if (!accent || !accent[0]) continue;
            if (key.accents.length()) key.accents += '|';
            key.accents += accent;
          }
        }
      }
      if (!key.value.length()) return false;
      if (!key.label.length()) key.label = key.value;
      target[rowIndex].push_back(std::move(key));
    }
    ++rowIndex;
  }
  return true;
}

bool Tab5KeyboardLayouts::sameLayout(const Layout& left, const Layout& right) {
  if (left.id != right.id || left.name != right.name || left.physical.size()!=right.physical.size()) return false;
  for(size_t i=0;i<left.physical.size();++i){const auto& a=left.physical[i];const auto& b=right.physical[i];if(a.usage!=b.usage||a.dead!=b.dead||a.letter!=b.letter||memcmp(a.values,b.values,sizeof(a.values)))return false;}
  for (uint8_t layer = 0; layer < 3; ++layer) {
    for (uint8_t row = 0; row < 4; ++row) {
      const auto& a = left.rows[layer][row];
      const auto& b = right.rows[layer][row];
      if (a.size() != b.size()) return false;
      for (size_t i = 0; i < a.size(); ++i)
        if (a[i].value != b[i].value || a[i].label != b[i].label ||
            a[i].accents != b[i].accents || a[i].grow != b[i].grow) return false;
    }
  }
  return true;
}

bool Tab5KeyboardLayouts::parseLayout(const JsonObjectConst& object,
                                      Layout& target) const {
  if (object.isNull()) return false;
  Layout parsed;
  parsed.id = String(object["id"] | "");
  parsed.name = String(object["name"] | parsed.id.c_str());
  if (!parsed.id.length() || !parsed.name.length()) return false;
  if (!parseLayer(object["lower"], parsed.rows[0]) ||
      !parseLayer(object["upper"], parsed.rows[1]) ||
      !parseLayer(object["symbols"], parsed.rows[2])) return false;
  auto bindings=object["hid"].as<JsonArrayConst>();if(bindings.size()>128)return false;
  for(auto binding:bindings){auto row=binding.as<JsonArrayConst>();if(row.size()<4||row.size()>6)return false;int usage=row[0]|0;if(usage<4||usage>223)return false;
    for(const auto& old:parsed.physical)if(old.usage==usage)return false;
    writing::KeyMapping key;key.usage=usage;key.letter=row[4]|false;key.dead=row[5]|0;if(key.dead>7)return false;
    for(unsigned i=0;i<3;++i){const char* text=row[i+1]|"";if(strlen(text)>=sizeof(key.values[i]))return false;strcpy(key.values[i],text);}parsed.physical.push_back(key);
  }
  target = std::move(parsed);
  return true;
}

bool Tab5KeyboardLayouts::loadCachedLayout(Layout& target) {
  Preferences preferences;
  if (!preferences.begin(INPUT_PREFS, true)) return false;
  const size_t size = preferences.getBytesLength(INPUT_CACHE_KEY);
  if (!size || size > MAX_CACHE_BYTES) {
    preferences.end();
    return false;
  }
  // This transient JSON can reach 16 KiB; reserve it in PSRAM, not in the
  // scarce internal heap that TLS and USB need.
  char* bytes = static_cast<char*>(heap_caps_malloc(
      size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!bytes) { preferences.end(); return false; }
  const size_t read = preferences.getBytes(INPUT_CACHE_KEY, bytes, size);
  preferences.end();
  if (read != size) { heap_caps_free(bytes); return false; }
  bytes[size] = '\0';
  JsonDocument document(tab5JsonAllocator());
  const DeserializationError error = deserializeJson(
      document, static_cast<const char*>(bytes), size);
  heap_caps_free(bytes);
  if (error) return false;
  return parseLayout(document.as<JsonObjectConst>(), target);
}

bool Tab5KeyboardLayouts::loadSdLayout(const String& id, Layout& target) const {
  if (!_sdAvailable || !tab5FS.exists(KEYBOARD_FILE)) return false;
  File file = tab5FS.open(KEYBOARD_FILE, FILE_READ);
  if (!file || file.size() > MAX_KEYBOARD_FILE) {
    if (file) file.close();
    return false;
  }
  JsonDocument document(tab5JsonAllocator());
  Tab5JsonFileReader reader(file);
  const DeserializationError error = deserializeJson(document, reader);
  file.close();
  if (error) return false;
  JsonArrayConst layouts = document["keyboards"].as<JsonArrayConst>();
  for (JsonObjectConst object : layouts) {
    if (String(object["id"] | "") == id) return parseLayout(object, target);
  }
  return false;
}

void Tab5KeyboardLayouts::addCatalogue(const String& id, const String& name,
                                       bool builtIn, bool cached) {
  if (!id.length() || id.length() > 63 || name.length() > 127) return;
  for (CatalogueEntry& item : _catalogue) {
    if (item.id != id) continue;
    if (name.length()) item.name = name;
    item.builtIn = item.builtIn || builtIn;
    item.cached = item.cached || cached;
    return;
  }
  if (_catalogue.size() >= MAX_LAYOUTS) return;
  CatalogueEntry entry;
  entry.id = id;
  entry.name = name.length() ? name : id;
  entry.builtIn = builtIn;
  entry.cached = cached;
  _catalogue.push_back(entry);
}

void Tab5KeyboardLayouts::scanCatalogue() {
  _catalogueScanned = true;
  _catalogue.clear();
  addCatalogue("en-US", "English (QWERTY)", true, _active.id == "en-US");
  if (_active.id != "en-US")
    addCatalogue(_active.id, _active.name, false, true);
  if (!_sdAvailable || !tab5FS.exists(KEYBOARD_FILE)) return;

  File file = tab5FS.open(KEYBOARD_FILE, FILE_READ);
  if (!file || file.size() > MAX_KEYBOARD_FILE) {
    if (file) file.close();
    return;
  }
  JsonDocument document(tab5JsonAllocator());
  JsonDocument filter(tab5JsonAllocator());
  filter["keyboards"][0]["id"] = true;
  filter["keyboards"][0]["name"] = true;
  Tab5JsonFileReader reader(file);
  const DeserializationError error = deserializeJson(
      document, reader, DeserializationOption::Filter(filter));
  file.close();
  if (error) return;
  for (JsonObjectConst object : document["keyboards"].as<JsonArrayConst>()) {
    addCatalogue(String(object["id"] | ""),
                 String(object["name"] | ""), false, false);
  }
}

void Tab5KeyboardLayouts::discoverLayouts() {
  if (!_catalogueScanned) scanCatalogue();
}

bool Tab5KeyboardLayouts::saveCache() const {
  JsonDocument document(tab5JsonAllocator());
  document["id"] = _active.id;
  document["name"] = _active.name;
  if(!_active.physical.empty()){auto hid=document["hid"].to<JsonArray>();for(const auto& key:_active.physical){auto row=hid.add<JsonArray>();row.add(key.usage);for(auto& text:key.values)row.add(text);row.add(key.letter);row.add(key.dead);}}
  const char* layerNames[] = {"lower", "upper", "symbols"};
  for (uint8_t layer = 0; layer < 3; ++layer) {
    JsonArray rows = document[layerNames[layer]].to<JsonArray>();
    for (uint8_t rowIndex = 0; rowIndex < 4; ++rowIndex) {
      if (_active.rows[layer][rowIndex].empty()) continue;
      JsonArray row = rows.add<JsonArray>();
      for (const Tab5KeyDef& key : _active.rows[layer][rowIndex]) {
        JsonObject item = row.add<JsonObject>();
        item["value"] = key.value;
        if (key.label != key.value) item["label"] = key.label;
        if (key.grow != 1) item["width"] = key.grow;
        if (key.accents.length()) item["accents"] = key.accents;
      }
    }
  }
  if (document.overflowed()) return false;
  const size_t bytes = measureJson(document);
  if (!bytes || bytes > MAX_CACHE_BYTES) return false;
  char* json = static_cast<char*>(heap_caps_malloc(
      bytes + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!json) return false;
  if (serializeJson(document, json, bytes + 1) != bytes) {
    heap_caps_free(json);
    return false;
  }
  Preferences preferences;
  if (!preferences.begin(INPUT_PREFS, false)) {
    heap_caps_free(json);
    return false;
  }
  const bool ok = preferences.putString(INPUT_ID_KEY, _active.id) > 0 &&
      preferences.putBytes(INPUT_CACHE_KEY, json, bytes) == bytes;
  if (ok) preferences.putBool(INPUT_MANUAL_KEY, _manual);
  preferences.end();
  heap_caps_free(json);
  return ok;
}

bool Tab5KeyboardLayouts::begin(bool sdAvailable) {
  _sdAvailable = sdAvailable;
  _catalogueScanned = false;
  Layout cached;
  if (loadCachedLayout(cached)) _active = std::move(cached);
  else setBuiltInEnglish(_active);
  Preferences preferences;
  if (preferences.begin(INPUT_PREFS, true)) {
    // A legacy non-default layout is an explicit preference. Never replace it
    // silently when migrating an existing installation.
    _manual = preferences.getBool(INPUT_MANUAL_KEY, _active.id != "en-US");
    preferences.end();
  }

  // La copie SD est prioritaire lorsqu'elle contient encore la disposition
  // choisie : une modification du JSON est donc prise en compte au démarrage.
  Layout fromSd;
  if (loadSdLayout(_active.id, fromSd)) {
    const bool changed = !sameLayout(_active, fromSd);
    _active = std::move(fromSd);
    // Preserve SD edits but avoid rewriting the identical NVS cache every boot.
    if (changed) saveCache();
  }
  _catalogue.clear();
  addCatalogue("en-US", "English (QWERTY)", true, _active.id == "en-US");
  if (_active.id != "en-US")
    addCatalogue(_active.id, _active.name, false, true);
  if (!_manual) selectForLanguage(Tab5Locale.selectedId());
  return true;
}

size_t Tab5KeyboardLayouts::count() const { return _catalogue.size(); }

String Tab5KeyboardLayouts::name(size_t index) const {
  if(index<_catalogue.size()) {
    const String& layout=_catalogue[index].id;
    if(layout.startsWith("zh")&&(Tab5Locale.fontFallbackActive()||!Tab5Locale.selectedId().startsWith("zh")))return "Chinese (Pinyin) - zh-CN";
    if(layout.startsWith("ja")&&(Tab5Locale.fontFallbackActive()||!Tab5Locale.selectedId().startsWith("ja")))return "Japanese (Romaji) - ja-JP";
    if(layout.startsWith("ko")&&(Tab5Locale.fontFallbackActive()||!Tab5Locale.selectedId().startsWith("ko")))return "Korean (2-beolsik) - ko-KR";
  }
  return index < _catalogue.size() ? _catalogue[index].name : String();
}

String Tab5KeyboardLayouts::id(size_t index) const {
  return index < _catalogue.size() ? _catalogue[index].id : String();
}

size_t Tab5KeyboardLayouts::selectedIndex() const {
  for (size_t i = 0; i < _catalogue.size(); ++i)
    if (_catalogue[i].id == _active.id) return i;
  return 0;
}

String Tab5KeyboardLayouts::selectedId() const { return _active.id; }
String Tab5KeyboardLayouts::selectedName() const { return _active.name; }

bool Tab5KeyboardLayouts::select(size_t index, bool userChoice) {
  if (index >= _catalogue.size()) return false;
  const CatalogueEntry entry = _catalogue[index];
  Layout selected;
  // SD also overrides en-US; selecting a built-in ID must not discard custom
  // key bindings when its editable SD definition is present.
  if (!loadSdLayout(entry.id, selected)) {
    if (entry.id == _active.id && entry.cached) selected = _active;
    else if (entry.builtIn) setBuiltInEnglish(selected);
    else return false;
  }
  _active = std::move(selected);
  _manual = userChoice;
  saveCache();
  return true;
}

const char* Tab5KeyboardLayouts::defaultForLanguage(const char* language) {
  if (!language) return "en-US";
  static const char* ids[]={"fr-FR","de-DE","es-ES","it-IT","nl-NL","pt-BR","pt-PT","ru-RU","id-ID","zh-CN","ja-JP","ko-KR","en-GB","en-US"};
  if (!strncmp(language,"ar",2)) return "ar";
  for (const char* id:ids) if (!strcmp(language,id)) return id;
  for (const char* id:ids) if (!strncmp(language,id,2)) return id;
  return "en-US";
}

bool Tab5KeyboardLayouts::selectForLanguage(const String& language, bool force) {
  if (_manual && !force) return true;
  const char* wanted = defaultForLanguage(language.c_str());
  if (_active.id == wanted) return true;
  discoverLayouts();
  for (size_t i=0;i<_catalogue.size();++i)
    if (_catalogue[i].id == wanted) return select(i,false);
  return false;
}

bool Tab5KeyboardLayouts::setAutomatic(bool value,const String& language) {
  const bool oldManual=_manual;
  _manual=!value;
  if(value&&!selectForLanguage(language,true)){_manual=oldManual;return false;}
  return saveCache();
}

const std::vector<Tab5KeyDef>& Tab5KeyboardLayouts::row(
    uint8_t layer, uint8_t rowIndex) const {
  static const std::vector<Tab5KeyDef> empty;
  if (layer >= 3 || rowIndex >= 4) return empty;
  return _active.rows[layer][rowIndex];
}
