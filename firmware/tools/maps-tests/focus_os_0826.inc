// Hardware/layout boundaries for the unchanged OS keyboard routing function.
namespace Tab5NumericKeyboard{static unsigned mode(lv_obj_t*){return 0;}}
enum class Page{Writing,Editor};static Page activePage=Page::Editor;
static lv_obj_t *keyboardPanel=nullptr,*keyboardTarget=nullptr,*editor=nullptr,*accentPopup=nullptr;
static bool keyboardVisible=false,keyboardUiNavigation=false,keyboardDockFocus=false,smoothFontReady=false;
static unsigned keyboardNumberMode=0;static void* keyTtfFont=nullptr;
static constexpr int PAGE_H=660,SCREEN_H=720;
static int keyboardHeight(){return 288;}
static void rebuildKeyboard(){}static bool ensureKeyFont(){return false;}
struct LayoutBoundary{void resize(int){}}Tab5Writing,Tab5Tools;
static void setKeyboardVisible(bool,lv_obj_t* = nullptr);
/* KEYBOARD FUNCTION */
