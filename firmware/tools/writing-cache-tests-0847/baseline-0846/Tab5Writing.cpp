#include "Tab5Serial.h"
#include "Tab5SerialText.h"
#include "Tab5VolumeFS.h"
#include "Tab5Storage.h"
#include "Tab5Writing.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "Tab5Memory.h"
#endif
#include "Tab5PathLabel.h"
#include "Tab5Icons.h"

#include <Arduino.h>
#include <FS.h>
#include <SD.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <new>

#include "Tab5FilePolicy.h"
#include "Tab5Focus.h"
#include "Tab5TextInput.h"
#include "Tab5ContextMenu.h"
#include "Tab5DirectoryView.h"
#include "Tab5UsbStorage.h"
#include "Tab5UiStyle.h"
#include "WriterDocument.h"
#include "WriterIndex.h"
#include "WriterStorage.h"
#include "WriterUsb.h"
#include "WriterView.h"
#include "WriterSyntax.h"
#include "WriterSyntaxIndex.h"
#include "WriterJson.h"
#include "WriterWorkspace.h"
#include "Tab5WriterWorkspace.h"
#include "src/draftling/md_parser.h"

namespace {
constexpr int DefaultWidth = 1216, FullHeight = 672;
constexpr size_t PageBytes = 4096, MaxRows = 64;
class SdFileSystem : public writing::FileSystem {
  File handle;
  bool systemStore = false;

 public:
  explicit SdFileSystem(bool internalStore = false) : systemStore(internalStore) {}
  bool opened() { return static_cast<bool>(handle); }
  bool writable(const char* p) const { return systemStore || Tab5Files::userWritable(p); }
  bool prepareBackup(const char* path) override {
    String dir;for(const char* p=path;*p;++p){if(*p=='/'&&dir.length()&&!tab5FS.exists(dir.c_str())&&!tab5FS.mkdir(dir.c_str()))return false;dir+=*p;}return true;
  }
  bool exists(const char* p) override { return tab5FS.exists(p); }
  bool probe(const char* p,bool& present) override { errno=0;present=tab5FS.exists(p);return present||errno==ENOENT; }
  bool remove(const char* p) override { return writable(p) && tab5FS.remove(p); }
  bool rename(const char* a, const char* b) override {
    return writable(a) && writable(b) && tab5FS.rename(a, b);
  }
  bool openWrite(const char* p) override {
    if (!writable(p)) return false;
    handle = tab5FS.open(p, FILE_WRITE);
    return static_cast<bool>(handle);
  }
  bool openRead(const char* p) override {
    std::string normalized;
    if (!p || !Tab5Files::canonical(p, normalized) || normalized == "/") return false;
    handle = tab5FS.open(p, FILE_READ);
    return handle && !handle.isDirectory();
  }
  size_t length() override { return handle.size(); }
  size_t write(const char* p, size_t n) override {
    return handle.write(reinterpret_cast<const uint8_t*>(p), n);
  }
  size_t read(char* p, size_t n) override { return handle.read(reinterpret_cast<uint8_t*>(p), n); }
  bool finishWrite() override {
    if(!handle)return false;
    errno=0;handle.flush();const bool flushed=!errno&&!handle.getWriteError();
    errno=0;handle.close();return flushed&&!errno;
  }
  void close() override {
    if (handle) {
      handle.flush();
      handle.close();
    }
  }
};
String parentOf(const String& s) {
  int slash = s.lastIndexOf('/');
  return slash <= 0 ? String("/") : s.substring(0, slash);
}
String baseOf(const String& s) { return s.substring(s.lastIndexOf('/') + 1); }
String join(const String& a, const String& b) { return a == "/" ? a + b : a + "/" + b; }
bool textFile(String name) {
  name.toLowerCase();
  return writing::editableFile(name.c_str());
}
bool imageFile(String name) {
  name.toLowerCase();
  return name.endsWith(".png") || name.endsWith(".jpg") || name.endsWith(".jpeg") ||
         name.endsWith(".bmp");
}
std::string urlPath(const String& path) {
  std::string out;
  const char* hex = "0123456789ABCDEF";
  for (unsigned char c : std::string(path.c_str()))
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' ||
        c == '.' || c == '-' || c == '_')
      out += char(c);
    else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  return out;
}
void yieldSd() { delay(1); }
size_t viewEnd(const writing::Document& doc, size_t start) {
  size_t end = doc.pageEnd(start, PageBytes), rows = 0;
  if (end < doc.size() && end > start && doc.at(end - 1) != '\n') {
    while (end < doc.size() && doc.at(end) != '\n' && end - start < 32768) end = doc.nextChar(end);
    if (doc.at(end) == '\n') ++end;
  }
  for (size_t p = start; p < end; ++p)
    if (doc.at(p) == '\n' && ++rows >= MaxRows) return p + 1;
  return end;
}
size_t previousView(const writing::Document& doc, size_t start) {
  // Bound backwards navigation too; walking all previous pages is quadratic
  // when returning through a large document.
  size_t p = start, rows = 0, boundary = start;
  while (p && start - p < PageBytes && rows < MaxRows) {
    p = doc.prevChar(p);
    if (doc.at(p) == '\n') {
      if (p + 1 < start) boundary = p + 1;
      ++rows;
    }
  }
  p = !p ? 0 : boundary < start ? boundary : p;
  // Do not reopen the middle of a GFM table without its header. The same
  // 32 KiB limit as forward rendering bounds this exceptional extension.
  auto hasPipe = [&](size_t from) {
    while (from < start && doc.at(from) != '\n') {
      if (doc.at(from++) == '|') return true;
    }
    return false;
  };
  if (hasPipe(p)) {
    while (p && start - p < 32768) {
      size_t prev = p - 1;
      while (prev && doc.at(prev - 1) != '\n' && start - prev < 32768) --prev;
      if (prev == p || (prev && doc.at(prev - 1) != '\n') || !hasPipe(prev)) break;
      p = prev;
    }
  }
  return p;

}
}  // namespace

struct Tab5WritingApp::Impl {
  static Impl* instance;
  WritingHooks hooks;
  writing::Workspace workspace;
  writing::WorkspaceScanner workspaceScanner;
  SdFileSystem workspaceFs{true};
  uint8_t currentDocumentProject = 0, resumeDocumentProject = 0;
  size_t documentsPage = 0, documentsMenuCount = 0;
  uint32_t lastWorkspaceSave = 0;
  bool projectNameDialog = false;
  lv_style_t panelStyle{}, buttonStyle{}, mutedStyle{}, referenceStyle{}, selectedStyle{}, fieldStyle{};
  bool stylesReady = false;
  struct Caption { lv_obj_t* object; std::string source; bool placeholder; };
  std::vector<Caption> captions;
  lv_obj_t *menuShade = nullptr, *menuPanel = nullptr, *menuOrigin = nullptr;
  lv_obj_t* menuButtons[8]{};
  int openMenu = 0, lastLayoutHeight = -1, lastLayoutWidth = -1;
  int Width = DefaultWidth;
  bool lastLayoutSplit = false, lastLayoutConsole = false, menuWasEditing = false;
  bool consoleVisible = false, runAfterSave = false, codeDraft = false;
  const char* draftExtension = ".py";
  writing::SyntaxLanguage syntaxChoice=writing::SyntaxLanguage::Auto;
  writing::SyntaxIndex syntaxIndex,referenceSyntaxIndex;
  String encodingOpenPath; bool encodingOpenReference = false; int encodingOpenProject = -1;
  lv_obj_t *consolePane = nullptr, *consoleText = nullptr;
  char* consoleCache = nullptr;
  bool consoleAllocationReported = false;
  uint32_t lastConsole = 0;
  String warnedPath, cachedTitlePath, cachedTitle;
  size_t cachedTitleCursor = SIZE_MAX, cachedTitleCount = SIZE_MAX, cachedTitleHeading = SIZE_MAX;
  bool cachedTitleDirty = false, cachedTitleSplit = false;

  writing::Document document, reference;
  writing::RichView view, refView;
  std::string pageSource;
  writing::DocumentIndex mainIndex, referenceIndex;
  std::vector<writing::Heading>& mainHeadings = mainIndex.titles;
  std::vector<writing::Heading>& refHeadings = referenceIndex.titles;
  bool pageLiteral = false;
  std::vector<String> references;
  bool headingsDirty = true;
  int deferredEdge = 0;
  lv_obj_t *boldButton = nullptr, *italicButton = nullptr, *proxy = nullptr, *mainPrev = nullptr,
           *mainNext = nullptr;
  SdFileSystem fs;
  writing::SaveJob saveJob{fs};
  SdFileSystem resumeFs;
  writing::LoadJob resumeJob{resumeFs};
  String resumePaths[2];
  size_t resumeStart = 0, resumeCursor = 0;
  unsigned resumePhase = 0;
  lv_obj_t* resumeProgress = nullptr;
  unsigned outlineAfterIndex = 0; // 0=none, 1=left, 2=right
  SdFileSystem sessionFs{true};
  writing::Document sessionDocument;
  writing::SaveJob sessionJob{sessionFs};
  String pendingSession, savedSession;
  bool sessionObserved = true;
  uint32_t sessionRetryDelay = 5000;
  lv_obj_t *printButton = nullptr;
  lv_obj_t *root = nullptr, *toolbar = nullptr, *body = nullptr, *mainPane = nullptr,
           *refPane = nullptr;
  lv_obj_t *mainScroll = nullptr, *refScroll = nullptr, *status = nullptr, *filename = nullptr,
           *referenceName = nullptr, *mainPathButton = nullptr, *refPathButton = nullptr;
  lv_obj_t *modal = nullptr, *dialogInput = nullptr, *replaceInput = nullptr, *modalList = nullptr,
           *modalTitle = nullptr;
  std::unique_ptr<Tab5DirectoryView> browser;
  lv_obj_t *browseMedia = nullptr, *browseMediaButton = nullptr;
  unsigned browseMediaState = 0;
  lv_obj_t* target = nullptr;
  bool documentSelection=false;
  size_t selectionAnchor=0,selectionCaret=0;
  size_t start = 0, end = 0, refStart = 0, refEnd = 0, cursor = 0;
  size_t lastWords = 0, lastCharacters = 0;
  String path, referencePath, pendingPath, browseDir = "/", message;
  uint32_t lastEdit = 0, lastAttempt = 0, lastStatus = 0, lastSession = 0;
  bool lineNumbers=false;
  bool built = false, visible = false, split = false, keyboard = false;
  bool layoutReclaimed = false;
  int32_t reclaimedMainScroll = 0, reclaimedRefScroll = 0;
  bool printAfterSave = false;
  bool saveObserved = false, sessionDirty = false, statsDirty = true, resumeTried = false;
  int height = FullHeight, browseMode = 0;
  uint16_t fontSize = 24;
  std::vector<std::vector<std::string>> tableCells, tableOriginal;
  std::vector<uint8_t> tableAlignment;
  struct CellEditor {
    Impl* app;
    size_t row, col;
    writing::RichView editor;
  };
  std::vector<std::unique_ptr<CellEditor>> tableFields;
  size_t activeCell = 0;
  lv_obj_t *cellBold = nullptr, *cellItalic = nullptr, *colorPanel = nullptr;
  uint32_t chosenColors[2] = {0x376b54, 0xf4c542};
  lv_obj_t* colorSwatches[2][12]{};
  lv_obj_t* colorPreview = nullptr;
  bool colorsOpen = false;
  writing::RichView& activeView() {
    return tableGrid && activeCell < tableFields.size() ? tableFields[activeCell]->editor : view;
  }
  lv_obj_t* tableGrid = nullptr;
  size_t blockStart = 0, blockEnd = 0;
  bool clipboardMarkdown = false;
  enum Action {
    New = 1,
    Open,
    Save,
    SaveAs,
    Undo,
    Redo,
    Search,
    Outline,
    Reference,
    Mode,
    Keyboard,
    Layout,
    Prev,
    Next,
    RefPrev,
    RefNext,
    RefClose,
    Underline, Strike, LineNumbers,
    Bold,
    Italic,
    Heading,
    Copy,
    Cut,
    Paste,
    Smaller,
    Larger,
    CloseDialog,
    ParentFolder,
    BrowseStorage,
    ChooseName,
    FindNext,
    ReplaceOne,
    ReplaceAll,
    RestoreBackup,
    RefOutline,
    Table,
    Image,
    Colors,
    ApplyTextColor,
    ApplyBackgroundColor,
    Print,
    TableApply,
    TableAddRow,
    TableAddColumn,
    TableRemoveRow,
    TableRemoveColumn,
    SelectAll,
    Help,
    CodeNew, CodeRun, CodeStop, CodeResume, CodeLibraries, CodeConsole, CodeClearConsole,
    Documents, ProjectNew, ProjectCreate, ProjectAdd, ProjectImport, ProjectUseFolder, DocumentsNext,
    SerialToggle, SerialListen, SerialStop, SerialSend, SerialCancel, SerialClear, SerialInsert, SerialData,
    TextEncodingDialog, TextReopenEncoding, CodeNewC, CodeNewJson, CodeValidateJson, CodeLanguage,
    MenuFile = 100,
    MenuEdit,
    MenuInsert,
    MenuFormat,
    MenuView,
    MenuHelp,
    MenuDocuments,
    MenuCode,
    EncodingUtf8 = 200, EncodingUtf8Bom, EncodingUtf16LE, EncodingUtf16BE, EncodingWindows1252
  };
  #include "Tab5WritingSerial.inc"
  struct WorkspaceRow { Impl* app; size_t index; bool remove; };
  struct Row {
    Impl* app;
    String path;
    bool directory;
    size_t offset;
    bool heading;
    bool ref = false;
  };

