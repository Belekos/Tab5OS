enum class HomeAction : uint8_t {
  Home,
  Files,
  Editor,
  Reader,
  Audio,
  Map,
  Settings,
  Calculator,
  Python, Theme, NativeApps, Terminal, Alarm, Sdr = 15, Storage = 16, Pentest = 17, ImageEditor = 18, Meshtastic = 19
};
struct HomeCardDef {
  HomeAction action;
  const char* title;
  const char* symbol;
};
static const HomeCardDef HOME_CARDS[] = {
  {HomeAction::Files,      "Explorateur", LV_SYMBOL_DIRECTORY},
  {HomeAction::Editor,     "Éditeur", LV_SYMBOL_EDIT},
  {HomeAction::Reader,     "eBook", LV_SYMBOL_EYE_OPEN},
  {HomeAction::Audio,      "Média", LV_SYMBOL_AUDIO},
  {HomeAction::Map,        "Cartes", LV_SYMBOL_GPS},
  {HomeAction::Calculator, "Calculatrice", TAB5_SYMBOL_CALCULATOR},
  {HomeAction::Settings,   "Réglages", LV_SYMBOL_SETTINGS},
  {HomeAction::Terminal, "Terminal", LV_SYMBOL_KEYBOARD},
  {HomeAction::Alarm, "Réveil", LV_SYMBOL_AUDIO},
  {HomeAction::Sdr, "RTL-SDR", LV_SYMBOL_AUDIO},
  {HomeAction::ImageEditor, "Dessin", LV_SYMBOL_IMAGE},
  {HomeAction::Meshtastic, "Meshtastic", LV_SYMBOL_WIFI},
};
static const char* HOME_IDS[]={"files","editor","reader","audio","map","calculator","settings","terminal","alarm","sdr","image-editor","meshtastic"};
static const uint8_t HOME_SYMBOLS[]={1,2,10,3,4,11,5,12,3,3,7,15};