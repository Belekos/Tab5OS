#pragma once
#include <lvgl.h>


#include "WriterKeys.h"
#include "WriterRich.h"
#include "WriterSyntax.h"
namespace writing {
struct ViewTheme {
  uint32_t foreground = 0xf1f3f5, muted = 0xadbab8, accent = 0x9ccfc0;
  uint32_t surface = 0x1e282b, selection = 0x38624f, border = 0x678078;
};
struct ViewMetrics {
  uint32_t layouts = 0;
  size_t laidOutGlyphs = 0, lastDrawGlyphs = 0, lastDrawRuns = 0;
};
class RichView {
 public:
  RichText text;
  lv_obj_t* object = nullptr;
  size_t caret = 0, anchor = 0;
  uint8_t typingStyle = 0;
  uint32_t typingForeground = NoColor, typingBackground = NoColor;
  std::string documentPath;
  SyntaxLanguage syntax=SyntaxLanguage::Auto;
  SyntaxState syntaxInitial{};
  bool syntaxReady=true;
  bool lineNumbers=false,overwrite=false;
  size_t firstLine=1;
  bool editing = false, readOnly = false, singleLine = false, sourceMode = false;
  uint16_t fontSize = 24;
  void* context = nullptr;
  const lv_font_t* (*font)(uint16_t, uint8_t) = nullptr;
  bool (*changed)(void*) = nullptr;
  void (*activated)(void*) = nullptr;
  void (*linked)(void*, const char*) = nullptr;
  void (*edge)(void*, int) = nullptr;
  void (*blockActivated)(void*, size_t) = nullptr;
  bool (*imageLoad)(const char*, int, int, lv_image_dsc_t*, void**) = nullptr;
  void (*imageFree)(void*) = nullptr;
  const char* (*translate)(const char*) = nullptr;
  void begin(lv_obj_t* parent, bool readonly = false);
  void load(const std::string& source, bool fenced = false);
  // Geometry-only callers pass false: resizing the other pane or an unchanged
  // parent must not parse/layout this pane again.
  void reflow(bool contentsChanged = true);
  void setTheme(const ViewTheme& value);
  void recolorSyntax();
  const ViewMetrics& metrics() const { return performance; }
  void insert(const char* value);
  void format(uint8_t bit);
  void color(uint32_t value, bool background);
  void wakeCaret();
  bool caretVisible() const { return editing && blinkOn; }
  void releaseImages();
  // Release only reconstructible geometry/images. Text and editing state stay.
  void discardLayout();
  void resumeLayout();
  void heading();
  void move(size_t position, bool shift = false);
  void key(const KeyEvent& event);
  std::string selected() const;
  void blur();
  size_t hit(int x, int y) const;

 private:
  struct Box {
    int x, y, w, h;
    const lv_font_t* font;
  };
  ExternalVector<Box> boxes;
  // Pointers are rebuilt with the boxes after any model mutation. Newlines
  // have a null pointer. Drawing never performs RichText's line-by-line lookup.
  ExternalVector<const RichGlyph*> glyphs;
  struct Row {
    size_t first, last;
    int y, h;
  };
  ExternalVector<Row> rows;
  struct Prefix {
    size_t index;
    std::string text;
  };
  ExternalVector<Prefix> prefixes;
  struct Metric {
    const lv_font_t* font = nullptr;
    uint32_t code = 0;
    int width = 0;
  };
  ExternalVector<Metric> widths;
  int glyphWidth(const lv_font_t* font, uint32_t code);
  RichGlyph glyphAt(size_t index) const;
  ViewTheme theme;
  ViewMetrics performance;
  std::string loadedSource, loadedPath;
  SyntaxLanguage loadedSyntax=SyntaxLanguage::Auto;
  SyntaxState loadedSyntaxInitial{};
  bool loadedSyntaxReady=true;
  bool sourceCached = false, loadedFenced = false, loadedSourceMode = false, layingOut = false;
  bool layoutSuspended = false;
  uint16_t layoutFontSize = 0;
  const lv_font_t* (*layoutFont)(uint16_t, uint8_t) = nullptr;
  struct CellBox {
    lv_area_t area;
    struct GlyphBox {
      RichGlyph glyph;
      Box box;
    };
    ExternalVector<GlyphBox> glyphs;
    uint8_t align;
    bool header;
  };
  struct BlockBox {
    size_t index;
    int y, w, h;
    uint8_t kind;
    ExternalVector<CellBox> cells;
    int image = -1;
    std::string caption;
  };
  struct Image {
    std::string path;
    lv_image_dsc_t dsc{};
    void* handle = nullptr;
  };
  ExternalVector<BlockBox> blocks;
  ExternalVector<Image> images;
  lv_timer_t* blinkTimer = nullptr;
  bool blinkOn = true;
  static void blink(lv_timer_t* timer);
  void invalidateCaret();
  int imageFor(const std::string& path);
  struct LineNumber {int y;size_t value;};
  ExternalVector<LineNumber> numbers;
  int gutter=0;
  int layoutWidth = 0;
  size_t pressIndex = 0;
  lv_point_t pressPoint{};
  bool selecting = false, dragged = false, bottomAtPress = false;
  static void event(lv_event_t* e);
  void draw(lv_layer_t* layer);
  void commit();
  void reveal();
};
}  // namespace writing
