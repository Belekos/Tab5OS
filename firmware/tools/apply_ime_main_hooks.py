"""Apply the owned IME/font hooks to the current main sketch, exactly once."""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[1]
p=root/'Tab5OS_Arduino_v0.8.18_Ecriture/Tab5OS_Arduino_v0.8.18_Ecriture.ino'
s=p.read_text(encoding='utf8')
if '#include "Tab5ImeUi.inc"' in s:raise SystemExit('IME hooks already present; inspect instead of reapplying.')
def replace(old,new,n=1):
    global s
    assert s.count(old)==n,(old,s.count(old),n)
    s=s.replace(old,new)
replace('#include "Tab5KeyboardLayouts.h"','#include "Tab5KeyboardLayouts.h"\n#include "Tab5Ime.h"\n#include "Tab5ImeDictionary.h"\n#include "Tab5CjkFonts.h"')
needle='static void accentChoiceEvent(lv_event_t* e) {'
replace(needle,'#include "Tab5ImeUi.inc"\n\n'+needle)
needle='  if (!k || !keyboardTarget || !lv_obj_is_valid(keyboardTarget)) return;'
replace(needle,needle+'\n  if (imeTouch(k)) return;')
needle='  lv_obj_clean(keyboardPanel);\n  auto* bar = lv_obj_create(keyboardPanel);'
replace(needle,'  lv_obj_clean(keyboardPanel);\n  imeSync(keyboardTarget);\n  if(imeComposer.mode()!=tab5ime::Mode::None)imeMount(keyboardPanel);\n  auto* bar = lv_obj_create(keyboardPanel);')
needle='  using K=writing::Key;writing::KeyEvent e=original;'
replace(needle,needle+'''
  auto* imeScope=keyboardScope();
  auto* imeTarget=Tab5Focus.focused();
  if(!imeTarget||!lv_obj_check_type(imeTarget,&lv_textarea_class))
    imeTarget=(!keyboardUiNavigation&&keyboardObjectVisible(keyboardTarget))?keyboardTarget:nullptr;
  if(imeTarget&&!Tab5Focus.contains(imeScope,imeTarget))imeTarget=nullptr;
  if(imePhysical(e,imeTarget))return;
''')
needle='  if (!sdReady || !tab5FS.exists(FONT_FILE)) return false;'
replace(needle,needle+'\n  Tab5Cjk.select(Tab5Locale.selectedId(),Tab5Keyboards.selectedId());')
for name,pixels in [('uiTtfFont','20'),('docTtfFont','fontSizeForIndex(settings.fontIndex)'),('keyTtfFont','34'),('readerTtfFont','fontSizeForIndex(settings.fontIndex) + 2')]:
    fallback='&lv_font_montserrat_32' if name=='keyTtfFont' else '&lv_font_montserrat_20'
    needle=f'{name}->fallback = {fallback};'
    replace(needle,f'Tab5Cjk.attach({name}, {pixels});')
replace('  f->fallback = &lv_font_montserrat_20;','  Tab5Cjk.attach(f, px);')
# Keep fallback size synchronized with every primary font resize. Balanced
# parsing preserves nested fontSizeForIndex(...) calls in compact lambdas.
starts=[m.start() for m in re.finditer(r'lv_tiny_ttf_set_size\(',s)]
for start in reversed(starts):
    at=start+len('lv_tiny_ttf_set_size(');depth=1;end=at
    while depth:
        if s[end]=='(':depth+=1
        elif s[end]==')':depth-=1
        end+=1
    args=s[at:end-1];assert s[end]==';';font,pixels=args.split(',',1)
    # Add braces if the original call is the only body of a single-line if.
    statement=s[start:end+1];replacement='{ '+statement+' Tab5Cjk.attach('+font.strip()+','+pixels.strip()+'); }'
    before=s[max(0,start-100):start]
    if before.rstrip().endswith(')'):
        s=s[:start]+replacement+s[end+1:]
    else:s=s[:end+1]+' Tab5Cjk.attach('+font.strip()+','+pixels.strip()+');'+s[end+1:]
replace('Tab5Locale.applyFontAvailability(smoothFontReady);','Tab5Locale.applyFontAvailability(smoothFontReady && Tab5Cjk.availableFor(Tab5Locale.selectedId()));')
# Language changes retain a manual keyboard override; auto mode applies the
# corresponding pack and immediately refreshes physical and touch mappings.
needle='  Tab5Writing.refreshAppearance();\n  toast("Langue enregistrée — redémarrez pour tout appliquer", 3000);'
replace(needle,'''  if(keyboardPanel)lv_obj_clean(keyboardPanel);
  Tab5Keyboards.selectForLanguage(Tab5Locale.selectedId());
  Tab5Cjk.select(Tab5Locale.selectedId(),Tab5Keyboards.selectedId());
  Tab5Locale.applyFontAvailability(smoothFontReady && Tab5Cjk.availableFor(Tab5Locale.selectedId()));
  imeSync(keyboardTarget);rebuildKeyboard();
  writing::usbFrench(Tab5Keyboards.selectedId().startsWith("fr"));
  writing::usbMapping(Tab5Keyboards.physical().data(),Tab5Keyboards.physical().size());
  if(keyboardLayoutDropdown)lv_dropdown_set_selected(keyboardLayoutDropdown,Tab5Keyboards.selectedIndex());
  Tab5Writing.refreshAppearance();
  toast("Langue enregistrée — redémarrez pour tout appliquer", 3000);''')
needle='  keyboardMode = KeyboardMode::Lower;\n  rebuildKeyboard();\n  writing::usbFrench(Tab5Keyboards.selectedId().startsWith("fr"));'
replace(needle,'''  keyboardMode = KeyboardMode::Lower;
  Tab5Cjk.select(Tab5Locale.selectedId(),Tab5Keyboards.selectedId());
  imeSync(keyboardTarget);
  rebuildKeyboard();
  writing::usbFrench(Tab5Keyboards.selectedId().startsWith("fr"));''')
# These handlers precede the .inc implementation in the original sketch.
needle='static void setKeyboardVisible(bool visible, lv_obj_t* target = nullptr);'
replace(needle,needle+'\nstatic void imeSync(lv_obj_t* input);\nstatic void imeCancel();')
p.write_text(s,encoding='utf8')
print('Applied IME, scope-safe keyboard dispatch and CJK font fallback hooks.')
