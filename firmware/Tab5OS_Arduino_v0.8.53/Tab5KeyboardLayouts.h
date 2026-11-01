#pragma once

#include <Arduino.h>
#ifndef ARDUINOJSON_ENABLE_STD_STREAM
#define ARDUINOJSON_ENABLE_STD_STREAM 0
#endif
#ifndef ARDUINOJSON_ENABLE_STD_STRING
#define ARDUINOJSON_ENABLE_STD_STRING 0
#endif
#include <ArduinoJson.h>
#include <vector>
#include "WriterKeys.h"

struct Tab5KeyDef {
  String label;
  String value;
  String accents;
  uint8_t grow = 1;
};

class Tab5KeyboardLayouts {
 public:
  bool begin(bool sdAvailable);
  // Populate the settings selector on demand; only the active layout is loaded
  // during boot. Dynamic SD layouts remain available without recompiling.
  void discoverLayouts();
  size_t count() const;
  const std::vector<writing::KeyMapping>& physical()const{return _active.physical;}
  String name(size_t index) const;
  String id(size_t index) const;
  size_t selectedIndex() const;
  String selectedId() const;
  String selectedName() const;
  bool select(size_t index, bool userChoice = true);
  bool selectForLanguage(const String& language, bool force = false);
  bool automatic() const { return !_manual; }
  bool setAutomatic(bool value, const String& language);
  static const char* defaultForLanguage(const char* language);

  const std::vector<Tab5KeyDef>& row(uint8_t layer, uint8_t row) const;

 private:
  struct Layout {
    String id;
    String name;
    std::vector<writing::KeyMapping> physical;
    std::vector<Tab5KeyDef> rows[3][4];
  };

  struct CatalogueEntry {
    String id;
    String name;
    bool builtIn = false;
    bool cached = false;
  };

  void setBuiltInEnglish(Layout& target) const;
  static bool sameLayout(const Layout& left, const Layout& right);
  bool parseLayout(const JsonObjectConst& object, Layout& target) const;
  bool parseLayer(const JsonVariantConst& value,
                  std::vector<Tab5KeyDef> target[4]) const;
  bool loadCachedLayout(Layout& target);
  bool loadSdLayout(const String& id, Layout& target) const;
  void scanCatalogue();
  void addCatalogue(const String& id, const String& name,
                    bool builtIn, bool cached);
  bool saveCache() const;

  bool _sdAvailable = false;
  bool _catalogueScanned = false;
  bool _manual = false;
  Layout _active;
  std::vector<CatalogueEntry> _catalogue;
};

extern Tab5KeyboardLayouts Tab5Keyboards;
