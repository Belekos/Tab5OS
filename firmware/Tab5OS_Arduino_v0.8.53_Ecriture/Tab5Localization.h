#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <vector>

class Tab5Localization {
 public:
  Tab5Localization() = default;
  ~Tab5Localization();
  Tab5Localization(const Tab5Localization&) = delete;
  Tab5Localization& operator=(const Tab5Localization&) = delete;
  bool begin(bool sdAvailable);
  // Applied after loading the actual UI font. This changes only this session;
  // it never overwrites the user's stored language or document contents.
  void applyFontAvailability(bool unicodeFontAvailable);
  bool fontFallbackActive() const { return _fontFallback; }
  // Discover optional SD packs when the language selector is opened, not at boot.
  void discoverLanguages();
  const char* text(const char* source) const;
  String text(const String& source) const;
  // Worker-safe translation: copy while the active dictionary is protected.
  // Returns the complete byte length, like snprintf (capacity includes NUL).
  size_t copyText(const char* source, char* destination, size_t capacity) const;

  size_t count() const;
  String name(size_t index) const;
  String id(size_t index) const;
  size_t selectedIndex() const;
  String selectedId() const;
  String effectiveId() const { return _fontFallback ? String("en-US") : _selectedId; }
  String selectedName() const;
  bool hasTranslations() const { return _fontFallback || _translationCount != 0 || _selectedId == "en-US"; }
  bool select(size_t index);

 private:
  struct LanguageEntry {
    String id;
    String name;
    String path;
  };
  struct TranslationEntry {
    const char* source;
    const char* translated;
  };

  void scanLanguages();
  void addLanguage(const String& id, const String& name, const String& path);
  bool loadLanguage(const LanguageEntry& language);
  bool readLanguageMetadata(const String& path, String& id, String& name) const;
  bool saveSelection() const;
  void clearTranslations();

  bool _sdAvailable = false;
  bool _fontFallback = false;
  bool _catalogueScanned = false;
  String _selectedId = "en-US";
  String _selectedName = "English";
  String _selectedPath;
  SemaphoreHandle_t _translationMutex = nullptr;
  std::vector<LanguageEntry> _languages;
  // Language packs can contain thousands of labels. Keep their strings and
  // lookup table in PSRAM, not thousands of small internal-heap Strings.
  TranslationEntry* _translations = nullptr;
  char* _translationText = nullptr;
  size_t _translationCount = 0;
};

extern Tab5Localization Tab5Locale;