  const char* tr(const char* source) const {
    return hooks.translate ? hooks.translate(source) : source;
  }
  uint32_t color(WritingColor role) const {
    if (hooks.color) return hooks.color(role);
    switch (role) {
      case WritingColor::Background: return 0x101214;
      case WritingColor::Surface: return 0x191c20;
      case WritingColor::Text: return 0xf1f3f5;
      case WritingColor::Muted: return 0x9ca3aa;
      case WritingColor::Accent: return 0x86baa7;
      case WritingColor::Selection: return 0x315246;
      case WritingColor::Reference: return 0x1a2327;
    }
    return 0;
  }
  writing::ViewTheme viewTheme() const {
    writing::ViewTheme theme;
    theme.foreground = color(WritingColor::Text);
    theme.muted = color(WritingColor::Muted);
    theme.accent = color(WritingColor::Accent);
    theme.surface = color(WritingColor::Surface);
    theme.selection = color(WritingColor::Selection);
    theme.border = color(WritingColor::Muted);
    return theme;
  }
  static void setLabel(lv_obj_t* label, const char* value) {
    if (label && strcmp(lv_label_get_text(label), value)) lv_label_set_text(label, value);
  }
  void caption(lv_obj_t* object, const char* source, bool placeholder = false) {
    if (placeholder) lv_textarea_set_placeholder_text(object, tr(source));
    else setLabel(object, tr(source));
    captions.push_back({object, source, placeholder});
    lv_obj_add_event_cb(object, [](lv_event_t* event) {
      auto* app = static_cast<Impl*>(lv_event_get_user_data(event));
      auto* object = static_cast<lv_obj_t*>(lv_event_get_target(event));
      auto& labels = app->captions;
      labels.erase(std::remove_if(labels.begin(), labels.end(),
                                 [object](const Caption& c) { return c.object == object; }), labels.end());
    }, LV_EVENT_DELETE, this);
  }
  void appearance() {
    if(browser)browser->appearance(font(22));
    if (!stylesReady) {
      for (auto* style : {&panelStyle, &buttonStyle, &mutedStyle, &referenceStyle, &selectedStyle, &fieldStyle})
        lv_style_init(style);
      stylesReady = true;
    }
    lv_style_set_bg_color(&panelStyle, lv_color_hex(color(WritingColor::Background)));
    lv_style_set_text_color(&panelStyle, lv_color_hex(color(WritingColor::Text)));
    lv_style_set_bg_color(&buttonStyle, lv_color_hex(color(WritingColor::Surface)));
    lv_style_set_text_color(&buttonStyle, lv_color_hex(color(WritingColor::Text)));
    lv_style_set_text_color(&mutedStyle, lv_color_hex(color(WritingColor::Muted)));
    lv_style_set_bg_color(&referenceStyle, lv_color_hex(color(WritingColor::Reference)));
    lv_style_set_bg_color(&selectedStyle, lv_color_hex(color(WritingColor::Selection)));
    lv_style_set_text_color(&selectedStyle, lv_color_hex(color(WritingColor::Text)));
    lv_style_set_bg_color(&fieldStyle, lv_color_hex(color(WritingColor::Surface)));
    lv_style_set_text_color(&fieldStyle, lv_color_hex(color(WritingColor::Text)));
    lv_style_set_border_color(&fieldStyle, lv_color_hex(color(WritingColor::Muted)));
    for (auto* style : {&panelStyle, &buttonStyle, &mutedStyle, &referenceStyle, &selectedStyle, &fieldStyle})
      lv_obj_report_style_change(style);
    for (const auto& item : captions) {
      if (item.placeholder) lv_textarea_set_placeholder_text(item.object, tr(item.source.c_str()));
      else setLabel(item.object, tr(item.source.c_str()));
    }
    if(printButton&&lv_obj_is_valid(printButton)){lv_obj_clean(printButton);tab5PrinterIcon(printButton,lv_color_hex(color(WritingColor::Text)),lv_color_hex(color(WritingColor::Surface)));Tab5Focus.describe(printButton,tr("Imprimer · Ctrl+P"));}
    view.translate = refView.translate = hooks.translate;
    view.setTheme(viewTheme());
    refView.setTheme(viewTheme());
    for (auto& cell : tableFields) {
      cell->editor.translate = hooks.translate;
      cell->editor.setTheme(viewTheme());
    }
    updateCellSelection();
    cachedTitleCursor = SIZE_MAX;
    if (built) {
      updateStatus();
      serialService(millis(),true);
      if (split) setLabel(referenceName, breadcrumb(referencePath, refHeadings, refStart, 254).c_str());
    }
  }
  bool isCode() const { return syntaxChoice!=writing::SyntaxLanguage::Auto || codeDraft || writing::sourceFile(path.c_str()); }
  const char* draftName() const { return !strcmp(draftExtension,".c")?"program.c":!strcmp(draftExtension,".json")?"data.json":"script.py"; }
  const char* syntaxPath() const { return path.length()?path.c_str():codeDraft?draftName():"document.md"; }
  writing::SyntaxLanguage resolvedSyntax() const { return syntaxChoice==writing::SyntaxLanguage::Auto?writing::syntaxLanguage(syntaxPath()):syntaxChoice; }
  bool isJson() const { return resolvedSyntax()==writing::SyntaxLanguage::Json; }
  bool canRunPython() const { return writing::pythonFile(syntaxPath())&&resolvedSyntax()==writing::SyntaxLanguage::Python; }
  String defaultExtension() const { if(codeDraft)return draftExtension;const int dot=path.lastIndexOf('.');return dot>path.lastIndexOf('/')+1&&writing::editableFile(path.c_str())?path.substring(dot):String(isCode()?".txt":".md"); }
  void releaseConsole() {
    writing::release(consoleCache);consoleCache=nullptr;
    if(consoleText)lv_label_set_text_static(consoleText,"");
    consoleAllocationReported=false;
  }
  void serviceConsole(uint32_t now, bool force = false) {
    if (!visible || !consoleVisible || !consoleText || Tab5TextInput::hasSelection(consoleText) || (!force && now - lastConsole < 250)) return;
    lastConsole = now;
    if(!consoleCache)consoleCache=static_cast<char*>(writing::allocate(8192));
    if(!consoleCache){
      if(!consoleAllocationReported)note("Mémoire insuffisante pour la console");
      consoleAllocationReported=true;return;
    }
    consoleAllocationReported=false;consoleCache[0]=0;
    if(hooks.console)hooks.console(consoleCache,8192);
    consoleCache[8191]=0;
    if(!strcmp(lv_label_get_text(consoleText),consoleCache))return;
    lv_label_set_text(consoleText,consoleCache);
    lv_obj_scroll_to_y(consolePane, LV_COORD_MAX, LV_ANIM_OFF);
  }
  void runCode() {
    if (!canRunPython()) { note("Exécution réservée aux fichiers Python (.py)"); return; }
    if (document.size() > 65536) { note("Script trop grand : limite d’exécution de 64 Kio"); return; }
    if (!save("", true)) return;
    if (document.dirty()) { note("Enregistrez le script avant de l’exécuter"); return; }
    if (!hooks.run || !hooks.run(path.c_str())) { note("Exécution Python indisponible"); return; }
    consoleVisible = true;
    layout();
    serviceConsole(millis(), true);
    // Use the same foreground path as the OS Resume control, after all editor
    // updates, so its keyboard/focus cannot cover the newly launched app.
    if (hooks.resume) hooks.resume();
  }
  bool persistWorkspace() {
    if (!workspace.dirty()) return true;
    tab5FS.mkdir("/System");
    if (workspace.save(workspaceFs, yieldSd)) return true;
    note("Liste des documents non enregistrée : vérifiez la carte SD");
    return false;
  }
  void capturePosition() {
    if (!path.length()) return;
    const int i = workspace.find(path.c_str(), currentDocumentProject);
    if (i < 0) return;
    auto* entry = workspace.entry(static_cast<size_t>(i));
    const uint32_t anchor = static_cast<uint32_t>(start + view.text.sourceAt(view.anchor));
    const uint32_t caret = static_cast<uint32_t>(start + view.text.sourceAt(view.caret));
    const int32_t scroll = mainScroll ? std::max<int32_t>(0, lv_obj_get_scroll_y(mainScroll)) : 0;
    if (entry->start == start && entry->cursor == caret && entry->anchor == anchor && entry->scroll == scroll && entry->syntaxLanguage == uint8_t(syntaxChoice)) return;
    entry->start = static_cast<uint32_t>(start); entry->cursor = caret; entry->anchor = anchor; entry->scroll = scroll; entry->syntaxLanguage=uint8_t(syntaxChoice);
    workspace.markDirty();
  }
  void restorePosition() {
    const int i = workspace.find(path.c_str(), currentDocumentProject);
    if (i < 0) return;
    const auto saved = *workspace.entry(static_cast<size_t>(i));
    syntaxChoice=writing::SyntaxLanguage(saved.syntaxLanguage);
    start = saved.start <= document.size() && document.boundary(saved.start) ? saved.start : 0;
    cursor = saved.cursor <= document.size() && document.boundary(saved.cursor) ? saved.cursor : start;
    render();
    view.anchor = view.text.indexAtSource(saved.anchor >= start ? std::min<size_t>(saved.anchor - start, end - start) : 0);
    view.caret = view.text.indexAtSource(cursor >= start ? std::min<size_t>(cursor - start, end - start) : 0);
    lv_obj_scroll_to_y(mainScroll, std::max<int32_t>(0, saved.scroll), LV_ANIM_OFF);
  }
  void fillDocumentsMenu() {
    documentsMenuCount = workspace.count();
    auto* groups = lv_dropdown_create(menuPanel);
    String names = tr("Hors projet"); uint32_t selected = 0;
    for (size_t i = 0; i < workspace.projectCount(); ++i) {
      auto* p = workspace.project(i); names += "\n"; names += p->name;
      if (p->id == workspace.activeProject()) selected = i + 1;
    }
    lv_dropdown_set_options(groups, names.c_str()); lv_dropdown_set_selected(groups, selected);
    lv_obj_set_size(groups, 374, 40); lv_obj_add_style(groups, &buttonStyle, 0);
    lv_obj_add_event_cb(groups, [](lv_event_t* e) {
      auto* a = static_cast<Impl*>(lv_event_get_user_data(e));
      auto n = lv_dropdown_get_selected(static_cast<lv_obj_t*>(lv_event_get_target(e)));
      a->workspace.setActiveProject(n && n <= a->workspace.projectCount() ? a->workspace.project(n - 1)->id : 0);
      a->documentsPage = 0; a->persistWorkspace(); a->closeMenu(false); a->toggleMenu(MenuDocuments);
    }, LV_EVENT_VALUE_CHANGED, this);
    size_t seen = 0, shown = 0, matching = 0;
    for (size_t i = 0; i < workspace.count(); ++i) if (workspace.entry(i)->projectID == workspace.activeProject()) ++matching;
    if (documentsPage >= matching) documentsPage = 0;
    for (size_t i = 0; i < workspace.count(); ++i) {
      auto* entry = workspace.entry(i);
      if (entry->projectID != workspace.activeProject() || seen++ < documentsPage) continue;
      if (shown++ >= 16) break;
      auto* row = lv_obj_create(menuPanel); panel(row); lv_obj_set_size(row,374,40);
      auto* choose = lv_button_create(row); panel(choose); lv_obj_add_style(choose,&buttonStyle,0);
      lv_obj_set_size(choose,326,40);
      if (writing::Workspace::samePath(path.c_str(),entry->path) && currentDocumentProject == entry->projectID) lv_obj_add_style(choose,&selectedStyle,0);
      label(choose, entry->path, 6, 6, 312, false);
      auto* close = lv_button_create(row); panel(close); lv_obj_set_pos(close,330,0); lv_obj_set_size(close,40,40);
      auto* x = lv_label_create(close); lv_label_set_text(x,LV_SYMBOL_CLOSE); lv_obj_center(x);
      Tab5Focus.describe(close,tr("Retirer de la liste ; le fichier reste sur la carte SD"));
      for (unsigned remove = 0; remove < 2; ++remove) {
        auto* context = new (std::nothrow) WorkspaceRow{this,i,remove != 0}; if (!context) continue;
        auto* target = remove ? close : choose;
        lv_obj_add_event_cb(target, [](lv_event_t* e) { delete static_cast<WorkspaceRow*>(lv_event_get_user_data(e)); }, LV_EVENT_DELETE,context);
        lv_obj_add_event_cb(target, [](lv_event_t* e) {
          const auto r = *static_cast<WorkspaceRow*>(lv_event_get_user_data(e));
          if (r.remove) r.app->removeDocument(r.index);
          else { const auto saved = *r.app->workspace.entry(r.index); r.app->closeMenu(false); r.app->load(saved.path,false,saved.projectID); }
        },LV_EVENT_CLICKED,context);
      }
    }
    if (!matching) label(menuPanel,"Aucun document dans ce groupe",0,0,360);
    if (matching > 16) button(menuPanel,"Documents suivants",DocumentsNext,374);
  }
  void cycleDocument(int direction) {
    size_t count = workspace.count(); if (!count) return;
    int current = workspace.find(path.c_str(),currentDocumentProject);
    for (size_t distance = 1; distance <= count; ++distance) {
      const size_t i = (static_cast<size_t>(current < 0 ? (direction > 0 ? count - 1 : 0) : current) + count + direction * static_cast<int>(distance)) % count;
      const auto item = *workspace.entry(i);
      if (item.projectID == workspace.activeProject()) { load(item.path,false,item.projectID); return; }
    }
  }
  void removeDocument(size_t index) {
    const auto* item = workspace.entry(index); if (!item) return;
    const bool current = writing::Workspace::samePath(path.c_str(),item->path) && currentDocumentProject == item->projectID;
    if (current && !beforeSwitch()) return;
    closeMenu(false);
    if (!workspace.remove(index)) { note(workspace.error()); return; }
    if (!persistWorkspace()) { workspace.load(workspaceFs); return; }
    if (current) {
      writing::Document empty; document.swap(empty); documentSelection=false;path = ""; codeDraft = false;syntaxChoice=writing::SyntaxLanguage::Auto;syntaxIndex.reset(); start = cursor = 0;
      headingsDirty = statsDirty = sessionDirty = true; view.blur(); render();
      // The committed workspace no longer contains this membership. A stale
      // pre-cut session is consequently rejected even before this clear saves.
      saveSession();
    }
    toggleMenu(MenuDocuments);
  }
  void projectDialog() {
    if (!beforeSwitch()) return;
    dialog("Créer un projet avec le document courant");
    projectNameDialog = true;
    dialogInput = field(modal,"Nom du projet",20,62,740);
    target = dialogInput;
    auto* b = button(modal,"Créer",ProjectCreate,170); lv_obj_set_pos(b,780,64);
    Tab5Focus.focusObject(dialogInput);
  }
  void addToProject() {
    if (!workspace.activeProject()) { note("Choisissez d’abord un projet dans Documents"); return; }
    if (!save("",true)) return;
    capturePosition();
    const int added = workspace.add(path.c_str(),workspace.activeProject());
    if (added < 0) { note("Liste pleine : 256 documents au maximum"); return; }
    const uint8_t previousProject = currentDocumentProject;
    currentDocumentProject = workspace.activeProject(); capturePosition();
    if (!persistWorkspace()) { currentDocumentProject = previousProject; return; }
    sessionDirty = true;
    note("Document ajouté au projet");
  }
  void note(const char* text) {
    message = text;
    if (hooks.notice) hooks.notice(tr(text));
    updateStatus();
  }
  void warnSystem(const String& file) {
    if (file != warnedPath && Tab5Files::protectedPath(file.c_str())) {
      warnedPath = file;
      note("Fichier système : vos modifications peuvent changer le fonctionnement de Tab5OS.");
    }
  }
  const lv_font_t* font(uint16_t size = 24, uint8_t flags = 0) {
    return hooks.font ? hooks.font(size, flags) : &lv_font_montserrat_24;
  }
  void panel(lv_obj_t* obj, bool scroll = false) {
    lv_obj_add_style(obj, &panelStyle, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    if (!scroll) lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  }
  lv_obj_t* button(lv_obj_t* parent, const char* text, int action, int w = 48, bool icon = false) {
    lv_obj_t* b = lv_button_create(parent);
    lv_obj_set_size(b, w, 40);
    lv_obj_set_style_radius(b, 8, 0);
    lv_obj_add_style(b, &buttonStyle, 0);
    lv_obj_add_style(b, &selectedStyle, LV_STATE_CHECKED);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 3, 0);
    lv_obj_set_style_opa(b, LV_OPA_50, LV_STATE_DISABLED);
    lv_obj_t* l = lv_label_create(b);
    if (icon) lv_label_set_text(l, text);
    else caption(l, text);
    lv_obj_center(l);
    lv_obj_set_style_text_font(l, icon ? &lv_font_montserrat_24 : font(20), 0);
    lv_obj_add_event_cb(
        b,
        [](lv_event_t* e) {
          if (instance)
            instance->act(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e))));
        },
        LV_EVENT_CLICKED, reinterpret_cast<void*>(static_cast<intptr_t>(action)));
    Tab5Focus.describe(b, tr(actionName(action)));
    return b;
  }
  lv_obj_t* label(lv_obj_t* parent, const char* text, int x, int y, int w, bool translate = true) {
    lv_obj_t* l = lv_label_create(parent);
    if (translate) caption(l, text);
    else lv_label_set_text(l, text);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_width(l, w);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(l, font(20), 0);
    lv_obj_set_height(l, font(20)->line_height + 2);
    return l;
  }
  void showKeyboard(bool on, lv_obj_t* obj = nullptr) {
    keyboard = on;
    if (obj) target = obj;
    if (hooks.keyboard) hooks.keyboard(on, obj ? obj : target);
  }
  const char* actionName(int action) {
    switch (action) {
      case Table:
        return "Insérer / modifier un tableau · Ctrl+T";
      case Image:
        return "Importer une image SD · Ctrl+Maj+I";
      case Colors:
        return "Couleur du texte et du fond · Ctrl+Maj+C";
      case TableApply:
        return "Appliquer le tableau";
      case TableAddRow:
        return "Ajouter une ligne";
      case TableRemoveRow:
        return "Retirer la dernière ligne";
      case TableAddColumn:
        return "Ajouter une colonne";
      case TableRemoveColumn:
        return "Retirer la dernière colonne";
      case New:
        return "Nouveau document · Ctrl+N";
      case Open:
        return "Ouvrir · Ctrl+O";
      case Save:
        return "Enregistrer · Ctrl+S";
      case SaveAs:
        return "Enregistrer sous · Ctrl+Maj+S";
      case Undo:
        return "Annuler · Ctrl+Z";
      case Redo:
        return "Rétablir · Ctrl+Y";
      case Search:
        return "Rechercher · Ctrl+F";
      case Outline:
        return "Plan du document";
      case Reference:
        return "Ouvrir à droite";
      case Bold:
        return "Gras · Ctrl+B";
      case Italic:
        return "Italique · Ctrl+I";
      case Heading:
        return "Titre : normal / H1 / H2 / H3";
      case Copy:
        return "Copier · Ctrl+C";
      case Cut:
        return "Couper · Ctrl+X";
      case Paste:
        return "Coller · Ctrl+V";
      case Keyboard:
        return "Clavier tactile";
      case Prev:
      case RefPrev:
        return "Titre précédent";
      case Next:
      case RefNext:
        return "Titre suivant";
      case RefOutline:
        return "Titres du document de droite";
      case RestoreBackup:
        return "Restaurer la sauvegarde précédente";
      default:
        return "Entrée : activer · Échap : revenir";
    }
  }
  void closeMenu(bool restoreFocus) {
    if (!menuShade) return;
    auto* old = menuShade;
    auto* origin = menuOrigin;
    menuShade = menuPanel = menuOrigin = nullptr;
    openMenu = 0;
    boldButton = italicButton = nullptr;
    Tab5Focus.clear();
    lv_obj_delete(old);
    view.editing = menuWasEditing;
    if (view.editing) view.wakeCaret();
    if (restoreFocus && !view.editing && origin && lv_obj_is_valid(origin)) Tab5Focus.focusObject(origin);
  }
  void toggleMenu(int which) {
    if (openMenu == which) { closeMenu(true); return; }
    const bool editing = menuShade ? menuWasEditing : view.editing;
    closeMenu(false);
    menuWasEditing = editing;
    view.blur();  // Selection and typing style survive while menu commands have focus.
    menuOrigin = menuButtons[which - MenuFile];
    openMenu = which;
    menuShade = lv_obj_create(root);
    lv_obj_set_style_bg_opa(menuShade, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(menuShade, 0, 0);
    lv_obj_set_style_pad_all(menuShade, 0, 0);
    lv_obj_set_style_radius(menuShade, 0, 0);
    lv_obj_remove_flag(menuShade, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(menuShade, 0, 40);
    lv_obj_set_size(menuShade, Width, std::max(60, height - 40));
    lv_obj_add_event_cb(menuShade, [](lv_event_t* e) {
      auto* app = static_cast<Impl*>(lv_event_get_user_data(e));
      if (lv_event_get_target(e) == app->menuShade) app->closeMenu(true);
    }, LV_EVENT_CLICKED, this);
    menuPanel = lv_obj_create(menuShade);
    panel(menuPanel, true);
    lv_obj_add_style(menuPanel, &buttonStyle, 0);
    lv_obj_set_style_border_width(menuPanel, 1, 0);
    lv_obj_set_style_border_color(menuPanel, lv_color_hex(color(WritingColor::Muted)), 0);
    lv_obj_set_style_radius(menuPanel, 8, 0);
    lv_obj_set_style_pad_all(menuPanel, 6, 0);
    lv_obj_set_style_pad_row(menuPanel, 2, 0);
    lv_obj_set_flex_flow(menuPanel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(menuPanel, LV_DIR_VER);
    lv_obj_update_layout(toolbar);
    lv_obj_set_pos(menuPanel, std::min(Width - 390, int(lv_obj_get_x(menuOrigin))), 0);
    lv_obj_set_size(menuPanel, 390, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(menuPanel, std::max(60, height - 44), 0);
    auto item = [&](const char* title, int action, const char* shortcut = "", bool enabled = true) {
      auto* b = button(menuPanel, title, action, 374);
      lv_obj_set_height(b, 40);
      auto* l = lv_obj_get_child(b, 0);
      lv_obj_set_width(l, *shortcut ? 236 : 352);
      lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
      lv_obj_align(l, LV_ALIGN_LEFT_MID, 7, 0);
      if (*shortcut) {
        auto* key = lv_label_create(b);
        caption(key, shortcut);
        lv_obj_set_style_text_font(key, font(16), 0);
        lv_obj_add_style(key, &mutedStyle, 0);
        lv_obj_align(key, LV_ALIGN_RIGHT_MID, -7, 0);
      }
      if (action == Bold) boldButton = b;
      if (action == Italic) italicButton = b;
      if (!enabled) lv_obj_add_state(b, LV_STATE_DISABLED);
      return b;
    };
    switch (which) {
      case MenuFile:
        item("Nouveau", New, "Ctrl+N");
        item("Ouvrir…", Open, "Ctrl+O");
        item("Ouvrir à droite…", Reference);
        item("Enregistrer", Save, "Ctrl+S");
        item("Enregistrer sous…", SaveAs, "Ctrl+Maj+S");
        item("Version précédente", RestoreBackup);
        item("Encodage du texte…", TextEncodingDialog, "", !saveJob.busy());
        item("Rouvrir avec un encodage…", TextReopenEncoding, "", path.length() && !document.dirty() && !saveJob.busy());
        item("Imprimer…", Print, "Ctrl+P");
        break;
      case MenuEdit:
        item("Annuler la modification", Undo, "Ctrl+Z", document.canUndo());
        item("Rétablir", Redo, "Ctrl+Y", document.canRedo());
        item("Couper", Cut, "Ctrl+X", documentSelection ? selectionAnchor != selectionCaret : view.caret != view.anchor);
        item("Copier", Copy, "Ctrl+C", documentSelection ? selectionAnchor != selectionCaret : view.caret != view.anchor);
        item("Coller", Paste, "Ctrl+V", *Tab5TextInput::clipboard());
        item("Tout sélectionner", SelectAll, "Ctrl+A", document.size() != 0);
        item("Rechercher…", Search, "Ctrl+F");
        break;
      case MenuInsert:
        item("Tableau…", Table, "Ctrl+T");
        item("Image…", Image, "Ctrl+Maj+I");
        break;
      case MenuFormat:
        item("Gras", Bold, "Ctrl+B");
        item("Souligné", Underline, "Ctrl+U");
        item("Barré", Strike, "Ctrl+Maj+X");
        item("Italique", Italic, "Ctrl+I");
        item("Niveau de titre", Heading);
        item("Réglage des couleurs…", Colors, "Ctrl+Maj+C");
        item("Couleur du texte", ApplyTextColor, "Ctrl+Alt+C");
        item("Fond du texte", ApplyBackgroundColor, "Ctrl+Alt+H");
        break;
      case MenuView:
        item("Numéros de lignes", LineNumbers, "Ctrl+Maj+N");
        if (serialVisible) lv_obj_add_state(item("Série", SerialToggle), LV_STATE_CHECKED);
        else item("Série", SerialToggle);
        if (split || consoleVisible || serialVisible) item("Fermer la partie droite", RefClose);
        item("Réduire le texte", Smaller);
        item("Agrandir le texte", Larger);
        break;
      case MenuHelp:
        item("Aide et raccourcis…", Help, "F1");
        break;
      case MenuDocuments:
        fillDocumentsMenu();
        item("Créer un projet avec ce document…", ProjectNew);
        item("Ajouter le document au projet", ProjectAdd);
        item("Importer un dossier…", ProjectImport);
        break;
      case MenuCode:
        item("Nouveau script Python", CodeNew);
        item("Nouveau fichier C", CodeNewC);
        item("Nouveau fichier JSON", CodeNewJson);
        item("Langage du document…", CodeLanguage);
        item("Vérifier le JSON", CodeValidateJson, "", isJson());
        item("Exécuter Python", CodeRun, "Ctrl+Entrée", canRunPython());
        item("Arrêter", CodeStop, "Ctrl+.", hooks.stop && hooks.running && hooks.running());
        item("Reprendre", CodeResume, "Ctrl+Maj+R");
        item("Bibliothèques…", CodeLibraries);
        item("Console", CodeConsole, "Ctrl+Maj+L");
        if(consoleVisible)item("Effacer la sortie", CodeClearConsole, "Ctrl+L");
        break;
      default: break;
    }
    syncStyle();
    Tab5Focus.focusFirst(menuPanel);
  }
  void build() {
    if (built) return;
    built = true;
    workspace.begin();
    workspace.load(workspaceFs);
    workspaceScanner.begin(workspace);
    toolbar = lv_obj_create(root);
    panel(toolbar);
    lv_obj_set_size(toolbar, Width, 40);
    lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW);
    lv_obj_add_flag(toolbar,LV_OBJ_FLAG_SCROLLABLE);lv_obj_set_scroll_dir(toolbar,LV_DIR_HOR);lv_obj_set_scrollbar_mode(toolbar,LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_column(toolbar, 4, 0);
    const char* menus[] = {"Fichier", "Édition", "Insertion", "Format", "Affichage", "Aide", "Documents", "Code"};
    const int widths[] = {106, 110, 120, 100, 125, 80, 145, 96};
    for (unsigned i = 0; i < 8; ++i) {
      lv_point_t extent{};lv_text_get_size(&extent,tr(menus[i]),font(20),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
      int width=std::max(76,int(extent.x)+44);
      auto* b = menuButtons[i] = button(toolbar, menus[i], MenuFile + i, width);
      lv_obj_set_height(b, 36);
      auto* title = lv_obj_get_child(b, 0);
      lv_obj_set_width(title,width-40);lv_label_set_long_mode(title,LV_LABEL_LONG_CLIP);
      lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);
      auto* arrow = lv_label_create(b);
      lv_label_set_text(arrow, LV_SYMBOL_DOWN);
      lv_obj_set_style_text_font(arrow, &lv_font_montserrat_20, 0);
      lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -2, 0);
    }
    auto* actionKeyboard = button(toolbar, LV_SYMBOL_KEYBOARD, Keyboard, 52, true);
    lv_obj_set_height(actionKeyboard, 36);
    Tab5Focus.describe(actionKeyboard, tr("Clavier tactile"));
    auto* actionPrint=printButton=button(toolbar,"",Print,52,true);lv_obj_set_height(actionPrint,36);lv_obj_clean(actionPrint);tab5PrinterIcon(actionPrint,lv_color_hex(color(WritingColor::Text)),lv_color_hex(color(WritingColor::Surface)));Tab5Focus.describe(actionPrint,tr("Imprimer · Ctrl+P"));
    mainPathButton = button(root, "", Outline, Width - 136);
    filename = lv_obj_get_child(mainPathButton, 0);
    lv_label_set_long_mode(filename, LV_LABEL_LONG_CLIP);
    lv_obj_align(filename, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_t* p = mainPrev = button(root, LV_SYMBOL_LEFT, Prev, 48, true);
    lv_obj_set_pos(p, Width - 112, 46);
    p = mainNext = button(root, LV_SYMBOL_RIGHT, Next, 48, true);
    lv_obj_set_pos(p, Width - 56, 46);
    body = lv_obj_create(root);
    panel(body);
    lv_obj_set_pos(body, 0, 40);
    mainPane = lv_obj_create(body);
    panel(mainPane);
    lv_obj_set_parent(mainPathButton, mainPane);
    lv_obj_set_parent(mainPrev, mainPane);
    lv_obj_set_parent(mainNext, mainPane);
    mainScroll = lv_obj_create(mainPane);
    panel(mainScroll, true);
    lv_obj_set_style_pad_all(mainScroll, 12, 0);
    lv_obj_set_style_pad_right(mainScroll, 28, 0);
    lv_obj_set_scroll_dir(mainScroll, LV_DIR_VER);
    lv_obj_remove_flag(mainScroll, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLL_ELASTIC |
                                                              LV_OBJ_FLAG_SCROLL_MOMENTUM));
    refPane = lv_obj_create(body);
    panel(refPane);
    lv_obj_add_style(refPane, &referenceStyle, 0);
    refPathButton = button(refPane, "", RefOutline, 270);
    referenceName = lv_obj_get_child(refPathButton, 0);
    lv_label_set_long_mode(referenceName, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(referenceName, 254);
    lv_obj_align(referenceName, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_pos(refPathButton, 8, 0);
    p = button(refPane, LV_SYMBOL_LEFT, RefPrev, 42, true);
    lv_obj_set_pos(p, 286, 0);
    p = button(refPane, LV_SYMBOL_RIGHT, RefNext, 42, true);
    lv_obj_set_pos(p, 334, 0);
    p = button(refPane, LV_SYMBOL_CLOSE, RefClose, 42, true);
    lv_obj_set_pos(p, 382, 0);
    refScroll = lv_obj_create(refPane);
    panel(refScroll, true);
    lv_obj_add_style(refScroll, &referenceStyle, 0);
    lv_obj_set_pos(refScroll, 0, 46);
    lv_obj_set_style_pad_all(refScroll, 12, 0);
    lv_obj_set_style_pad_right(refScroll, 28, 0);
    lv_obj_set_scroll_dir(refScroll, LV_DIR_VER);
    lv_obj_remove_flag(refScroll, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLL_ELASTIC |
                                                             LV_OBJ_FLAG_SCROLL_MOMENTUM));
    consolePane = lv_obj_create(body);
    panel(consolePane, true);
    lv_obj_add_style(consolePane, &referenceStyle, 0);
    lv_obj_set_style_pad_all(consolePane, 12, 0);
    lv_obj_set_scroll_dir(consolePane, LV_DIR_VER);
    consoleText = lv_label_create(consolePane);Tab5TextInput::selectable(consoleText);
    Tab5ContextMenu::bindOutput(consoleText,[](void* value){auto* self=static_cast<Impl*>(value);if(self->hooks.clearConsole)self->hooks.clearConsole();if(self->consoleCache)self->consoleCache[0]=0;lv_label_set_text(self->consoleText,"");lv_obj_scroll_to_y(self->consolePane,0,LV_ANIM_OFF);},this);
    lv_label_set_text(consoleText, "");
    lv_obj_set_style_text_font(consoleText, font(18), 0);
    lv_obj_set_width(consoleText, 406);
    lv_label_set_long_mode(consoleText, LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(consolePane, LV_OBJ_FLAG_HIDDEN);
    view.context = this;
    view.font = hooks.font;
    view.translate = hooks.translate;
    view.setTheme(viewTheme());
    view.imageLoad = hooks.imageLoad;
    view.imageFree = hooks.imageFree;
    view.begin(mainScroll);
    Tab5Focus.describe(view.object, tr("Texte · Entrée : écrire · Échap : commandes"));
    view.blockActivated = [](void* p, size_t index) { static_cast<Impl*>(p)->tableDialog(index); };
    view.changed = [](void* p) { return static_cast<Impl*>(p)->changedRich(); };
    view.activated = [](void* p) {
      auto* a = static_cast<Impl*>(p);
      a->documentSelection=false;
      a->target = a->proxy;
      a->view.editing = true;
      Tab5Focus.clear();
      a->syncStyle();
      // Register the hidden rich-text input proxy for a physical keyboard too.
      // The OS IME needs this target while the onscreen keyboard stays closed.
      a->showKeyboard(!writing::usbConnected(), a->proxy);
    };
    view.linked = [](void* p, const char* target) { static_cast<Impl*>(p)->followLink(target); };
    view.edge = [](void* p, int n) { static_cast<Impl*>(p)->deferredEdge = n; };
    refView.context = this;
    refView.font = hooks.font;
    refView.translate = hooks.translate;
    refView.setTheme(viewTheme());
    refView.imageLoad = hooks.imageLoad;
    refView.imageFree = hooks.imageFree;
    refView.begin(refScroll, true);
    Tab5Focus.describe(refView.object,
                       tr("Document de droite · Entrée : parcourir · Échap : commandes"));
    refView.edge = [](void* p, int n) { static_cast<Impl*>(p)->deferredEdge = n * 2; };
    proxy = lv_textarea_create(root);
    lv_obj_add_flag(proxy, LV_OBJ_FLAG_HIDDEN);
    lv_textarea_set_text(proxy, "");
    serialBuild();
    status = label(root, "", 12, FullHeight - 30, Width - 24);
    lv_obj_add_style(status, &mutedStyle, 0);
    layout();
  }
  void layout() {
    if(!consoleVisible)releaseConsole();
    if (!built) return;
    if (lastLayoutHeight == height && lastLayoutWidth == Width && lastLayoutSplit == split && lastLayoutConsole == consoleVisible) return;
    lastLayoutHeight = height;
    lastLayoutWidth = Width;
    lastLayoutSplit = split;
    lastLayoutConsole = consoleVisible;
    cachedTitleCursor = SIZE_MAX;
    lv_obj_set_size(root, Width, height);
    lv_obj_set_width(toolbar, Width);
    lv_obj_set_width(status, Width - 24);
    lv_obj_set_size(body, Width, std::max(96, height - 74));
    int mw = (split || consoleVisible || serialVisible) ? Width - 446 : Width;
    lv_obj_set_size(mainPane, mw, std::max(96, height - 74));
    lv_obj_set_pos(mainScroll, 0, 46);
    lv_obj_set_pos(mainPathButton, 8, 0);
    lv_obj_set_width(mainPathButton, mw - 136);
    lv_obj_set_width(filename, mw - 152);
    lv_obj_set_pos(mainPrev, mw - 112, 0);
    lv_obj_set_pos(mainNext, mw - 56, 0);
    lv_obj_set_size(mainScroll, mw, std::max(96, height - 120));
    lv_obj_set_pos(refPane, mw + 8, 0);
    lv_obj_set_size(refPane, 438, std::max(96, height - 74));
    lv_obj_set_size(refScroll, 438, std::max(96, height - 120));
    if (split && !consoleVisible && !serialVisible)
      lv_obj_remove_flag(refPane, LV_OBJ_FLAG_HIDDEN);
    else
      lv_obj_add_flag(refPane, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(consolePane, mw + 8, 0);
    lv_obj_set_size(consolePane, 438, std::max(96, height - 74));
    if (consoleVisible && !serialVisible) lv_obj_remove_flag(consolePane, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(consolePane, LV_OBJ_FLAG_HIDDEN);
    serialLayout(mw);
    lv_obj_set_pos(status, 12, height - 28);
    lv_obj_update_layout(root);
    if (view.object) view.reflow(false);
    if (refView.object && split) refView.reflow(false);
    if (menuShade) lv_obj_set_size(menuShade, Width, std::max(60, height - 40));
    if (modalList) lv_obj_set_height(modalList, std::max(80, height - 120));
    if (tableGrid) lv_obj_set_height(tableGrid, std::max(80, height - 138));
  }
  static void freeRow(lv_event_t* e) { delete static_cast<Row*>(lv_event_get_user_data(e)); }
  void bindRow(lv_obj_t* row, Row* context) {
    lv_obj_add_event_cb(row, freeRow, LV_EVENT_DELETE, context);
    lv_obj_add_event_cb(
        row,
        [](lv_event_t* e) {
          Row* r = static_cast<Row*>(lv_event_get_user_data(e));
          Impl* app = r->app;  // Closing or repopulating the dialog deletes this Row.
          if (r->heading) {
            const size_t offset = r->offset;
            const bool ref = r->ref;
            String file = r->path;
            app->closeDialog();
            if (ref) {
              if (file.length() && file != app->referencePath) app->load(file, true);
              app->refStart = offset;
              app->renderReference();
            } else {
              app->cursor = app->start = offset;
              app->view.blur();
              app->render();
            }
          } else if (r->directory) {
            app->browseDir = r->path;
            app->listFiles();
          } else {
            String chosen = r->path;
            const bool ref = r->ref || app->browseMode == 1;
            const bool image = app->browseMode == 3;
            app->closeDialog();
            if (image)
              app->importImage(chosen);
            else
              app->load(chosen, ref);
          }
        },
        LV_EVENT_CLICKED, context);
  }
  bool inFence(const writing::Document& doc, size_t pos) {
    return (&doc == &document ? mainIndex : referenceIndex).fencedAt(doc, pos);
  }

  std::string slice(const writing::Document& doc, size_t a, size_t b) {
    std::string out(b - a, '\0');
    if (b > a) doc.copy(a, &out[0], b - a);
    return out;
  }
  void indexHeadings() {
    cachedTitleCursor = SIZE_MAX;
    mainIndex.reset();
    mainIndex.step(document);
    headingsDirty = false;
  }
  size_t sectionEnd(const writing::Document& doc, size_t from,
                    const std::vector<writing::Heading>& hs) {
    size_t stop = viewEnd(doc, from);
    for (auto& h : hs)
      if (h.offset > from) {
        stop = std::min(stop, h.offset);
        break;
      }
    // During background indexing, retain section boundaries in the visible
    // window without waiting for the rest of the document.
    bool fence = inFence(doc, from);
    for (size_t p = from; p < stop;) {
      const bool marker = (doc.at(p) == '`' && doc.at(p + 1) == '`' && doc.at(p + 2) == '`') ||
                          (doc.at(p) == '~' && doc.at(p + 1) == '~' && doc.at(p + 2) == '~');
      if (marker) fence = !fence;
      size_t hashes = 0;
      while (hashes < 6 && doc.at(p + hashes) == '#') ++hashes;
      if (p > from && !fence && hashes && doc.at(p + hashes) == ' ') {
        stop = p;
        break;
      }
      while (p < stop && doc.at(p) != '\n') ++p;
      if (p < stop) ++p;
    }
    // Keep a table intact across the usual text-window boundary (bounded at 32 KiB).
    if (stop < doc.size() && stop > from) {
      size_t prior = doc.lineStart(doc.prevChar(stop));
      auto hasPipe = [&](size_t a, size_t b) {
        for (size_t p = a; p < b; ++p)
          if (doc.at(p) == '|') return true;
        return false;
      };
      if (hasPipe(prior, stop))
        while (stop < doc.size() && stop - from < 32768) {
          size_t next = stop;
          while (next < doc.size() && doc.at(next) != '\n' && next - from < 32768) ++next;
          if (!hasPipe(stop, next)) break;
          stop = next + (next < doc.size() && doc.at(next) == '\n' ? 1 : 0);
        }
    }
    return stop;
  }
  String breadcrumb(const String& file, const std::vector<writing::Heading>& hs, size_t at,
                    int maxWidth) {
    String name = file.length() ? baseOf(file) : tr("Brouillon sans nom");
    if (writing::sourceFile(file.c_str()) || (file == path && codeDraft)) return tailPath(name.c_str(),font(20),maxWidth).c_str();
    std::string levels[6];
    for (auto& h : hs) {
      if (h.offset > at) break;
      levels[h.level - 1] = h.title.c_str();
      for (size_t i = h.level; i < 6; ++i) levels[i].clear();
    }
    for (auto& l : levels)
      if (!l.empty()) {
        name += " / ";
        name += l.c_str();
      }
    return tailPath(name.c_str(), font(20), maxWidth).c_str();
  }

  void syncStyle() {
    auto set = [&](lv_obj_t* b, uint8_t bit) {
      if (!b) return;
      auto& editor = activeView();
      bool on = editor.anchor != editor.caret
                    ? editor.text.allStyle(std::min(editor.anchor, editor.caret),
                                           std::max(editor.anchor, editor.caret), bit)
                    : (editor.typingStyle & bit);
      if (on) lv_obj_add_state(b, LV_STATE_CHECKED);
      else lv_obj_remove_state(b, LV_STATE_CHECKED);
    };
    if(mainScroll){lv_obj_set_style_border_width(mainScroll,2,0);lv_obj_set_style_border_color(mainScroll,lv_color_hex(color(view.editing||Tab5Focus.focused()==view.object?WritingColor::Accent:WritingColor::Surface)),0);}
    set(tableGrid ? cellBold : boldButton, 1);
    set(tableGrid ? cellItalic : italicButton, 2);
  }
  void prepareSyntaxView(bool ref) {
    auto& rich=ref?refView:view;auto& index=ref?referenceSyntaxIndex:syntaxIndex;
    const auto& doc=ref?reference:document;const size_t offset=ref?refStart:start;
    rich.syntax=ref?writing::syntaxLanguage(referencePath.c_str()):resolvedSyntax();
    rich.syntaxInitial={};rich.syntaxReady=index.stateAt(doc,rich.syntax,offset,rich.syntaxInitial);
    if(!rich.syntaxReady){index.step(doc,rich.syntax);rich.syntaxReady=index.stateAt(doc,rich.syntax,offset,rich.syntaxInitial);}
  }
  void serviceSyntax() {
    if(!visible)return;
    for(unsigned ref=0;ref<2;++ref){
      if(ref&&!split)continue;auto& rich=ref?refView:view;if(!rich.sourceMode)continue;
      auto& index=ref?referenceSyntaxIndex:syntaxIndex;const auto& doc=ref?reference:document;
      index.step(doc,rich.syntax);
      if(rich.syntaxReady)continue;
      writing::SyntaxState state;
      const bool ready=index.stateAt(doc,rich.syntax,ref?refStart:start,state);
      if(ready!=rich.syntaxReady||(ready&&state!=rich.syntaxInitial)){
        rich.syntaxReady=ready;rich.syntaxInitial=state;rich.recolorSyntax();
      }
    }
  }
  void render(bool = false) {
    if (!built) return;
    layoutReclaimed = false;
    if (headingsDirty) indexHeadings();
    start = std::min(start, document.size());
    end = isCode() ? viewEnd(document, start) : sectionEnd(document, start, mainHeadings);
    pageSource = slice(document, start, end);
    view.fontSize = fontSize;
    view.lineNumbers=lineNumbers;view.firstLine=mainIndex.lineAt(document,start);
    view.documentPath = syntaxPath();
    view.sourceMode = isCode();
    if(view.sourceMode)prepareSyntaxView(false);
    pageLiteral = isCode() || inFence(document, start) || partialLine();
    view.load(pageSource, pageLiteral);
    view.move(view.text.indexAtSource(cursor >= start ? cursor - start : 0));
    displaySelection();
    lv_obj_scroll_to_y(mainScroll, 0, LV_ANIM_OFF);
    syncStyle();
    updateStatus();
    sessionDirty = true;
  }
  void renderReference() {
    if (!built) return;
    layout();
    if (!split) return;
    referenceIndex.step(reference);
    refEnd = writing::sourceFile(referencePath.c_str()) ? viewEnd(reference, refStart) : sectionEnd(reference, refStart, refHeadings);
    refView.sourceMode = writing::sourceFile(referencePath.c_str());
    refView.fontSize = fontSize;
    refView.lineNumbers=lineNumbers;refView.firstLine=referenceIndex.lineAt(reference,refStart);
    refView.documentPath = referencePath.c_str();
    if(refView.sourceMode)prepareSyntaxView(true);
    refView.load(slice(reference, refStart, refEnd), inFence(reference, refStart));
    setLabel(referenceName, breadcrumb(referencePath, refHeadings, refStart, 254).c_str());
    lv_obj_scroll_to_y(refScroll, 0, LV_ANIM_OFF);
  }
  bool partialLine() const {
    return (start && document.at(start - 1) != '\n') ||
           (end < document.size() && end && document.at(end - 1) != '\n');
  }
  bool changedRich() {
    std::vector<size_t> mapping;
    std::string next = view.text.markdown(&mapping);
    size_t oldLen = pageSource.size(), newLen = next.size(), prefix = 0, suffix = 0;
    while (prefix < oldLen && prefix < newLen && pageSource[prefix] == next[prefix]) ++prefix;
    while (prefix && (static_cast<uint8_t>(next[prefix]) & 0xc0) == 0x80) --prefix;
    while (suffix < oldLen - prefix && suffix < newLen - prefix &&
           pageSource[oldLen - suffix - 1] == next[newLen - suffix - 1])
      ++suffix;
    while (suffix && (static_cast<uint8_t>(next[newLen - suffix]) & 0xc0) == 0x80) --suffix;
    size_t caret = view.caret, anchor = view.anchor;
    uint8_t style = view.typingStyle;
    writing::RichText parsed;
    parsed.parse(next, pageLiteral, isCode());
    caret = parsed.indexAtSource(mapping[std::min(caret, mapping.size() - 1)]);
    anchor = parsed.indexAtSource(mapping[std::min(anchor, mapping.size() - 1)]);
    size_t after = start + parsed.sourceAt(caret);
    if (!document.replace(start + prefix, start + oldLen - suffix, next.data() + prefix,
                          newLen - prefix - suffix, cursor, after, millis())) {
      view.load(pageSource, pageLiteral);
      note("Modification refusée : mémoire ou taille maximale");
      return false;
    }
    mainIndex.edited(start + prefix, oldLen - prefix - suffix, newLen - prefix - suffix);
    syntaxIndex.edited(document,start+prefix);
    if(isCode())prepareSyntaxView(false);
    if (isCode()) { const uint32_t bg = color(WritingColor::Surface); writing::colorSyntax(parsed, syntaxPath(), (bg & 255) + ((bg >> 8) & 255) + (bg >> 16) < 384, view.syntaxReady?view.syntax:writing::SyntaxLanguage::Plain, view.syntaxInitial); }
    end = start + newLen;
    cursor = after;
    pageSource = std::move(next);
    view.text = std::move(parsed);
    view.caret = caret;
    view.anchor = anchor;
    view.typingStyle = style;
    lastEdit = millis();
    statsDirty = sessionDirty = true;
    message = "";
    syncStyle();
    return true;
  }
  void navigate(bool ref, int direction, bool chunk = false) {
    if (headingsDirty) indexHeadings();
    auto& hs = ref ? refHeadings : mainHeadings;
    auto& doc = ref ? reference : document;
    size_t at = ref ? refStart : start, next = at;
    if (chunk || (ref ? writing::sourceFile(referencePath.c_str()) : isCode())) {
      if (direction > 0)
        next = ref ? refEnd : end;
      else
        next = previousView(doc, at);
    } else if (direction > 0) {
      for (auto& h : hs)
        if (h.offset > at) {
          next = h.offset;
          break;
        }
    } else {
      next = 0;
      for (auto& h : hs) {
        if (h.offset >= at) break;
        next = h.offset;
      }
    }
    if (next == at || next >= doc.size()) return;
    if (ref) {
      refStart = next;
      renderReference();
    } else {
      start = cursor = next;
      view.blur();
      render();
    }
    sessionDirty = true;
  }
  void followLink(const char* targetText) {
    if (saveJob.busy()) {
      note("Attendez la fin de la sauvegarde");
      return;
    }
    std::string target = targetText;
    std::string decoded;
    for (size_t i = 0; i < target.size(); ++i) {
      if (target[i] == '%' && i + 2 < target.size()) {
        auto hex = [](char c) -> int {
          return c >= '0' && c <= '9'   ? c - '0'
                 : c >= 'A' && c <= 'F' ? c - 'A' + 10
                 : c >= 'a' && c <= 'f' ? c - 'a' + 10
                                        : -1;
        };
        int hi = hex(target[i + 1]), lo = hex(target[i + 2]);
        if (hi >= 0 && lo >= 0 && hi * 16 + lo) {
          decoded += char(hi * 16 + lo);
          i += 2;
          continue;
        }
      }
      decoded += target[i];
    }
    target = decoded;
    if (target.find("://") != std::string::npos || target.rfind("mailto:", 0) == 0) {
      note("Lien externe : ouverture web non disponible");
      return;
    }
    size_t hash = target.find('#');
    std::string file = hash == std::string::npos ? "" : target.substr(0, hash),
                term = hash == std::string::npos ? target : target.substr(hash + 1);
    if (term.empty()) term = file;
    struct Match {
      String path;
      size_t offset;
    };
    std::vector<Match> found;
    for (auto& candidate : references) {
      std::string base = baseOf(candidate).c_str(), stem = base.substr(0, base.find_last_of('.'));
      if (!file.empty() && file != base && file != stem && file != candidate.c_str()) continue;
      writing::Document temp;
      bool recovered = false;
      const writing::Document* source = &reference;
      if (candidate != referencePath) {
        if (!writing::loadDocument(fs, candidate.c_str(), temp, recovered, yieldSd)) continue;
        source = &temp;
      }
      writing::DocumentIndex temporaryIndex;
      auto& index = candidate == referencePath ? referenceIndex : temporaryIndex;
      while (!index.ready()) {
        index.step(*source);  // Bounded index chunks; no second full-file source copy.
        yieldSd();
      }
      for (const auto& h : index.titles)
        if (h.title.c_str() == term ||
            writing::anchorName(h.title.c_str()) == writing::anchorName(term))
          found.push_back({candidate, h.offset});
      if (found.empty() && (target == base || target == stem || target == candidate.c_str()))
        found.push_back({candidate, 0});
    }
    if (found.empty()) {
      note("Référence introuvable : ajoutez le document à droite et vérifiez son titre");
      return;
    }
    if (found.size() > 1) {
      dialog("Plusieurs titres correspondent — choisissez le document");
      modalList = lv_obj_create(modal);
      panel(modalList, true);
      lv_obj_set_pos(modalList, 16, 56);
      lv_obj_set_size(modalList, Width - 32, FullHeight - 72);
      lv_obj_set_style_pad_right(modalList, 32, 0);
      lv_obj_set_flex_flow(modalList, LV_FLEX_FLOW_COLUMN);
      for (auto& m : found) {
        auto* b = lv_button_create(modalList);
        panel(b);
        lv_obj_set_size(b, Width - 96, 48);
        label(b, m.path.c_str(), 12, 8, Width - 144, false);
        auto* r = new (std::nothrow) Row{this, m.path, false, m.offset, true, true};
        if (r) bindRow(b, r);
      }
      return;
    }
    if (load(found[0].path, true)) {
      refStart = found[0].offset;
      renderReference();
    }
  }
  void updateStatus() {
    if (!built) return;
    // A timer tick must not re-measure the filename or invalidate unchanged labels.
    // Document titles can change during indexing or edits even at the same caret.
    const size_t headingPosition = std::upper_bound(mainHeadings.begin(), mainHeadings.end(), cursor,
        [](size_t at, const writing::Heading& heading) { return at < heading.offset; }) - mainHeadings.begin();
    if (cachedTitleCursor == SIZE_MAX || cachedTitleHeading != headingPosition || cachedTitlePath != path ||
        cachedTitleCount != mainHeadings.size() || cachedTitleSplit != split ||
        cachedTitleDirty != document.dirty()) {
      cachedTitle = breadcrumb(path, mainHeadings, cursor, ((split || consoleVisible) ? Width - 446 : Width) - 174);
      if (document.dirty()) cachedTitle += "  •";
      cachedTitleCursor = cursor;
      cachedTitleHeading = headingPosition;
      cachedTitlePath = path;
      cachedTitleCount = mainHeadings.size();
      cachedTitleSplit = split;
      cachedTitleDirty = document.dirty();
      setLabel(filename, cachedTitle.c_str());
    }
    String line;
    if (saveJob.busy())
      line = String(tr("Sauvegarde vérifiée")) + " : " + String(saveJob.progress()) + " %";
    else if (!mainIndex.ready())
      line = tr("Indexation du document… · passage affiché disponible");
    else if (message.length())
      line = tr(message.c_str());
    else {
      line = String(static_cast<unsigned>(lastWords)) + " " + tr("mots") + " · " +
             String(static_cast<unsigned>(lastCharacters)) + " " + tr("caractères") + " · " +
             String(static_cast<unsigned>((document.size() + 1023) / 1024)) + " " + tr("Kio") +
             " · " + writing::encodingName(document.encoding()) + "  |  " + tr("Passage") + " " +
             String(static_cast<unsigned>(document.size() ? start * 100 / document.size() : 0)) +
             "–" + String(static_cast<unsigned>(document.size() ? end * 100 / document.size() : 100)) +
             " %  |  " + tr(writing::usbStatus());
    }
    setLabel(status, line.c_str());
  }
  bool ensurePath() {
    if (path.length()) return Tab5Files::userWritable(path.c_str());
    const String directory = codeDraft && browseDir.length() ? browseDir : String("/Documents");
    bool present=false;if(!fs.probe(directory.c_str(),present)||(!present&&!tab5FS.mkdir(directory.c_str()))){note("Dossier inaccessible");return false;}
    for (unsigned i = 1; i <= 9999; ++i) {
      char p[300];
      snprintf(p, sizeof(p), codeDraft ? "draft-%04u%s" : "Draft-%04u%s", i, codeDraft?draftExtension:".md");
      const String candidate=join(directory,p);
      if(candidate.length()>=sizeof(p)){note("Chemin de fichier invalide");return false;}
      strcpy(p,candidate.c_str());
      char backup[320];if(!writing::backupPath(p,backup,sizeof(backup))){note("Chemin de fichier invalide");return false;}
      bool primary=false,saved=false,legacy=false;const String oldBackup=String(p)+".writer.bak";
      if(!fs.probe(p,primary)||!fs.probe(backup,saved)||!fs.probe(oldBackup.c_str(),legacy)){note("Dossier inaccessible");return false;}
      if (!primary && !saved && !legacy) { path = p; return true; }
    }
    note("Impossible de nommer le brouillon");
    return false;
  }
  bool save(const String& destination = "", bool synchronous = false) {
    if (saveJob.busy()) {
      if (!synchronous) return true;
      while (saveJob.busy()) {
        saveJob.step();
        delay(1);
      }
      observeSave();
    }
    if (!destination.length() && !ensurePath()) return false;
    if (!Tab5Files::userWritable((destination.length() ? destination : path).c_str())) {
      note("Chemin de fichier invalide");
      return false;
    }
    if (!isCode() && destination.length() && path.length() && parentOf(destination) != parentOf(path) &&
        !copyRelativeImages(parentOf(destination)))
      return false;
    pendingPath = destination.length() ? destination : path;
    warnSystem(pendingPath);
    lastAttempt = millis();
    saveObserved = false;
    if (!saveJob.begin(document, pendingPath.c_str())) {
      note(saveJob.error());
      return false;
    }
    if (synchronous) {
      while (saveJob.busy()) {
        saveJob.step();
        delay(1);
      }
      observeSave();
      return saveJob.state() == writing::SaveJob::State::Done;
    }
    updateStatus();
    return true;
  }
  void observeSave() {
    if (saveObserved || saveJob.busy()) return;
    if (saveJob.state() == writing::SaveJob::State::Done) {
      const bool moved = parentOf(path) != parentOf(pendingPath);
      const bool wasCode = isCode();
      const auto oldSyntax=resolvedSyntax();
      path = pendingPath;
      workspace.add(path.c_str(),currentDocumentProject);
      codeDraft = false;
      if (wasCode != isCode()||oldSyntax!=resolvedSyntax()) render();
      view.documentPath=syntaxPath();
      capturePosition();
      if (moved) {
        view.documentPath = syntaxPath();
        view.releaseImages();
        view.reflow();
      }
      message = "Enregistré";
      cachedTitleCursor = SIZE_MAX;
      sessionDirty = true;
    } else if (saveJob.state() == writing::SaveJob::State::Error)
      note(saveJob.error());
    saveObserved = true;
    if (printAfterSave) {
      // An autosave may already have captured an older revision when Print
      // was pressed. Finish that transaction, then save the current document
      // before handing its full file to the section selector / print worker.
      if (saveJob.state() == writing::SaveJob::State::Done && document.dirty()) {
        message = "Enregistrement du texte à imprimer…";
        if (!save()) printAfterSave = false;
        updateStatus();
        return;
      }
      printAfterSave = false;
      if (saveJob.state() == writing::SaveJob::State::Done && hooks.print) hooks.print(path.c_str());
    }
    updateStatus();
  }
  bool beforeSwitch() {
    if (saveJob.busy()) {
      while (saveJob.busy()) {
        saveJob.step();
        delay(1);
      }
      observeSave();
    }
    if (document.dirty() && !save("", true)) return false;
    capturePosition();
    return persistWorkspace();
  }
  bool create(const String& directory, bool code = false, const char* extension = ".py") {
    if (!beforeSwitch()) return false;
    writing::Document empty;
    document.swap(empty);
    documentSelection=false;
    path = "";
    codeDraft = code; draftExtension = extension;
    syntaxChoice=writing::SyntaxLanguage::Auto;syntaxIndex.reset();
    currentDocumentProject = workspace.activeProject();
    browseDir = directory.length() ? directory : String("/");
    start = cursor = 0;
    headingsDirty = statsDirty = sessionDirty = true;
    view.blur();
    render();
    return true;
  }
  bool load(const String& file, bool ref = false, int project = -1, writing::TextEncoding encoding = writing::TextEncoding::Auto) {
    char normalized[writing::Workspace::PathSize];
    if (!writing::Workspace::normalizePath(file.c_str(), normalized)) { note("Chemin invalide ou supérieur à 299 octets"); return false; }
    const uint8_t destinationProject = project < 0 ? 0 : static_cast<uint8_t>(project);
    if (!ref && workspace.find(file.c_str(),destinationProject) < 0 && workspace.count() >= 256) { note("Liste pleine : 256 documents au maximum"); return false; }
    if (!ref && !beforeSwitch()) return false;
    if (saveJob.busy()) {
      note("Attendez la sauvegarde");
      return false;
    }
    bool recovered = false; writing::CodecError codecError=writing::CodecError::None;
    if (!writing::loadDocument(fs, file.c_str(), ref ? reference : document, recovered, yieldSd,encoding,&codecError)) {
      if(codecError==writing::CodecError::InvalidSequence){encodingDialog(file,ref,project);note("Choisissez l'encodage du fichier");}
      else note(codecError==writing::CodecError::Unsupported?"Encodage non pris en charge":"Ouverture impossible : stockage ou limite de taille");
      return false;
    }
    if (ref) {
      referenceIndex.reset();referenceSyntaxIndex.reset();
      referencePath = file;
      if (std::find(references.begin(), references.end(), file) == references.end())
        references.push_back(file);
      refStart = 0;
      split = true;
      renderReference();
    } else {
      documentSelection=false;
      path = file;
      codeDraft = false;syntaxChoice=writing::SyntaxLanguage::Auto;syntaxIndex.reset();
      currentDocumentProject = destinationProject;
      workspace.add(file.c_str(),destinationProject);
      workspace.setActiveProject(destinationProject);
      start = cursor = 0;
      headingsDirty = true;
      statsDirty = true;
      view.blur();
      restorePosition();
      persistWorkspace();
    }
    sessionDirty = true;
    if (recovered) note("Version précédente récupérée (.writer.bak)");
    if (!ref) warnSystem(file);
    return true;
  }
  void closeDialog() {
    encodingOpenPath="";
    browser.reset();browseMedia=nullptr;browseMediaButton=nullptr;
    if (resumePhase) {
      resumeJob.cancel(); resumePhase = 0; resumeProgress = nullptr;
    }
    outlineAfterIndex = 0;
    closeMenu(false);
    showKeyboard(false);
    target = nullptr;
    Tab5Focus.clear();
    tableGrid = nullptr;
    cellBold = cellItalic = colorPanel = nullptr;
    colorsOpen = false; colorPreview = nullptr;
    projectNameDialog = false;
    dialogInput = replaceInput = modalList = modalTitle = nullptr;
    lv_obj_t* old = modal;
    modal = nullptr;
    if (old) lv_obj_delete(old);
    tableFields.clear();
  }
  void dialog(const char* title) {
    closeDialog();
    modal = lv_obj_create(root);
    panel(modal);
    lv_obj_set_size(modal, Width, FullHeight);
    lv_obj_set_pos(modal, 0, 0);
    modalTitle = label(modal, title, 20, 10, Width - 100);
    lv_obj_t* b = button(modal, LV_SYMBOL_CLOSE, CloseDialog, 48, true);
    lv_obj_set_pos(b, Width - 64, 0);
  }
  lv_obj_t* field(lv_obj_t* parent, const char* placeholder, int x, int y, int w) {
    lv_obj_t* f = lv_textarea_create(parent);
    lv_obj_set_pos(f, x, y);
    lv_obj_set_size(f, w, 48);
    lv_textarea_set_one_line(f, true);
    lv_textarea_set_max_length(f, 200);
    lv_textarea_set_text(f, "");
    caption(f, placeholder, true);
    lv_obj_add_style(f, &fieldStyle, 0);
    lv_obj_add_style(f, &mutedStyle, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_text_font(f, font(22), 0);
    lv_obj_add_event_cb(
        f,
        [](lv_event_t* e) {
          if (instance) {
            instance->target = static_cast<lv_obj_t*>(lv_event_get_target(e));
            if (!writing::usbConnected()) instance->showKeyboard(true, instance->target);
          }
        },
        LV_EVENT_CLICKED, nullptr);
    return f;
  }
  void browse(int mode) {
    browseMode = mode;
    browseDir = path.length() ? parentOf(path) : codeDraft && browseDir.length() ? browseDir : String("/Documents");
    if (!tab5FS.exists(browseDir.c_str())) browseDir = "/";
    dialog(mode == 3   ? "Importer une image (JPEG, PNG, BMP)"
           : mode == 1 ? "Ouvrir un document à droite"
           : mode == 2 ? "Enregistrer sous"
                       : "Ouvrir un document");
    lv_obj_t* b = button(modal, LV_SYMBOL_UP, ParentFolder, 48, true);
    lv_obj_set_pos(b, 16, 48);
    b = browseMediaButton = button(modal, LV_SYMBOL_DRIVE, BrowseStorage, 54, true);
    lv_obj_set_pos(b, 76, 48);
    Tab5Focus.describe(b,tr("Stockage"));
    if (mode == 4) {
      b = button(modal,"Importer ce dossier",ProjectUseFolder,300);
      lv_obj_set_pos(b,176,48);
    }
    if (mode == 2) {
      dialogInput = field(modal, "Nom du fichier", 176, 48, std::max(120,Width-364));
      lv_textarea_set_text(dialogInput, path.length() ? baseOf(path).c_str() : (codeDraft ? draftName() : "Novel.md"));
      target = dialogInput;
      b = button(modal, "Enregistrer", ChooseName, 150);
      lv_obj_set_pos(b, Width-170, 52);
    }
    modalList = lv_obj_create(modal);
    panel(modalList, true);
    lv_obj_set_pos(modalList, 16, 170);
    lv_obj_set_size(modalList, Width - 32, FullHeight - 188);
    browser.reset(new(std::nothrow)Tab5DirectoryView(modalList,font(22),
      [](const char* name,bool folder,void* context){auto* app=static_cast<Impl*>(context);return folder||(app->browseMode!=4&&(app->browseMode==3?imageFile(name):textFile(name)));},
      [](const char* selected,bool folder,void* context){auto* app=static_cast<Impl*>(context);String chosen=selected;
        if(folder){app->browseDir=chosen;app->listFiles();return;}
        if(app->browseMode==2){if(app->dialogInput)lv_textarea_set_text(app->dialogInput,baseOf(chosen).c_str());return;}
        const bool ref=app->browseMode==1,image=app->browseMode==3;app->closeDialog();if(image)app->importImage(chosen);else app->load(chosen,ref);
      },this));
    if(!browser){note("Mémoire insuffisante.");return;}
    auto* sort=browser->sortControl(modal,std::min(360,Width-32));lv_obj_set_pos(sort,16,108);
    browseMediaState=(tab5SdAvailable()?1:0)|(Tab5UsbStorage::mounted()?2:0);
    listFiles();
    if(mode==2)Tab5Focus.editObject(dialogInput);
  }
  void listFiles() {
    if (!browser) return;
    setLabel(modalTitle, browseDir.c_str());
    // The location is user data, so it must not be retranslated on language changes.
    for (auto& item : captions) if (item.object == modalTitle) item.source = browseDir.c_str();
    if(!browser->open(browseDir.c_str(),browseMode!=2))Tab5Focus.focusInitial(modal,browseMediaButton);
  }
  void closeBrowseMedia(){
    if(!browseMedia)return;Tab5Focus.clear();lv_obj_delete(browseMedia);browseMedia=nullptr;if(browseMediaButton)Tab5Focus.focusObject(browseMediaButton);
  }
  void openBrowseMedia(){
    if(browseMedia){closeBrowseMedia();return;}
    browseMedia=lv_obj_create(modal);lv_obj_remove_style_all(browseMedia);lv_obj_set_size(browseMedia,lv_pct(100),lv_pct(100));lv_obj_remove_flag(browseMedia,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(browseMedia,[](lv_event_t* e){auto* app=static_cast<Impl*>(lv_event_get_user_data(e));if(lv_event_get_target(e)==app->browseMedia)app->closeBrowseMedia();},LV_EVENT_CLICKED,this);
    auto* box=lv_obj_create(browseMedia);panel(box);lv_obj_set_size(box,350,180);lv_obj_center(box);lv_obj_set_style_border_width(box,1,0);lv_obj_set_style_border_color(box,lv_color_hex(color(WritingColor::Muted)),0);
    auto* close=lv_button_create(box);lv_obj_set_size(close,48,48);lv_obj_set_pos(close,294,8);lv_obj_add_style(close,&buttonStyle,0);Tab5UiStyle::control(close);auto* icon=lv_label_create(close);lv_label_set_text(icon,LV_SYMBOL_CLOSE);lv_obj_set_style_text_font(icon,&lv_font_montserrat_24,0);lv_obj_center(icon);Tab5Focus.describe(close,tr("Fermer"));
    lv_obj_add_event_cb(close,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->closeBrowseMedia();},LV_EVENT_CLICKED,this);
    lv_obj_t* first=nullptr;int y=12;
    auto option=[&](const char* title,bool usb){auto* b=lv_button_create(box);lv_obj_set_pos(b,12,y);lv_obj_set_size(b,268,54);lv_obj_add_style(b,&buttonStyle,0);auto* label=lv_label_create(b);lv_label_set_text(label,tr(title));lv_obj_center(label);Tab5UiStyle::control(b);
      lv_obj_add_event_cb(b,[](lv_event_t* e){auto* app=instance;const bool usb=bool(reinterpret_cast<uintptr_t>(lv_event_get_user_data(e)));app->closeBrowseMedia();app->browseDir=usb?"/USB":"/";app->listFiles();},LV_EVENT_CLICKED,reinterpret_cast<void*>(uintptr_t(usb)));y+=64;if(!first)first=b;};
    if(tab5SdAvailable())option("Carte SD",false);if(Tab5UsbStorage::mounted())option("USB",true);Tab5Focus.focusInitial(browseMedia,first?first:close);
  }
  void serviceBrowser(){
    if(!browser)return;const unsigned media=(tab5SdAvailable()?1:0)|(Tab5UsbStorage::mounted()?2:0);
    if(media!=browseMediaState){browseMediaState=media;closeBrowseMedia();const bool usb=browseDir=="/USB"||browseDir.startsWith("/USB/");if((usb&&!(media&2))||(!usb&&!(media&1))){browseDir=media&1?"/":media&2?"/USB":browseDir;listFiles();}}
    if(browseMediaButton){if(media)lv_obj_remove_state(browseMediaButton,LV_STATE_DISABLED);else lv_obj_add_state(browseMediaButton,LV_STATE_DISABLED);}
    if(!browseMedia)browser->service();
  }
  void outline(bool ref = false) {
    if (headingsDirty) indexHeadings();
    if (!(ref ? referenceIndex : mainIndex).ready()) {
      dialog("Indexation en cours…");
      outlineAfterIndex = ref ? 2 : 1;
      return;
    }
    dialog(ref ? "Plan du document de droite" : "Plan du document");
    if (ref) {
      auto* choice = lv_dropdown_create(modal);
      lv_obj_set_pos(choice, 16, 52);
      lv_obj_set_size(choice, Width - 110, 42);
      String options;
      uint32_t selected = 0;
      for (size_t i = 0; i < references.size(); ++i) {
        if (i) options += "\n";
        options += references[i];
        if (references[i] == referencePath) selected = i;
      }
      lv_dropdown_set_options(choice, options.c_str());
      lv_dropdown_set_selected(choice, selected);
      lv_obj_add_style(choice, &buttonStyle, 0);
      lv_obj_add_event_cb(choice, [](lv_event_t* e) {
        auto* app = static_cast<Impl*>(lv_event_get_user_data(e));
        const size_t n = lv_dropdown_get_selected(static_cast<lv_obj_t*>(lv_event_get_target(e)));
        if (n >= app->references.size()) return;
        const String path = app->references[n];
        app->closeDialog();
        if (app->load(path, true)) app->outline(true);
      }, LV_EVENT_VALUE_CHANGED, this);
    }
    modalList = lv_obj_create(modal);
    panel(modalList, true);
    lv_obj_set_pos(modalList, 16, ref ? 104 : 56);
    lv_obj_set_size(modalList, Width - 32, FullHeight - (ref ? 118 : 70));
    lv_obj_set_style_pad_right(modalList, 32, 0);
    lv_obj_set_style_pad_row(modalList, 6, 0);
    lv_obj_set_flex_flow(modalList, LV_FLEX_FLOW_COLUMN);
    auto& hs = ref ? refHeadings : mainHeadings;
    size_t active = writing::Document::npos, at = ref ? refStart : cursor;
    for (auto& h : hs)
      if (h.offset <= at) active = h.offset;
    lv_obj_t* activeRow = nullptr;
    for (auto& h : hs) {
      auto* b = lv_button_create(modalList);
      panel(b);
      lv_obj_set_size(b, Width - 96, 46);
      label(b, h.title.c_str(), 12 + h.level * 14, 8, Width - 200, false);
      if (h.offset == active) {
        lv_obj_add_style(b, &selectedStyle, 0);
        activeRow = b;
      }
      auto* r = new (std::nothrow) Row{this, "", false, h.offset, true, ref};
      if (r) bindRow(b, r);
    }
    if (hs.size() == 4096)
      label(modalList, "4096 premiers titres affichés ; la suite reste accessible par défilement.",
            12, 0, Width - 100);
    if (hs.empty()) label(modalList, "Ajoutez un titre dans le menu Format.", 12, 0, Width - 100);
    if (activeRow) lv_obj_scroll_to_view(activeRow, LV_ANIM_OFF);
  }
  void searchDialog() {
    dialog("Rechercher / remplacer — document entier, casse respectée");
    dialogInput = field(modal, "Texte à rechercher", 20, 64, Width - 40);
    replaceInput = field(modal, "Texte de remplacement", 20, 124, Width - 40);
    target = dialogInput;
    lv_obj_t* b = button(modal, "Suivant", FindNext, 150);
    lv_obj_set_pos(b, 20, 190);
    b = button(modal, "Remplacer", ReplaceOne, 170);
    lv_obj_set_pos(b, 186, 190);
    b = button(modal, "Tout remplacer", ReplaceAll, 220);
    lv_obj_set_pos(b, 372, 190);
    label(modal,
          "Le remplacement global est annulable en une fois ; il peut être refusé si l’historique "
          "manque de place.",
          20, 246, Width - 40);
  }
  bool find(const char* needle, bool replace = false, const char* replacement = "") {
    if (!*needle) {
      note("Saisissez un texte à rechercher");
      return false;
    }
    size_t at = document.find(needle, cursor);
    if (at == writing::Document::npos) at = document.find(needle, 0);
    if (at == writing::Document::npos) {
      note("Aucune occurrence");
      return false;
    }
    size_t after = at + strlen(needle);
    if (replace) {
      if (!document.replace(at, after, replacement, strlen(replacement), at,
                            at + strlen(replacement))) {
        note("Remplacement refusé");
        return false;
      }
      after = at + strlen(replacement);
      lastEdit = millis();
      statsDirty = true;
    }
    closeDialog();
    cursor = after;
    start = document.lineStart(at);
    headingsDirty = true;
    render();
    view.anchor = view.text.indexAtSource(at - start);
    view.caret = view.text.indexAtSource(after - start);
    view.editing = true;
    view.wakeCaret();
    lv_obj_invalidate(view.object);
    return true;
  }
  void displaySelection(){
    if(!documentSelection)return;
    const size_t a=std::min(selectionAnchor,selectionCaret),b=std::max(selectionAnchor,selectionCaret);
    view.anchor=view.text.indexAtSource(a<=start?0:std::min(a-start,pageSource.size()));
    view.caret=view.text.indexAtSource(b<=start?0:std::min(b-start,pageSource.size()));
    lv_obj_invalidate(view.object);
  }
  bool replaceSelection(const char* value){
    if(!documentSelection)return false;
    const size_t a=std::min(selectionAnchor,selectionCaret),b=std::max(selectionAnchor,selectionCaret),n=strlen(value);
    if(!document.replace(a,b,value,n,cursor,a+n,millis())){note("Modification refusée : mémoire ou taille maximale");return false;}
    mainIndex.edited(a,b-a,n);syntaxIndex.edited(document,a);documentSelection=false;cursor=a+n;start=document.lineStart(cursor);
    statsDirty=sessionDirty=true;headingsDirty=true;lastEdit=millis();render();view.editing=true;return true;
  }
  void clipboardAction(bool cut) {
    if(documentSelection){
      size_t a=std::min(selectionAnchor,selectionCaret),b=std::max(selectionAnchor,selectionCaret);
      char* copied=static_cast<char*>(writing::allocate(b-a+1));if(!copied)return;
      document.copy(a,copied,b-a);copied[b-a]=0;bool ok=Tab5TextInput::copy(copied,b-a,!isCode());writing::release(copied);
      if(ok&&cut)replaceSelection("");return;
    }
    clipboardMarkdown = false;
    size_t a = std::min(view.anchor, view.caret), b = std::max(view.anchor, view.caret);
    for (size_t i = a; i < b; ++i)
      if (view.text.blockAt(i)) {
        clipboardMarkdown = true;
        break;
      }
    std::string text;
    if (clipboardMarkdown) {
      text = view.text.fragment(a, b);
    } else
      text = view.selected();
    if (text.empty()) {
      note("Sélectionnez du texte");
      return;
    }
    if(!Tab5TextInput::copy(text.data(),text.size(),clipboardMarkdown))return;
    if (cut) view.insert("");
  }
  void insertionRange() {
    blockStart = start + view.text.sourceAt(std::min(view.anchor, view.caret));
    blockEnd = start + view.text.sourceAt(std::max(view.anchor, view.caret));
  }
  bool replaceBlock(std::string text) {
    if (blockStart && document.at(blockStart - 1) != '\n') text = '\n' + text;
    if (!text.empty() && text.back() != '\n') text += '\n';
    size_t after = blockStart + text.size();
    if (!document.replace(blockStart, blockEnd, text.data(), text.size(), cursor, after,
                          millis())) {
      note("Insertion refusée : mémoire ou taille maximale");
      return false;
    }
    cursor = after;
    start = document.lineStart(blockStart);
    headingsDirty = statsDirty = sessionDirty = true;
    lastEdit = millis();
    render();
    view.editing = true;
    target = proxy;
    view.wakeCaret();
    return true;
  }
  void readTableFields() {
    // Each cell commits into tableCells. The document changes once at Apply,
    // so Cancel is lossless and Undo restores the entire previous table.
  }
  void updateCellSelection() {
    for (size_t i = 0; i < tableFields.size(); ++i) {
      auto* holder = lv_obj_get_parent(tableFields[i]->editor.object);
      lv_obj_set_style_border_color(holder, lv_color_hex(color(WritingColor::Accent)), LV_STATE_CHECKED);
      if (i == activeCell) lv_obj_add_state(holder, LV_STATE_CHECKED);
      else lv_obj_remove_state(holder, LV_STATE_CHECKED);
    }
  }
  void selectCell(CellEditor* cell) {
    for (size_t i = 0; i < tableFields.size(); ++i) {
      if (tableFields[i].get() == cell)
        activeCell = i;
      else
        tableFields[i]->editor.blur();
    }
    updateCellSelection();
    syncStyle();
  }
  void activateCell(CellEditor* cell) {
    selectCell(cell);
    cell->editor.editing = true;
    cell->editor.wakeCaret();
    target = proxy;
    Tab5Focus.clear();
    syncStyle();
    if (!writing::usbConnected())
      showKeyboard(true, proxy);
  }
  void buildTableGrid() {
    Tab5Focus.clear();
    if (tableGrid) lv_obj_delete(tableGrid);
    tableFields.clear();
    tableGrid = lv_obj_create(modal);
    panel(tableGrid, true);
    lv_obj_set_pos(tableGrid, 16, 110);
    lv_obj_set_size(tableGrid, Width - 32, std::max(120, height - 138));
    lv_obj_set_style_pad_right(tableGrid, 28, 0);
    int columns = tableCells[0].size(), cw = std::max(180, (Width - 76) / columns);
    for (size_t r = 0; r < tableCells.size(); ++r)
      for (int c = 0; c < columns; ++c) {
        auto cell = std::unique_ptr<CellEditor>(new CellEditor{this, r, size_t(c), {}});
        auto* holder = lv_obj_create(tableGrid);
        panel(holder, true);
        lv_obj_set_pos(holder, c * cw, r * 86);
        lv_obj_set_size(holder, cw - 8, 78);
        lv_obj_add_style(holder, &fieldStyle, 0);
        lv_obj_add_style(holder, r == 0 ? &referenceStyle : &buttonStyle, 0);
        lv_obj_add_style(holder, &selectedStyle, LV_STATE_CHECKED);
        lv_obj_set_style_pad_all(holder, 4, 0);
        lv_obj_set_style_border_width(holder, 1, 0);
        lv_obj_set_style_border_opa(holder, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(holder, 2, 0);
        cell->editor.context = cell.get();
        cell->editor.font = hooks.font;
        cell->editor.translate = hooks.translate;
        cell->editor.setTheme(viewTheme());
        cell->editor.fontSize = 20;
        cell->editor.singleLine = true;
        lv_obj_update_layout(holder);
        cell->editor.begin(holder);
        lv_obj_add_event_cb(cell->editor.object, [](lv_event_t* e) {
          auto* c = static_cast<CellEditor*>(lv_event_get_user_data(e));
          c->app->selectCell(c);
        }, LV_EVENT_FOCUSED, cell.get());
        lv_obj_add_event_cb(holder, [](lv_event_t* e) {
          auto* c = static_cast<CellEditor*>(lv_event_get_user_data(e));
          c->app->activateCell(c);
        }, LV_EVENT_CLICKED, cell.get());
        cell->editor.text.parseCell(tableCells[r][c]);
        cell->editor.reflow();
        cell->editor.changed = [](void* p) {
          auto* c = static_cast<CellEditor*>(p);
          auto md = c->editor.text.cellMarkdown();
          if (md.size() > 8192) {
            c->editor.text.parseCell(c->app->tableCells[c->row][c->col]);
            c->editor.move(c->editor.text.size());
            c->app->note("Cellule limitée à 8 Kio");
            return false;
          }
          c->app->tableCells[c->row][c->col] = std::move(md);
          c->app->syncStyle();
          return true;
        };
        cell->editor.activated = [](void* p) {
          auto* c = static_cast<CellEditor*>(p);
          c->app->activateCell(c);
        };
        tableFields.push_back(std::move(cell));
      }
    activeCell = std::min(activeCell, tableFields.size() - 1);
    updateCellSelection();
    lv_obj_update_layout(tableGrid);
    for (auto& cell : tableFields) cell->editor.reflow();
    Tab5Focus.focusObject(tableFields[activeCell]->editor.object);
    lv_obj_scroll_to_view(lv_obj_get_parent(tableFields[activeCell]->editor.object), LV_ANIM_OFF);
    lv_obj_invalidate(tableGrid);
  }
  void tableDialog(size_t at) {
    auto* block = view.text.blockAt(at);
    if (block && block->block != 1) return;
    if (block) {
      if (block->cells.size() > 16 || block->cells[0].size() > 6) {
        note("Édition : maximum 16 lignes et 6 colonnes par tableau");
        return;
      }
      blockStart = start + block->source;
      blockEnd = blockStart + block->original.size();
      tableCells = block->cells;
      tableAlignment = block->alignment;
    } else {
      insertionRange();
      tableCells = std::vector<std::vector<std::string>>(3, std::vector<std::string>(3));
      tableAlignment.clear();
    }
    tableOriginal = tableCells;
    view.blur();
    dialog("Tableau — première ligne : en-têtes");
    auto* b = button(modal, "L+", TableAddRow, 60);
    lv_obj_set_pos(b, 16, 52);
    b = button(modal, "L−", TableRemoveRow, 60);
    lv_obj_set_pos(b, 84, 52);
    b = button(modal, "C+", TableAddColumn, 60);
    lv_obj_set_pos(b, 152, 52);
    b = button(modal, "C−", TableRemoveColumn, 60);
    lv_obj_set_pos(b, 220, 52);
    cellBold = button(modal, "B", Bold);
    lv_obj_set_pos(cellBold, 330, 52);
    cellItalic = button(modal, "I", Italic);
    lv_obj_set_pos(cellItalic, 386, 52);
    b = button(modal, "H", Heading);
    lv_obj_set_pos(b, 442, 52);
    b = button(modal, "A", Colors);
    lv_obj_set_pos(b, 498, 52);
    b = button(modal, LV_SYMBOL_KEYBOARD, Keyboard, 56, true);
    lv_obj_set_pos(b, 568, 52);
    b = button(modal, "Appliquer", TableApply, 156);
    lv_obj_set_pos(b, Width - 196, 52);
    buildTableGrid();
  }
  void closeColors() {
    if (colorPanel) {
      auto* old = colorPanel;
      colorPanel = nullptr;
      colorsOpen = false;
      colorPreview = nullptr;
      Tab5Focus.clear();
      lv_obj_delete(old);
    } else if (colorsOpen) closeDialog();
  }
  void applyChosenColor(bool background) {
    closeColors();
    auto& editor = activeView();
    editor.color(chosenColors[background ? 1 : 0], background);
    editor.editing = true;
    editor.wakeCaret();
    syncStyle();
  }
  void updateColorPreview() {
    if (!colorPreview) return;
    lv_obj_set_style_text_color(colorPreview, lv_color_hex(
        chosenColors[0] == writing::NoColor ? color(WritingColor::Text) : chosenColors[0]), 0);
    lv_obj_set_style_bg_color(colorPreview, lv_color_hex(
        chosenColors[1] == writing::NoColor ? color(WritingColor::Surface) : chosenColors[1]), 0);
  }
  void colorsDialog() {
    if (colorsOpen) return;
    const bool cell = tableGrid != nullptr;
    lv_obj_t* host;
    if (cell) {
      colorPanel = lv_obj_create(modal);
      panel(colorPanel);
      lv_obj_set_pos(colorPanel, 8, 100);
      lv_obj_set_size(colorPanel, Width - 16, FullHeight - 108);
      host = colorPanel;
      label(host, "Réglage des couleurs", 20, 10, Width - 120);
      auto* close = lv_button_create(host);
      lv_obj_add_style(close, &buttonStyle, 0);
      lv_obj_set_size(close, 48, 40);
      auto* glyph = lv_label_create(close);
      lv_label_set_text(glyph, LV_SYMBOL_CLOSE);
      lv_obj_center(glyph);
      lv_obj_set_pos(close, Width - 84, 0);
      lv_obj_add_event_cb(close, [](lv_event_t* e) {
        static_cast<Impl*>(lv_event_get_user_data(e))->closeColors();
      }, LV_EVENT_CLICKED, this);
    } else {
      dialog("Réglage des couleurs");
      host = modal;
    }
    colorsOpen = true;
    static const uint32_t values[] = {writing::NoColor, 0xffffff, 0x101214, 0xf4c542, 0xf08080, 0xffa45b,
                                      0x8bd4b0, 0x74b9ff, 0xbfa4ee, 0x376b54, 0x314d80, 0x734357};
    const char* names[] = {"Automatique", "Blanc", "Noir", "Jaune", "Rouge", "Orange", "Vert clair",
                           "Bleu clair", "Violet", "Vert foncé", "Bleu foncé", "Prune"};
    struct Choice { Impl* app; unsigned row, index; uint32_t value; };
    for (unsigned row = 0; row < 2; ++row) {
      const int y = 72 + row * 166;
      label(host, row == 0 ? "Texte" : "Fond du texte", 20, y, 340);
      auto* apply = button(host, row ? "Fond du texte" : "Couleur du texte",
                           row ? ApplyBackgroundColor : ApplyTextColor, 420);
      lv_obj_set_pos(apply, 728, y - 8);
      for (unsigned n = 0; n < 12; ++n) {
        auto* b = lv_button_create(host);
        colorSwatches[row][n] = b;
        lv_obj_set_size(b, 76, 64);
        lv_obj_set_pos(b, 20 + n * 94, y + 46);
        lv_obj_set_style_bg_color(b, lv_color_hex(
            values[n] == writing::NoColor ? color(WritingColor::Surface) : values[n]), 0);
        auto* l = lv_label_create(b);
        lv_obj_set_style_text_font(l, font(24), 0);
        lv_label_set_text(l, chosenColors[row] == values[n] ? LV_SYMBOL_OK : n == 0 ? "↺" : "A");
        lv_obj_center(l);
        uint32_t rgb = values[n];
        bool light = ((rgb >> 16) & 255) * 299 + ((rgb >> 8) & 255) * 587 + (rgb & 255) * 114 > 140000;
        lv_obj_set_style_text_color(l, lv_color_hex(n == 0 ? color(WritingColor::Text) : light ? 0x101214 : 0xf1f3f5), 0);
        Tab5Focus.describe(b, tr(names[n]));
        auto* choice = new Choice{this, row, n, values[n]};
        lv_obj_add_event_cb(b, [](lv_event_t* e) {
          auto choice = *static_cast<Choice*>(lv_event_get_user_data(e));
          auto* app = choice.app;
          app->chosenColors[choice.row] = choice.value;
          for (unsigned i = 0; i < 12; ++i) {
            auto* l = lv_obj_get_child(app->colorSwatches[choice.row][i], 0);
            lv_label_set_text(l, i == choice.index ? LV_SYMBOL_OK : i == 0 ? "↺" : "A");
            lv_obj_center(l);
          }
          app->updateColorPreview();
        }, LV_EVENT_CLICKED, choice);
        lv_obj_add_event_cb(b, [](lv_event_t* e) { delete static_cast<Choice*>(lv_event_get_user_data(e)); },
                            LV_EVENT_DELETE, choice);
      }
    }
    colorPreview = label(host, "Aperçu des couleurs", 20, 430, 390);
    lv_obj_set_style_bg_opa(colorPreview, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(colorPreview, 8, 0);
    lv_obj_set_height(colorPreview, font(20)->line_height + 16);
    updateColorPreview();
    label(host, "Texte : Ctrl+Alt+C · Fond : Ctrl+Alt+H", 440, 430, 700);
    label(host, "Avec une sélection : appliquer. Sans sélection : continuer à écrire avec cette couleur.",
          20, 490, Width - 60);
  }
  bool copyImage(const String& source, const String& destinationFolder, String& destination,
                 const String& exactName = "") {
    if (!Tab5Files::userWritable(destinationFolder.c_str())) {
      note("Dossier invalide");
      return false;
    }
    File in = tab5FS.open(source.c_str(), FILE_READ);
    if (!in || in.isDirectory() || !in.size() || in.size() > 4 * 1024 * 1024) {
      if (in) in.close();
      note("Image inaccessible ou supérieure à 4 Mio");
      return false;
    }
    String suffix = source.substring(source.lastIndexOf('.'));
    suffix.toLowerCase();
    bool present=false;
    if (!fs.probe(destinationFolder.c_str(),present) || (!present&&!tab5FS.mkdir(destinationFolder.c_str()))) {
      in.close();
      note("Dossier images inaccessible");
      return false;
    }
    if (exactName.length())
      destination = join(destinationFolder, exactName);
    else {
      for (unsigned n = 1; n < 10000; ++n) {
        char name[32];
        snprintf(name, sizeof(name), "image-%04u", n);
        destination = join(destinationFolder, String(name) + suffix);
        if(!fs.probe(destination.c_str(),present)){in.close();return false;}
        if (!present) break;
        destination = "";
      }
    }
    if (!Tab5Files::userWritable(destination.c_str())) {
      in.close();
      note("Chemin de fichier invalide");
      return false;
    }
    if(!fs.probe(destination.c_str(),present)){in.close();return false;}
    if (exactName.length() && present) {
      size_t length = in.size();
      std::vector<uint8_t> bytes(4096);
      uint32_t first = 2166136261u, second = 2166136261u;
      size_t done = 0;
      while (done < length) {
        size_t n = in.read(bytes.data(), std::min(bytes.size(), length - done));
        if (!n) {
          in.close();
          return false;
        }
        for (size_t i = 0; i < n; ++i) first = (first ^ bytes[i]) * 16777619u;
        done += n;
        delay(1);
      }
      in.close();
      File check = tab5FS.open(destination.c_str(), FILE_READ);
      if (!check || check.isDirectory() || check.size() != length) {
        if (check) check.close();
        note("Une image différente existe dans le dossier cible");
        return false;
      }
      done = 0;
      while (done < length) {
        size_t n = check.read(bytes.data(), std::min(bytes.size(), length - done));
        if (!n) {
          check.close();
          return false;
        }
        for (size_t i = 0; i < n; ++i) second = (second ^ bytes[i]) * 16777619u;
        done += n;
        delay(1);
      }
      check.close();
      if (first != second) note("Une image différente existe dans le dossier cible");
      return first == second;
    }
    if (!destination.length()) {
      in.close();
      return false;
    }
    String temporary = destination + ".tmp";
    if (!fs.probe(temporary.c_str(),present)||present) {
      in.close();
      note("Une copie temporaire existe déjà");
      return false;
    }
    File out = tab5FS.open(temporary.c_str(), FILE_WRITE);
    if (!out) {
      in.close();
      return false;
    }
    uint8_t buffer[4096];
    size_t expected = in.size(), done = 0;
    uint32_t hash = 2166136261u;
    bool good = true;
    while (done < expected) {
      size_t n = in.read(buffer, std::min(sizeof(buffer), expected - done));
      if (!n || out.write(buffer, n) != n) {
        good = false;
        break;
      }
      for (size_t i = 0; i < n; ++i) hash = (hash ^ buffer[i]) * 16777619u;
      done += n;
      delay(1);
    }
    in.close();
    errno=0;out.flush();good=good&&!errno;
    errno=0;out.close();good=good&&!errno;
    File check = tab5FS.open(temporary.c_str(), FILE_READ);
    uint32_t verified = 2166136261u;
    size_t read = 0;
    if (!check || check.size() != expected) good = false;
    while (good && read < expected) {
      size_t n = check.read(buffer, std::min(sizeof(buffer), expected - read));
      if (!n) {
        good = false;
        break;
      }
      for (size_t i = 0; i < n; ++i) verified = (verified ^ buffer[i]) * 16777619u;
      read += n;
      delay(1);
    }
    check.close();
    if (!good || hash != verified || !fs.probe(destination.c_str(),present)||present || !tab5FS.rename(temporary.c_str(), destination.c_str())) {
      tab5FS.remove(temporary.c_str());
      note("Copie image non vérifiée : insertion annulée");
      return false;
    }
    return true;
  }
  bool copyRelativeImages(const String& destinationParent) {
    for (size_t pos = 0; pos < document.size();) {
      size_t finish = pos;
      while (finish < document.size() && document.at(finish) != '\n') ++finish;
      if (finish - pos < 2048 && document.at(pos) == '!' && document.at(pos + 1) == '[' &&
          !inFence(document, pos)) {
        writing::RichText line;
        line.parse(slice(document, pos, finish));
        if (!line.lines.empty() && line.lines[0].block == 2) {
          std::string rel = line.lines[0].imagePath;
          for (size_t i = 0; i + 2 < rel.size(); ++i)
            if (rel[i] == '%') {
              auto hex = [](char c) -> int {
                return c >= '0' && c <= '9'   ? c - '0'
                       : c >= 'a' && c <= 'f' ? c - 'a' + 10
                       : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                              : -1;
              };
              int a = hex(rel[i + 1]), b = hex(rel[i + 2]);
              if (a >= 0 && b >= 0 && a * 16 + b) rel.replace(i, 3, 1, char(a * 16 + b));
            }
          if (!rel.empty() && rel[0] != '/' && rel.find("://") == std::string::npos) {
            while (rel.rfind("./", 0) == 0) rel.erase(0, 2);
            if (rel.rfind("../", 0) == 0 || rel.find("/../") != std::string::npos) {
              note("Réimportez l’image externe avant de changer de dossier");
              return false;
            }
            String relative = rel.c_str(), folder = destinationParent;
            std::string sub =
                parentOf(relative).c_str();  // parentOf returns / when there is no subfolder.
            if (sub != "/")
              for (size_t a = 0; a < sub.size();) {
                size_t end = sub.find('/', a);
                if (end == std::string::npos) end = sub.size();
                folder = join(folder, String(sub.substr(a, end - a).c_str()));
                if (!tab5FS.exists(folder.c_str()) && !tab5FS.mkdir(folder.c_str())) return false;
                a = end + 1;
              }
            String target;
            if (!copyImage(join(parentOf(path), relative), folder, target, baseOf(relative)))
              return false;
          }
        }
      }
      pos = finish + (finish < document.size() ? 1 : 0);
    }
    return true;
  }
  void importImage(const String& source) {
    if (saveJob.busy()) {
      note("Attendez la fin de la sauvegarde");
      return;
    }
    if (!ensurePath()) return;
    lv_image_dsc_t preview{};
    void* handle = nullptr;
    if (!hooks.imageLoad || !hooks.imageLoad(source.c_str(), 600, 240, &preview, &handle)) {
      note("Image illisible ou trop grande (JPEG / PNG / BMP, 4 Mio)");
      return;
    }
    if (handle && hooks.imageFree) hooks.imageFree(handle);
    String destination, folder = path + ".assets";
    if (!copyImage(source, folder, destination)) return;
    insertionRange();
    std::string relative = urlPath(baseOf(folder) + "/" + baseOf(destination));
    replaceBlock("![Image](" + relative + ")\n");
  }
  void languageDialog() {
    dialog("Langage du document");
    auto* hint=label(modal,"Le langage choisi règle uniquement la coloration de ce document.",20,62,Width-40);
    lv_label_set_long_mode(hint,LV_LABEL_LONG_WRAP);
    auto* choice=lv_dropdown_create(modal);String names;
    for(unsigned i=0;i<unsigned(writing::SyntaxLanguage::Count);++i){if(i)names+='\n';names+=tr(writing::syntaxLanguageName(writing::SyntaxLanguage(i)));}
    lv_dropdown_set_options(choice,names.c_str());lv_dropdown_set_selected(choice,unsigned(syntaxChoice));
    lv_obj_set_pos(choice,20,130);lv_obj_set_size(choice,std::min(650,Width-40),52);Tab5UiStyle::control(choice);
    lv_obj_set_style_text_font(choice,font(22),0);lv_obj_add_style(choice,&fieldStyle,0);
    lv_obj_add_event_cb(choice,[](lv_event_t* e){
      auto* a=static_cast<Impl*>(lv_event_get_user_data(e));const auto selected=lv_dropdown_get_selected(lv_event_get_target_obj(e));
      if(selected>=unsigned(writing::SyntaxLanguage::Count))return;
      const int32_t scroll=lv_obj_get_scroll_y(a->mainScroll);
      const size_t caret=a->view.text.sourceAt(a->view.caret),anchor=a->view.text.sourceAt(a->view.anchor);
      a->cursor=a->start+caret;a->syntaxChoice=writing::SyntaxLanguage(selected);a->syntaxIndex.reset();
      a->render();a->view.anchor=a->view.text.indexAtSource(anchor);a->view.caret=a->view.text.indexAtSource(caret);
      lv_obj_scroll_to_y(a->mainScroll,scroll,LV_ANIM_OFF);a->capturePosition();a->persistWorkspace();a->sessionDirty=true;
    },LV_EVENT_VALUE_CHANGED,this);
    Tab5Focus.focusObject(choice);
  }
  void encodingDialog(String requested = "", bool ref = false, int project = -1) {
    dialog(requested.length()?"Choisir l'encodage à l'ouverture":"Encodage du texte");
    encodingOpenPath=requested;encodingOpenReference=ref;encodingOpenProject=project;
    auto* hint=label(modal,requested.length()?"UTF-8 sans signature ou Windows-1252 : choisissez explicitement":"L'encodage choisi sera utilisé à l'enregistrement",20,62,Width-40);
    lv_label_set_long_mode(hint,LV_LABEL_LONG_WRAP);
    const char* names[]={"UTF-8","UTF-8 BOM","UTF-16 LE","UTF-16 BE","Windows-1252 (ANSI occidental)"};
    for(unsigned i=0;i<5;++i){auto* b=button(modal,names[i],EncodingUtf8+i,std::min(510,Width-40));lv_obj_set_pos(b,20,126+int(i)*58);lv_obj_set_height(b,48);
      if(!requested.length()&&unsigned(document.encoding())==i+1)lv_obj_add_state(b,LV_STATE_CHECKED);}
    Tab5Focus.focusFirst(modal);
  }
  void chooseEncoding(writing::TextEncoding encoding) {
    if(encodingOpenPath.length()){
      String requested=encodingOpenPath;bool ref=encodingOpenReference;int project=encodingOpenProject;
      closeDialog();load(requested,ref,project,encoding);return;
    }
    if(saveJob.busy()){note("Attendez la fin de la sauvegarde");return;}
    if(encoding==writing::TextEncoding::Windows1252){
      char* snapshot=document.snapshot();if(!snapshot){note("PSRAM insuffisante pour sauvegarder");return;}
      writing::TextEncoder encoder;encoder.reset(snapshot,document.size(),encoding);char block[4096];bool ok=true;
      do{size_t n=0;if(!encoder.next(block,sizeof(block),n)){ok=false;break;}yieldSd();}while(!encoder.done());
      writing::release(snapshot);if(!ok){note("Caractère non représentable dans cet encodage");return;}
    }
    document.setEncoding(encoding);lastEdit=millis();closeDialog();cachedTitleCursor=SIZE_MAX;updateStatus();
  }
  void validateJsonDocument(){
    auto result=writing::validateJson(document);
    if(result.valid){note("JSON valide");return;}
    size_t line=1,column=1;for(size_t i=0;i<result.offset&&i<document.size();++i){if(document.at(i)=='\n'){++line;column=1;}else if((uint8_t(document.at(i))&0xc0)!=0x80)++column;}
    cursor=std::min(result.offset,document.size());start=document.lineStart(cursor);view.blur();render();
    note((String(tr(result.message))+" · "+tr("Ligne")+" "+String(unsigned(line))+" : "+String(unsigned(column))).c_str());
  }
  void act(int action) {
    if(action>=EncodingUtf8&&action<=EncodingWindows1252){chooseEncoding(static_cast<writing::TextEncoding>(action-EncodingUtf8+1));return;}
    if (action >= MenuFile && action <= MenuCode) { toggleMenu(action); return; }
    closeMenu(false);
    switch (action) {
      case Help:
        if (hooks.help) hooks.help();
        break;
      case SelectAll:
        view.anchor = 0;
        view.caret = view.text.size();
        lv_obj_invalidate(view.object);
        break;
      case Table:
        if (isCode()) break;
        tableDialog(view.caret);
        break;
      case Image:
        if (isCode()) break;
        browse(3);
        break;
      case ApplyTextColor:
      case ApplyBackgroundColor:
        applyChosenColor(action == ApplyBackgroundColor);
        break;
      case Colors:
        if (isCode()) break;
        colorsDialog();
        break;
      case TableApply: {
        readTableFields();
        auto md = writing::tableMarkdown(tableCells, tableAlignment);
        closeDialog();
        replaceBlock(md);
        break;
      }
      case TableAddRow:
      case TableAddColumn:
      case TableRemoveRow:
      case TableRemoveColumn:
        readTableFields();
        if (action == TableAddRow && tableCells.size() < 16)
          tableCells.push_back(std::vector<std::string>(tableCells[0].size()));
        if (action == TableAddColumn && tableCells[0].size() < 6)
          for (auto& row : tableCells) row.push_back("");
        if (action == TableRemoveRow && tableCells.size() > 1) tableCells.pop_back();
        if (action == TableRemoveColumn && tableCells[0].size() > 1)
          for (auto& row : tableCells) row.pop_back();
        if (action == TableAddRow) activeCell = (tableCells.size()-1)*tableCells[0].size();
        if (action == TableAddColumn) activeCell = tableCells[0].size()-1;
        buildTableGrid();
        break;
      case Documents:
        toggleMenu(MenuDocuments);
        break;
      case DocumentsNext:
        documentsPage += 16; toggleMenu(MenuDocuments);
        break;
      case ProjectNew:
        projectDialog();
        break;
      case ProjectCreate: {
        if (!dialogInput) break;
        String name = lv_textarea_get_text(dialogInput); name.trim();
        if (!name.length()) break;
        if (path.length() && !save("",true)) break;
        if (!path.length() && document.size() && !save("",true)) break;
        auto id = workspace.createProject(name.c_str(),"");
        if (!id) { note("Impossible de créer ce projet : 8 projets au maximum"); break; }
        if (path.length()) workspace.add(path.c_str(),id);
        workspace.setActiveProject(id);
        const uint8_t previousProject = currentDocumentProject;
        if (path.length()) { currentDocumentProject = id; capturePosition(); }
        if (!persistWorkspace()) { currentDocumentProject = previousProject; break; }
        sessionDirty = true; closeDialog(); toggleMenu(MenuDocuments);
        break;
      }
      case ProjectAdd:
        addToProject();
        break;
      case ProjectImport:
        browse(4);
        break;
      case ProjectUseFolder: {
        const String folder = browseDir;
        auto id = workspace.activeProject();
        if (!id) id = workspace.createProject(baseOf(folder).length() ? baseOf(folder).c_str() : "Project",folder.c_str());
        if (!id) { note("Impossible de créer ce projet : 8 projets au maximum"); break; }
        workspace.setActiveProject(id);
        if (!workspaceScanner.startScan(id,folder.c_str())) { note("Dossier inaccessible"); break; }
        persistWorkspace(); closeDialog(); toggleMenu(MenuDocuments);
        note("Import du dossier en cours…");
        break;
      }
      case TextEncodingDialog: encodingDialog(); break;
      case TextReopenEncoding: if(path.length()&&!document.dirty()&&!saveJob.busy())encodingDialog(path,false,currentDocumentProject); break;
      case CodeLanguage: languageDialog(); break;
      case CodeNewC: create("/Projects",true,".c"); break;
      case CodeNewJson: create("/Documents",true,".json"); break;
      case CodeValidateJson: validateJsonDocument(); break;
      case CodeNew:
        create("/MicroPython", true);
        break;
      case CodeRun:
        runCode();
        break;
      case CodeStop:
        if (hooks.stop) hooks.stop();
        break;
      case CodeResume:
        if (hooks.resume) hooks.resume();
        break;
      case CodeLibraries:
        if (hooks.libraries) hooks.libraries();
        break;
      case SerialToggle: serialToggle(); break;
      case SerialListen: case SerialStop: case SerialSend: case SerialCancel: case SerialClear: case SerialInsert: case SerialData: serialAction(action); break;
      case CodeConsole:
        serialVisible=false;lastLayoutHeight=-1;
        consoleVisible = !consoleVisible;
        layout();
        serviceConsole(millis(), true);
        break;
      case CodeClearConsole:
        Tab5ContextMenu::clearOutput(consoleText);
        break;
      case New:
        create(browseDir);
        break;
      case Open:
        browse(0);
        break;
      case Reference:
        browse(1);
        break;
      case Print:
        if(document.encoding()!=writing::TextEncoding::Utf8){note("Pour imprimer, enregistrez une copie en UTF-8");break;}
        printAfterSave = true;
        if (!save()) printAfterSave = false;
        break;
      case Save:
        save();
        break;
      case SaveAs:
        browse(2);
        break;
      case Undo:
      case Redo:
        if ((action == Undo ? document.undo(cursor) : document.redo(cursor))) {
          documentSelection=false;
          start = document.lineStart(cursor);
          lastEdit = millis();
          headingsDirty = statsDirty = true;
          view.blur();
          render();
        }
        break;
      case Keyboard:
        activeView().editing = true;
        target = proxy;
        showKeyboard(!keyboard, proxy);
        break;
      case Search:
        searchDialog();
        break;
      case Outline:
        if (isCode()) toggleMenu(MenuDocuments); else outline();
        break;
      case Prev:
        navigate(false, -1);
        break;
      case Next:
        navigate(false, 1);
        break;
      case RefPrev:
        navigate(true, -1);
        break;
      case RefNext:
        navigate(true, 1);
        break;
      case RefClose:
        split = false;
        consoleVisible = false;
        serialVisible = false;
        lastLayoutHeight = -1;
        layout();
        break;
      case RefOutline:
        outline(true);
        break;
      case Smaller:
        if (fontSize > 18) fontSize -= 2;
        view.fontSize = refView.fontSize = fontSize;
        view.reflow();
        refView.reflow();
        break;
      case Larger:
        if (fontSize < 36) fontSize += 2;
        view.fontSize = refView.fontSize = fontSize;
        view.reflow();
        refView.reflow();
        break;
      case LineNumbers:
        lineNumbers=!lineNumbers;render();if(split)renderReference();break;
      case Underline:case Strike:
        activeView().format(action==Underline?8:16);syncStyle();break;
      case Bold:
        activeView().format(1);
        syncStyle();
        break;
      case Italic:
        activeView().format(2);
        syncStyle();
        break;
      case Heading:
        activeView().heading();
        break;
      case Copy:
        clipboardAction(false);
        break;
      case Cut:
        clipboardAction(true);
        break;
      case Paste:
        if(documentSelection){replaceSelection(Tab5TextInput::clipboard());break;}
        if (*Tab5TextInput::clipboard()) {
          if (Tab5TextInput::markdown() && !isCode()) {
            insertionRange();
            replaceBlock(Tab5TextInput::clipboard());
          } else
            view.insert(Tab5TextInput::clipboard());
        }
        break;
      case CloseDialog:
        closeDialog();
        break;
      case ParentFolder:
        browseDir = parentOf(browseDir);
        listFiles();
        break;
      case BrowseStorage:openBrowseMedia();break;
      case ChooseName: {
        if (!dialogInput) break;
        String name = lv_textarea_get_text(dialogInput);
        name.trim();
        if (!name.length() || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 || name == "." ||
            name == ".." || name.indexOf(':') >= 0) {
          note("Nom de fichier invalide");
          break;
        }
        if (!textFile(name)) name += defaultExtension();
        String chosen = join(browseDir, name);
        if (tab5FS.exists(chosen.c_str()) && chosen != path) {
          note("Ce nom existe déjà ; choisissez un autre nom");
          break;
        }
        closeDialog();
        save(chosen);
        break;
      }
      case FindNext:
      case ReplaceOne:
      case ReplaceAll: {
        if (!dialogInput) break;
        String needle = lv_textarea_get_text(dialogInput),
               repl = lv_textarea_get_text(replaceInput);
        if (action == ReplaceAll) {
          size_t count = 0;
          if (document.replaceAll(needle.c_str(), repl.c_str(), count)) {
            closeDialog();
            start = cursor = 0;
            lastEdit = millis();
            headingsDirty = statsDirty = true;
            render();
            note((String(count) + " " + tr("remplacement(s)")).c_str());
          } else
            note("Remplacement global refusé : recherche vide, taille ou historique");
        } else
          find(needle.c_str(), action == ReplaceOne, repl.c_str());
        break;
      }
      case RestoreBackup: {
        if (saveJob.busy() || !path.length()) {
          note("Sauvegarde en cours ou document sans nom");
          break;
        }
        char name[320];writing::backupPath(path.c_str(),name,sizeof(name));String backup=name;if(!tab5FS.exists(backup.c_str()))backup=path+".writer.bak";
        writing::Document old;
        bool recovered = false;
        if (!writing::loadDocument(fs, backup.c_str(), old, recovered, yieldSd)) {
          note("Aucune version précédente lisible");
          break;
        }
        char* text = old.snapshot();
        if (text && document.replace(0, document.size(), text, old.size(), cursor, 0)) {
          cursor = start = 0;
          lastEdit = millis();
          headingsDirty = statsDirty = true;
          render();
          note("Version précédente restaurée — Annuler reste possible");
        } else
          note("Restauration refusée : historique insuffisant");
        writing::release(text);
        break;
      }
      default:
        break;
    }
  }
  bool key(const writing::KeyEvent& e, lv_obj_t* neighbor) {
    if (!visible) return false;
    using K = writing::Key;
    if(!modal&&!menuPanel){
      const bool command=e.ctrl||e.meta;
      if(e.key==K::Text&&command&&e.shift&&e.text[0]=='n'){act(LineNumbers);return true;}
      if(e.key==K::Text&&command&&e.text[0]=='u'){act(Underline);return true;}
      if(e.key==K::Text&&command&&e.shift&&e.text[0]=='x'){act(Strike);return true;}
      if(e.key==K::Insert&&command){act(Copy);return true;}
      if(e.key==K::Insert&&e.shift){act(Paste);return true;}
      if(e.key==K::Delete&&e.shift){act(Cut);return true;}
      if(e.key==K::Text&&command&&e.text[0]=='a'){
        selectionAnchor=0;selectionCaret=document.size();documentSelection=true;view.editing=true;displaySelection();return true;
      }
      if((e.key==K::Home||e.key==K::End)&&command){
        if(e.shift){if(!documentSelection)selectionAnchor=start+view.text.sourceAt(view.anchor);documentSelection=true;}
        else documentSelection=false;
        cursor=e.key==K::Home?0:document.size();selectionCaret=cursor;
        start=e.key==K::Home?0:document.lineStart(document.size());
        if(start==document.size()&&start)start=document.lineStart(document.prevChar(start));
        render();view.editing=true;displaySelection();syncStyle();return true;
      }
      if(e.shift&&(e.key==K::PageUp||e.key==K::PageDown)){
        if(!documentSelection)selectionAnchor=start+view.text.sourceAt(view.anchor);documentSelection=true;
        navigate(false,e.key==K::PageUp?-1:1,true);selectionCaret=cursor;view.editing=true;displaySelection();return true;
      }
      if(documentSelection){
        if(e.key==K::Backspace||e.key==K::Delete){replaceSelection("");return true;}
        if(e.key==K::Text&&!command&&!e.alt){replaceSelection(e.text);return true;}
        if(e.key==K::Enter){replaceSelection("\n");return true;}
        if(e.key==K::Tab&&!command){replaceSelection("    ");return true;}
        if(e.key==K::Escape)documentSelection=false;
        if(e.key==K::Left||e.key==K::Right||e.key==K::Up||e.key==K::Down){
          size_t dest=selectionCaret;
          if(e.shift){if(e.key==K::Left)dest=document.prevChar(dest);else if(e.key==K::Right)dest=document.nextChar(dest);else {size_t col=dest-document.lineStart(dest);size_t line=document.lineStart(dest);if(e.key==K::Up)line=line?document.lineStart(document.prevChar(line)):0;else {while(line<document.size()&&document.at(line)!='\n')++line;if(line<document.size())++line;}dest=line;while(dest<document.size()&&dest-line<col&&document.at(dest)!='\n')dest=document.nextChar(dest);}}
          else {dest=e.key==K::Left||e.key==K::Up?std::min(selectionAnchor,selectionCaret):std::max(selectionAnchor,selectionCaret);documentSelection=false;}
          cursor=selectionCaret=dest;if(dest<start||dest>end)start=document.lineStart(dest);render();view.editing=true;displaySelection();return true;
        }
      }
    }
    if (!modal && (e.ctrl || e.meta)) {
      if (e.key == K::Tab) { closeMenu(false); cycleDocument(e.shift ? -1 : 1); return true; }
      if (e.key == K::Enter) { act(CodeRun); return true; }
      if (e.key == K::Text && e.text[0] == '.') { act(CodeStop); return true; }
      if (e.key == K::Text && e.shift && e.text[0] == 'r') { act(CodeResume); return true; }
      if (e.key == K::Text && ((e.shift && e.text[0] == 'l') || e.text[0] == '`')) { act(CodeConsole); return true; }
    }
    if (menuPanel) {
      if (e.key == K::Escape) { closeMenu(true); return true; }
      if (e.key == K::Left || e.key == K::Right) {
        int next = (openMenu - MenuFile + (e.key == K::Right ? 1 : 7)) % 8;
        toggleMenu(MenuFile + next);
        return true;
      }
      if (Tab5Focus.key(e, menuPanel)) return true;
      if (!(e.ctrl || e.meta)) return true;
      closeMenu(false);
    }
    if ((e.ctrl || e.meta) && e.key == K::Text) {
      if (e.alt && (e.text[0] == 'c' || e.text[0] == 'h')) {
        if (!modal || colorsOpen || tableGrid) applyChosenColor(e.text[0] == 'h');
        return true;
      }
      if (e.shift && e.text[0] == 'c' && tableGrid) { colorsDialog(); return true; }
    }
    if (modal) {
      if(browseMedia){if(e.key==K::Escape)closeBrowseMedia();else Tab5Focus.key(e,browseMedia);return true;}
      if(browser){
        if(e.key==K::F5){listFiles();return true;}
        auto* active=Tab5Focus.focused();const bool input=Tab5Focus.controlling()||(active&&lv_obj_is_valid(active)&&lv_obj_check_type(active,&lv_textarea_class));
        if((e.alt&&e.key==K::Left)||(!input&&!e.ctrl&&!e.meta&&e.key==K::Backspace)){act(ParentFolder);return true;}
        if(browser->key(e))return true;
      }
      if (colorPanel) {
        if (e.key == K::Escape) {
          closeColors();
        } else
          Tab5Focus.key(e, colorPanel);
        return true;
      }
      if (tableGrid && !tableFields.empty()) {
        auto& editor = activeView();
        if ((e.ctrl || e.meta) && e.key == K::Text) {
          if (e.text[0] == 'u' || (e.shift && e.text[0] == 'x')) {editor.format(e.text[0] == 'u'?8:16);syncStyle();return true;}
          if (e.text[0] == 'b') {
            editor.format(1);
            syncStyle();
            return true;
          }
          if (e.text[0] == 'i') {
            editor.format(2);
            syncStyle();
            return true;
          }
          if (e.text[0] == 'a') {
            editor.anchor = 0;
            editor.caret = editor.text.size();
            lv_obj_invalidate(editor.object);
            return true;
          }
          if (e.text[0] == 'c' || e.text[0] == 'x') {
            auto selected = editor.selected();
            const bool copied=Tab5TextInput::copy(selected.data(),selected.size());
            if (e.text[0] == 'x' && copied) editor.insert("");
            return true;
          }
          if (e.text[0] == 'v') {
            if (*Tab5TextInput::clipboard()) editor.insert(Tab5TextInput::clipboard());
            return true;
          }
        }
        if (editor.editing) {
          if (e.key == K::Escape || e.key == K::Enter || e.key == K::Tab) {
            editor.blur();
            target = nullptr;
            Tab5Focus.focusObject(editor.object);
            if (e.key == K::Tab) Tab5Focus.key(e, modal);
          } else
            editor.key(e);
          syncStyle();
          return true;
        }
      }
      if (e.key == K::Escape) {
        if (Tab5Focus.controlling() && Tab5Focus.key(e, modal)) return true;
        closeDialog();
        return true;
      }
      if (Tab5Focus.key(e, modal)) return true;
      if (!Tab5Focus.focused() && target && lv_obj_is_valid(target) &&
          lv_obj_check_type(target, &lv_textarea_class))
        return Tab5Focus.edit(target, e);
      return false;
    }
    if ((e.ctrl || e.meta) && e.key == K::Tab) { cycleDocument(e.shift ? -1 : 1); return true; }
    if ((e.ctrl || e.meta) && e.key == K::Enter) { act(CodeRun); return true; }
    if ((e.ctrl || e.meta) && e.key == K::Text) {
      char c = e.text[0];
      if (c == '.') { act(CodeStop); return true; }
      if (c == 'r' && e.shift) { act(CodeResume); return true; }
      if ((c == 'l' && e.shift) || c == '`') { act(CodeConsole); return true; }
      if ((c == 'l' || c == 'L') && !e.shift && consoleVisible) { act(CodeClearConsole); return true; }
      if (c == 's')
        act(e.shift ? SaveAs : Save);
      else if (c == 'p')
        act(Print);
      else if (c == 'o')
        act(e.shift ? Documents : Open);
      else if (c == 'n')
        act(New);
      else if (c == 'z')
        act(e.shift ? Redo : Undo);
      else if (c == 'y')
        act(Redo);
      else if (c == 'f' || c == 'h')
        act(Search);
      else if (c == 'b')
        act(Bold);
      else if (c == 'i')
        act(e.shift ? Image : Italic);
      else if (c == 't')
        act(Table);
      else if (c == 'c' && e.shift)
        act(Colors);
      else if (c == 'c')
        act(Copy);
      else if (c == 'x')
        act(Cut);
      else if (c == 'v')
        act(Paste);
      else if (c == 'a') {
        view.anchor = 0;
        view.caret = view.text.size();
        lv_obj_invalidate(view.object);
      } else
        return false;
      return true;
    }
    if (e.key == K::Escape) {
      if (view.editing) {
        view.blur();
        showKeyboard(false);
        target = nullptr;
        Tab5Focus.focusFirst(toolbar);
        return true;
      }
      return false;
    }
    if (!view.editing) {
      if (Tab5Focus.key(e, root, proxy, neighbor)) return true;
      if (e.key == K::Enter && !Tab5Focus.focused()) {
        view.editing = true;
        target = proxy;
        Tab5Focus.clear();
        view.wakeCaret();
        return true;
      }
      if (Tab5Focus.focused()) return false;
      if (e.key != K::Text && !(e.ctrl && (e.key == K::Home || e.key == K::End))) return false;
      view.editing = true;
      target = proxy;
      Tab5Focus.clear();
    }
    if (e.key == K::PageUp || e.key == K::PageDown) {
      navigate(false, e.key == K::PageUp ? -1 : 1, true);
      return true;
    }
    if (e.ctrl && (e.key == K::Home || e.key == K::End)) {
      cursor = e.key == K::Home ? 0 : document.size();
      start = e.key == K::Home ? 0 : document.lineStart(document.size());
      if (e.key == K::End && start == document.size() && start)
        start = document.lineStart(document.prevChar(start));
      render();
      view.editing = true;
      view.move(e.key == K::Home ? 0 : view.text.size(), e.shift);
      return true;
    }
    if (!e.shift && e.key == K::Left && view.caret == 0 && start) {
      navigate(false, -1, true);
      view.move(view.text.size());
      view.editing = true;
      return true;
    }
    if (!e.shift && e.key == K::Right && view.caret == view.text.size() && end < document.size()) {
      navigate(false, 1, true);
      view.move(0);
      view.editing = true;
      return true;
    }
    view.key(e);
    cursor = start + view.text.sourceAt(view.caret);
    syncStyle();
    sessionDirty = true;
    return true;
  }
  String sessionState() const {
    String state = String("WRITER084\n") + String(currentDocumentProject) + "\n" + path + "\n" + referencePath + "\n" + String(static_cast<unsigned>(start)) + "\n" +
                   String(static_cast<unsigned>(cursor)) + "\n" + String(split ? 1 : 0) + "\n";
    for (const auto& ref : references) state += ref + "\n";
    return state;
  }
  void saveSession() {
    if (saveJob.busy() || sessionJob.busy()) return;
    capturePosition();
    if (!persistWorkspace()) return;
    String state = sessionState();
    if (state == savedSession) { sessionDirty = false; return; }
    if (!sessionDocument.assign(state.c_str(), state.length())) {
      sessionRetryDelay = 30000;
      return;
    }
    tab5FS.mkdir("/System");
    tab5FS.mkdir("/System/writing");
    pendingSession = state;
    sessionObserved = false;
    if (!sessionJob.begin(sessionDocument, "/System/writing/session.txt")) {
      sessionObserved = true;
      sessionRetryDelay = 30000;
    }
  }
  void serviceSession() {
    // One verified-save step per loop: session persistence never monopolizes
    // the UI task, and repeated redraws do not rewrite identical session data.
    if (sessionJob.busy()) sessionJob.step();
    if (sessionObserved || sessionJob.busy()) return;
    sessionObserved = true;
    if (sessionJob.state() == writing::SaveJob::State::Done) {
      savedSession = pendingSession;
      if (sessionState() == savedSession) sessionDirty = false;
      sessionRetryDelay = 5000;
    } else {
      sessionRetryDelay = 30000;
    }
    pendingSession = "";
  }
  void resume() {
    if (resumeTried) return;
    resumeTried = true;
    SdFileSystem reader;
    writing::Document session;
    writing::LoadJob sessionLoad(reader);
    if (!sessionLoad.begin(session, "/System/writing/session.txt", 16384)) return;
    while (sessionLoad.busy()) sessionLoad.step();
    if (sessionLoad.state() != writing::LoadJob::State::Done) return;
    char* data = session.snapshot();
    if (!data) return;
    // Empty second line is valid (no reference), so strtok must not be used.
    String lines[7];
    const char* p = data;
    for (size_t i = 0; i < 7; ++i) {
      const char* endLine = strchr(p, '\n');
      if (!endLine) break;
      lines[i] = String(p).substring(0, endLine - p);
      p = endLine + 1;
    }
    while (*p) {
      const char* e = strchr(p, '\n');
      if (!e) break;
      String file = String(p).substring(0, e - p);
      if (file.startsWith("/") &&
          std::find(references.begin(), references.end(), file) == references.end())
        references.push_back(file);
      p = e + 1;
    }
    writing::release(data);
    if (lines[0] != "WRITER084" || !lines[2].startsWith("/")) return;
    const long project = lines[1].toInt();
    if (project < 0 || project > 255 || lines[1] != String(project)) return;
    resumeDocumentProject = static_cast<uint8_t>(project);
    if (workspace.find(lines[2].c_str(),resumeDocumentProject) < 0) return;
    resumePaths[0] = lines[2];
    resumePaths[1] = lines[6] != "0" && lines[3].startsWith("/") ? lines[3] : String();
    resumeStart = static_cast<size_t>(lines[4].toInt());
    resumeCursor = static_cast<size_t>(lines[5].toInt());
    dialog("Reprise du document…");
    resumeProgress = label(modal, "", 20, 70, Width - 80);
    if (!resumeJob.begin(document, resumePaths[0].c_str())) {
      closeDialog(); note("Ouverture impossible : SD, UTF-8 ou limite de 2 Mio"); return;
    }
    resumePhase = 1;
  }
  void serviceResume() {
    if (!resumePhase || !visible) return;
    resumeJob.step(); // Yield to the main loop, USB and LVGL after each SD block.
    if (resumeJob.busy()) {
      char progress[40];
      snprintf(progress, sizeof(progress), "%u / 2 · %u%%", resumePhase, resumeJob.progress());
      setLabel(resumeProgress, progress);
      return;
    }
    if (resumeJob.state() != writing::LoadJob::State::Done) {
      const bool ref=resumePhase==2;const String requested=resumePaths[ref?1:0];const auto error=resumeJob.codecError();const int project=resumeDocumentProject;
      closeDialog();if(error==writing::CodecError::InvalidSequence){encodingDialog(requested,ref,project);note("Choisissez l'encodage du fichier");}else note("Ouverture impossible : stockage ou limite de taille");return;
    }
    const bool recovered = resumeJob.recovered();
    if (resumePhase == 1) {
      path = resumePaths[0];syntaxChoice=writing::SyntaxLanguage::Auto;syntaxIndex.reset();
      currentDocumentProject = resumeDocumentProject;
      start = resumeStart <= document.size() && document.boundary(resumeStart) ? resumeStart : 0;
      cursor = resumeCursor <= document.size() && document.boundary(resumeCursor) ? resumeCursor : start;
      headingsDirty = statsDirty = true;
      view.blur(); restorePosition();
      if (resumePaths[1].length()) {
        resumePhase = 2;
        if (resumeJob.begin(reference, resumePaths[1].c_str())) return;
        closeDialog(); note("Ouverture impossible : SD, UTF-8 ou limite de 2 Mio"); return;
      }
    } else {
      referencePath = resumePaths[1]; referenceIndex.reset();referenceSyntaxIndex.reset(); refStart = 0; split = true;
      if (std::find(references.begin(), references.end(), referencePath) == references.end())
        references.push_back(referencePath);
      renderReference();
    }
    resumePhase = 0; resumeProgress = nullptr;
    closeDialog();
    if (recovered) note("Version précédente récupérée (.writer.bak)");
    warnSystem(path);
  }

};
Tab5WritingApp::Impl* Tab5WritingApp::Impl::instance = nullptr;
Tab5WritingApp Tab5Writing;
void Tab5WritingApp::begin(lv_obj_t* parent, WritingHooks hooks) {
  if (impl_) return;
  impl_ = new (std::nothrow) Impl;
  if (!impl_) return;
  Impl::instance = impl_;
  impl_->hooks = hooks;
  impl_->fontSize = impl_->view.fontSize = impl_->refView.fontSize = textSize_;
  impl_->appearance();
  impl_->root = lv_obj_create(parent);
  impl_->panel(impl_->root);
  lv_obj_update_layout(parent);
  impl_->Width = std::max(320, int(lv_obj_get_content_width(parent)));
  lv_obj_set_size(impl_->root, impl_->Width, FullHeight);
  lv_obj_set_pos(impl_->root, 0, 0);
  lv_obj_add_flag(impl_->root, LV_OBJ_FLAG_HIDDEN);
}
lv_obj_t* Tab5WritingApp::navigationScope() const {
  return impl_ ? (impl_->menuPanel ? impl_->menuPanel : impl_->modal ? impl_->modal : impl_->root) : nullptr;
}
void Tab5WritingApp::leaveEditing() {
  if (!impl_) return;
  impl_->closeMenu(false);
  if (impl_->view.editing) {
    impl_->view.blur();
    Tab5Focus.focusObject(impl_->view.object);
  }
  impl_->showKeyboard(false);
  impl_->target = nullptr;
}
lv_obj_t* Tab5WritingApp::object() const { return impl_ ? impl_->root : nullptr; }
const writing::ViewMetrics* Tab5WritingApp::viewMetrics(bool reference) const {
  return impl_ ? &(reference ? impl_->refView.metrics() : impl_->view.metrics()) : nullptr;
}
void Tab5WritingApp::refreshAppearance() {
  if (impl_) impl_->appearance();
}
void Tab5WritingApp::setTextSize(uint16_t pixels) {
  pixels = std::max<uint16_t>(18, std::min<uint16_t>(36, pixels));
  if (textSize_ == pixels) return;
  textSize_ = pixels;
  if (!impl_) return;
  auto& s = *impl_;
  s.fontSize = s.view.fontSize = s.refView.fontSize = pixels;
  // Hidden/reclaimed views must not allocate geometry or open fonts here.
  // Their normal show/resume path will use the new size.
  if (!s.built || !s.visible) return;
  const int32_t mainY = lv_obj_get_scroll_y(s.mainScroll);
  const int32_t refY = lv_obj_get_scroll_y(s.refScroll);
  s.view.reflow(false);
  if (s.split) s.refView.reflow(false);
  lv_obj_scroll_to_y(s.mainScroll, mainY, LV_ANIM_OFF);
  lv_obj_scroll_to_y(s.refScroll, refY, LV_ANIM_OFF);
}
void Tab5WritingApp::show(bool visible) {
  if (!impl_) return;
  impl_->visible = visible;
  if (visible) {
    impl_->build();
    lv_obj_remove_flag(impl_->root, LV_OBJ_FLAG_HIDDEN);
    if (impl_->layoutReclaimed) {
      // No document reload: preserve the selection, cursor and unsaved edits.
      impl_->layout();
      impl_->view.resumeLayout();
      impl_->refView.resumeLayout();
      lv_obj_scroll_to_y(impl_->mainScroll, impl_->reclaimedMainScroll, LV_ANIM_OFF);
      lv_obj_scroll_to_y(impl_->refScroll, impl_->reclaimedRefScroll, LV_ANIM_OFF);
      impl_->layoutReclaimed = false;
      impl_->syncStyle();
      impl_->updateStatus();
    } else {
      impl_->resume();
      impl_->render();
      impl_->renderReference();
    }
  } else {
    if(impl_->browser)impl_->closeDialog();
    impl_->serialStopSession();
    impl_->releaseConsole();
    impl_->workspaceScanner.service(millis(), false);
    impl_->closeMenu(false);
    impl_->showKeyboard(false);
    lv_obj_add_flag(impl_->root, LV_OBJ_FLAG_HIDDEN);
    // The document, undo history, caret and unsaved edits remain owned by the
    // app. Only reloadable drawing geometry is returned when navigating away,
    // before the next app needs to reserve its startup memory.
    reclaimHiddenLayout();
  }
}
bool Tab5WritingApp::open(const char* path) {
  if (!impl_) return false;
  impl_->build();
  if (impl_->resumePhase) impl_->closeDialog();
  impl_->resumeTried = true;
  const bool loaded=impl_->load(path);
  return loaded || impl_->encodingOpenPath.length(); // Accepted opening may await an explicit encoding choice.
}
bool Tab5WritingApp::create(const char* directory) {
  if (!impl_) return false;
  impl_->build();
  if (impl_->resumePhase) impl_->closeDialog();
  impl_->resumeTried = true;
  return impl_->create(directory ? directory : "/");
}
void Tab5WritingApp::resize(int height) {
  if (impl_) {
    if(impl_->root){auto* parent=lv_obj_get_parent(impl_->root);lv_obj_update_layout(parent);impl_->Width=std::max(320,int(lv_obj_get_content_width(parent)));}
    impl_->keyboard = height < FullHeight;
    impl_->height = height;
    impl_->layout();
  }
}
void Tab5WritingApp::service(uint32_t now, bool autosave, uint32_t delayMs) {
  if (!impl_ || !impl_->built) return;
  auto& s = *impl_;
  if(s.visible)s.serviceBrowser();
  if (s.resumePhase) { s.serviceResume(); return; }
  if (s.saveJob.busy()) s.saveJob.step();
  s.observeSave();
  s.serviceSession();
  s.serialService(now);
  s.serviceConsole(now);
  s.serviceSyntax();
  s.workspaceScanner.service(now,s.visible);
  if (s.openMenu == Impl::MenuDocuments && !s.workspaceScanner.busy() && s.documentsMenuCount != s.workspace.count()) {
    s.closeMenu(false); s.toggleMenu(Impl::MenuDocuments);
  }
  if (now - s.lastWorkspaceSave >= 5000 && !s.saveJob.busy() && !s.sessionJob.busy()) {
    s.lastWorkspaceSave = now; s.capturePosition(); s.persistWorkspace();
  }
  if (autosave && s.document.dirty() && !s.saveJob.busy() &&
      (now - s.lastEdit >= delayMs || now - s.lastAttempt >= 30000) &&
      now - s.lastAttempt >= std::max<uint32_t>(delayMs, 5000))
    s.save();
  if (s.visible && s.deferredEdge) {
    const int edge = s.deferredEdge;
    s.deferredEdge = 0;
    s.navigate(edge == 2, 1, true);
  }
  if (s.headingsDirty) s.indexHeadings();
  const bool indexWasReady = s.mainIndex.ready();
  s.mainIndex.step(s.document);
  if (!indexWasReady && s.mainIndex.ready()) s.cachedTitleCursor = SIZE_MAX;
  const bool referenceWasReady = s.referenceIndex.ready();
  s.referenceIndex.step(s.reference);
  if (!referenceWasReady && s.referenceIndex.ready() && s.split) {
    // Index publication changes the heading path, not the displayed text.
    // Update this label once without reloading or reflowing either pane.
    s.setLabel(s.referenceName,
               s.breadcrumb(s.referencePath, s.refHeadings, s.refStart, 254).c_str());
  }
  if (s.visible && s.outlineAfterIndex && (s.outlineAfterIndex == 2 ? s.referenceIndex : s.mainIndex).ready()) {
    const bool ref = s.outlineAfterIndex == 2;
    s.outlineAfterIndex = 0; s.outline(ref);
  }
  if (s.visible && s.end - s.start > 32768 && s.view.anchor == s.view.caret && now - s.lastEdit >= 700) {
    s.cursor = s.start + s.view.text.sourceAt(s.view.caret);
    s.start = s.cursor > 8192 ? s.document.lineStart(s.cursor - 8192) : 0;
    if (s.cursor - s.start > 32768) s.start = s.cursor - 4096;
    while (s.start && !s.document.boundary(s.start)) --s.start;
    s.render();
  }
  if (s.statsDirty && s.mainIndex.ready()) {
    s.lastWords = s.mainIndex.words();
    s.lastCharacters = s.mainIndex.characters();
    s.statsDirty = false;
  }
  if (s.sessionDirty && now - s.lastEdit >= 1500 && now - s.lastSession > s.sessionRetryDelay &&
      !s.saveJob.busy() && !s.sessionJob.busy()) {
    s.lastSession = now;
    s.saveSession();
  }
  if (s.visible && now - s.lastStatus > 300) {
    s.lastStatus = now;
    s.updateStatus();
  }
}
bool Tab5WritingApp::key(const writing::KeyEvent& event, lv_obj_t* neighbor) {
  return impl_ && impl_->key(event, neighbor);
}
bool Tab5WritingApp::contextMenu(lv_obj_t* hit,int x,int y) {
  if(!impl_||!impl_->visible||impl_->modal||!hit)return false;
  auto& s=*impl_;
  const bool reference=s.refView.object&&Tab5Focus.contains(s.refView.object,hit);
  if(!reference&&(!s.view.object||!Tab5Focus.contains(s.view.object,hit)))return false;
  Tab5ContextMenu::Item items[10];size_t n=0;
  auto add=[&](const char* title,const char* keys,int action,bool enabled=true){
    items[n++]={title,keys,[](void* value){
      auto* self=Impl::instance;if(!self||!self->visible)return;
      const int command=int(reinterpret_cast<uintptr_t>(value));
      if(command==1000){auto text=self->refView.selected();Tab5TextInput::copy(text.data(),text.size());}
      else if(command==1001){self->refView.anchor=0;self->refView.caret=self->refView.text.size();lv_obj_invalidate(self->refView.object);}
      else self->act(command);
    },reinterpret_cast<void*>(uintptr_t(action)),enabled};
  };
  if(reference){
    add("Copier","Ctrl+C",1000,s.refView.caret!=s.refView.anchor);
    add("Tout sélectionner","Ctrl+A",1001,s.refView.text.size()!=0);
  }else{
    const bool selection=s.documentSelection||s.view.caret!=s.view.anchor;
    add("Annuler la modification","Ctrl+Z",Impl::Undo,s.document.canUndo());
    add("Rétablir","Ctrl+Y",Impl::Redo,s.document.canRedo());
    add("Couper","Ctrl+X",Impl::Cut,selection);
    add("Copier","Ctrl+C",Impl::Copy,selection);
    add("Coller","Ctrl+V",Impl::Paste,*Tab5TextInput::clipboard());
    add("Tout sélectionner","Ctrl+A",Impl::SelectAll,s.document.size()!=0);
    add("Rechercher…","Ctrl+F",Impl::Search);
    add("Enregistrer","Ctrl+S",Impl::Save);
  }
  return Tab5ContextMenu::show(reference?s.refView.object:s.view.object,x,y,items,n);
}
bool Tab5WritingApp::isInput(lv_obj_t* target) const {
  return impl_ && impl_->visible && target == impl_->proxy;
}
void Tab5WritingApp::insert(const char* text) {
  if (impl_) {if(impl_->documentSelection)impl_->replaceSelection(text);else impl_->activeView().insert(text);}
}
void Tab5WritingApp::touchKey(writing::Key key) {
  if (impl_) {
    writing::KeyEvent e;
    e.key = key;
    if (impl_->documentSelection){impl_->key(e,nullptr);return;}
    if (impl_->tableGrid && key == writing::Key::Enter)
      impl_->activeView().blur();
    else
      impl_->activeView().key(e);
  }
}
bool Tab5WritingApp::editing() const { return impl_ && impl_->visible && impl_->activeView().editing; }

bool Tab5WritingApp::storageIdle() const {
 return !impl_ || ((!impl_->browser||!impl_->browser->scanning()) && !impl_->fs.opened() && !impl_->resumeFs.opened() && !impl_->sessionFs.opened() && !impl_->workspaceFs.opened() && impl_->workspaceScanner.storageIdle());
}

bool Tab5WritingApp::prepareSleep() {
 if(!impl_)return true;
 auto& s=*impl_;
 if(s.document.dirty()||s.saveJob.busy()||s.sessionJob.busy()||s.resumePhase)return false;
 if(!s.built)return storageIdle();
 s.capturePosition();if(!s.persistWorkspace())return false;
 if(s.path.length())s.saveSession();
 return !s.sessionJob.busy()&&storageIdle();
}

size_t Tab5WritingApp::reclaimHiddenLayout() {
  if (!impl_ || !impl_->built || impl_->visible || impl_->layoutReclaimed ||
      impl_->modal || impl_->tableGrid || impl_->resumePhase) return 0;
  auto& s = *impl_;
#if defined(ARDUINO_ARCH_ESP32)
  const auto before = tab5MemoryTotals();
#endif
  s.reclaimedMainScroll = lv_obj_get_scroll_y(s.mainScroll);
  s.reclaimedRefScroll = lv_obj_get_scroll_y(s.refScroll);
  s.view.discardLayout();
  s.refView.discardLayout();
  s.layoutReclaimed = true;
#if defined(ARDUINO_ARCH_ESP32)
  const auto after = tab5MemoryTotals();
  return (after.internal > before.internal ? after.internal - before.internal : 0) +
         (after.psram > before.psram ? after.psram - before.psram : 0);
#else
  return 0; // Native tests have no ESP heap-capability counters.
#endif
}

bool Tab5WritingApp::createCode(const char* directory) {
  if (!impl_) return false;
  impl_->build();
  if (impl_->resumePhase) impl_->closeDialog();
  impl_->resumeTried = true;
  return impl_->create(directory ? directory : "/MicroPython", true);
}
bool Tab5WritingApp::codeFile(const char* path) { return writing::sourceFile(path); }
const char* Tab5WritingApp::path() const { return impl_ ? impl_->path.c_str() : ""; }
bool Tab5WritingApp::development() const { return impl_ && impl_->isCode(); }
