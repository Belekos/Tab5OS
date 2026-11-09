#include "Tab5ThemePreview.h"
#include "Tab5ThemeSchema.h"
#include "Tab5UiStyle.h"
#include "Tab5ThemeLive.h"
#ifndef TAB5_THEME_TEST
#include "Tab5Localization.h"
#endif
#include <cstring>
#include <algorithm>
#include <cstdio>
namespace Tab5ThemePreview {
namespace {
struct Demo { uint32_t colors[Count]{}; unsigned selected=0, count=9; lv_obj_t* switches[2]{}; };
const char* tr(const char* text) {
#ifdef TAB5_THEME_TEST
 return text;
#else
 return Tab5Locale.text(text);
#endif
}
struct Zone {int x,y,w,h; Role role;};
const Zone zones[]={{881,106,28,28,SwitchKnobOn},{851,151,28,28,SwitchKnobOff},
 {8,42,176,42,Raised},{192,42,176,42,Selected},{376,42,176,42,Selected},{560,42,176,42,Raised},
 {756,42,295,24,Text},{756,67,295,25,Muted},{8,96,250,152,Surface},{272,99,330,48,Cursor},
 {272,160,330,44,Border},{618,102,190,32,Selected},{618,145,190,32,Surface},{846,103,210,34,SwitchOn},{846,148,210,34,SwitchOff},
 {883,190,28,30,Text},{630,198,253,14,Active},{911,198,127,14,Raised},{272,228,198,13,Active},{470,228,132,13,Raised}};
const char* names[]={"Fond","Surface","Relief","Bordure","Texte","Texte secondaire","Actif","Curseur","Sélection","Pointeur","Contour du pointeur","Interrupteur : fond inactif","Interrupteur : fond actif","Interrupteur : bouton inactif","Interrupteur : bouton actif","Icônes des fichiers","Icônes des dossiers","Icônes des fichiers masqués","Icônes des dossiers masqués"};
static_assert(sizeof(names)/sizeof(names[0])==Count,"Every editable theme role needs a label");
static_assert(Count==tab5theme::PaletteCount,"Preview and saved palette roles must match");
void render(lv_event_t* e,Demo& d) {
 auto* obj=lv_event_get_current_target_obj(e); auto* layer=lv_event_get_layer(e);
 lv_area_t bounds;lv_obj_get_coords(obj,&bounds);const auto* font=lv_obj_get_style_text_font(obj,LV_PART_MAIN);
 auto rect=[&](int x,int y,int w,int h,Role bg,int radius=4,lv_opa_t opacity=LV_OPA_COVER){lv_draw_rect_dsc_t r;lv_draw_rect_dsc_init(&r);r.bg_color=lv_color_hex(d.colors[bg]);r.bg_opa=opacity;r.radius=radius;r.border_width=1;r.border_opa=opacity;r.border_color=lv_color_hex(d.colors[Border]);lv_area_t a={bounds.x1+x,bounds.y1+y,bounds.x1+x+w-1,bounds.y1+y+h-1};lv_draw_rect(layer,&r,&a);};
 auto text=[&](const char* value,int x,int y,int w,Role fg=Text,lv_opa_t opacity=LV_OPA_COVER){char shortText[256];snprintf(shortText,sizeof(shortText),"%s",tr(value));lv_point_t size;lv_text_get_size(&size,shortText,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);while(size.x>w&&shortText[0]){size_t n=strlen(shortText)-1;while(n&&(uint8_t(shortText[n])&0xc0)==0x80)--n;shortText[n]=0;lv_text_get_size(&size,shortText,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);}lv_draw_label_dsc_t l;lv_draw_label_dsc_init(&l);l.text=shortText;l.font=font;l.color=lv_color_hex(d.colors[fg]);l.opa=opacity;l.text_local=1;lv_area_t a={bounds.x1+x,bounds.y1+y,bounds.x1+x+w-1,bounds.y1+y+int(font->line_height)-1};lv_draw_label(layer,&l,&a);};
 rect(0,0,lv_obj_get_width(obj),lv_obj_get_height(obj),Bg,6);
 for(unsigned i=0;i<d.count;++i){rect(8+i*(1053/d.count),6,1053/d.count-4,29,Role(i));text(names[i],12+i*(1053/d.count),8,1053/d.count-12,i==Text||i==Muted||i==Cursor?Bg:Text);}
 const char* buttons[]={"Bouton normal","Bouton actif","Bouton sélectionné","Bouton désactivé"};
 for(unsigned i=0;i<4;++i){const lv_opa_t opacity=i==3?LV_OPA_50:LV_OPA_COVER;rect(8+i*184,42,176,42,i==0||i==3?Raised:Selected,4,opacity);text(buttons[i],14+i*184,51,164,Text,opacity);}
 text("Aperçu interactif",756,42,295);text("Couleurs du thème",756,67,295,Muted);
 rect(8,96,250,152,Surface);text("Liste",18,104,230,Muted);
 if(d.count>HiddenFolderIcon){
  const char* entries[]={"Fichier","Dossier","Fichier masqué","Dossier masqué"};
  rect(14,178,238,28,Selected);
  for(unsigned i=0;i<4;++i){
   const int y=130+int(i)*26;lv_draw_label_dsc_t glyph;lv_draw_label_dsc_init(&glyph);
   glyph.text=i%2?LV_SYMBOL_DIRECTORY:LV_SYMBOL_FILE;glyph.font=&lv_font_montserrat_24;
   glyph.color=lv_color_hex(d.colors[FileIcon+i]);
   lv_area_t area={bounds.x1+18,bounds.y1+y,bounds.x1+45,bounds.y1+y+25};lv_draw_label(layer,&glyph,&area);
   text(entries[i],50,y,198,i>=2?Muted:Text);
  }
 }else{text("Document",18,146,230);rect(14,186,238,44,Selected);text("Sélection",20,196,226);}
 rect(272,99,330,48,Surface);rect(350,105,94,34,Selected);text("Texte et curseur",280,111,312);rect(444,105,2,34,Cursor,0);
 rect(272,160,330,44,Surface);text("Premier choix",280,171,270);text(LV_SYMBOL_DOWN,571,171,24);
 rect(618,106,26,26,Selected);text(LV_SYMBOL_OK,620,106,24);text("Coché",654,106,156);
 rect(618,150,26,26,Surface);text("Case libre",654,150,156);
  text("Interrupteur actif",922,108,150);
  text("Interrupteur inactif",922,153,150);
 rect(630,198,408,14,Raised,7);rect(630,198,265,14,Active,7);rect(883,190,28,30,Text,15);
 rect(272,228,330,13,Raised,6);rect(272,228,198,13,Active,6);
 if(d.count>9){
  const lv_point_t p[]={{0,0},{0,38},{10,28},{18,42},{24,38},{16,24},{30,24},{0,0}};
  for(int pass=0;pass<2;++pass)for(unsigned i=1;i<8;++i){lv_draw_line_dsc_t l;lv_draw_line_dsc_init(&l);l.color=lv_color_hex(d.colors[pass?Pointer:PointerOutline]);l.width=pass?3:7;l.p1={lv_value_precise_t(bounds.x1+1070+p[i-1].x),lv_value_precise_t(bounds.y1+114+p[i-1].y)};l.p2={lv_value_precise_t(bounds.x1+1070+p[i].x),lv_value_precise_t(bounds.y1+114+p[i].y)};lv_draw_line(layer,&l);}
 }
 if(lv_obj_has_state(obj,LV_STATE_FOCUS_KEY)){lv_draw_rect_dsc_t r;lv_draw_rect_dsc_init(&r);r.bg_opa=LV_OPA_TRANSP;r.border_width=2;r.border_color=lv_color_hex(d.colors[Text]);const int columnWidth=1053/int(d.count);const int selectionOffset=int(d.selected)*columnWidth;lv_area_t a={bounds.x1+8+selectionOffset,bounds.y1+6,bounds.x1+columnWidth+3+selectionOffset,bounds.y1+34};lv_draw_rect(layer,&r,&a);}
}
void event(lv_event_t* e) {
 auto* obj=lv_event_get_current_target_obj(e);if(lv_event_get_target_obj(e)!=obj)return;
 auto* d=static_cast<Demo*>(lv_event_get_user_data(e));auto code=lv_event_get_code(e);
 if(code==LV_EVENT_DELETE){delete d;return;}
 if(code==LV_EVENT_DRAW_MAIN){render(e,*d);return;}
 unsigned role=d->selected;
 if(code==LV_EVENT_CLICKED){auto* input=lv_event_get_indev(e);if(input){lv_point_t point;lv_indev_get_point(input,&point);lv_area_t bounds;lv_obj_get_coords(obj,&bounds);int x=point.x-bounds.x1,y=point.y-bounds.y1;role=Bg;
  if(y>=6&&y<35&&x>=8&&x<1061)role=std::min(int(d->count)-1,(x-8)/(1053/int(d->count)));
  else if(d->count>HiddenFolderIcon&&x>=14&&x<258&&y>=130&&y<234)role=FileIcon+std::min(3,(y-130)/26);
  else if(d->count>9&&x>=1060&&y>=102&&y<178)role=Pointer;
  else for(const auto& z:zones)if(x>=z.x&&x<z.x+z.w&&y>=z.y&&y<z.y+z.h){role=z.role;break;}
 }} else if(code==LV_EVENT_KEY){auto key=lv_event_get_key(e);if(key==LV_KEY_LEFT||key==LV_KEY_UP)role=(role+d->count-1)%d->count;else if(key==LV_KEY_RIGHT||key==LV_KEY_DOWN)role=(role+1)%d->count;else if(key!=LV_KEY_ENTER)return;}
 else return;
 if(role==SwitchOn&&d->count<=SwitchOn)role=Active;
 if(role==SwitchOff&&d->count<=SwitchOff)role=Raised;
 if(role==SwitchKnobOn&&d->count<=SwitchKnobOn)role=Surface;
 if(role==SwitchKnobOff&&d->count<=SwitchKnobOff)role=Text;
 d->selected=role;lv_obj_invalidate(obj);lv_obj_send_event(obj,LV_EVENT_VALUE_CHANGED,&d->selected);
}
}
const char* roleName(unsigned role){return role<Count?names[role]:"";}
lv_obj_t* create(lv_obj_t* parent) {
 auto* obj=lv_obj_create(parent);Tab5ThemeLive::preserve(obj);lv_obj_remove_style_all(obj);lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_add_flag(obj,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_USER_1|LV_OBJ_FLAG_USER_2));
 auto* d=new Demo;lv_obj_set_user_data(obj,d);lv_obj_add_event_cb(obj,event,LV_EVENT_ALL,d);
 for(unsigned i=0;i<2;++i){auto* sw=d->switches[i]=lv_switch_create(obj);lv_obj_set_pos(sw,846,i?103:148);lv_obj_set_size(sw,68,34);
  lv_obj_remove_flag(sw,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_CLICK_FOCUSABLE));
  lv_obj_set_style_transition(sw,nullptr,LV_STATE_ANY);lv_obj_set_style_anim_duration(sw,0,0);
  lv_obj_set_style_pad_all(sw,3,0);if(i)lv_obj_add_state(sw,LV_STATE_CHECKED);
 }
 return obj;
}
void update(lv_obj_t* obj,const uint32_t* palette,unsigned count) {
 if(!obj||!palette||count<9)return;auto* d=static_cast<Demo*>(lv_obj_get_user_data(obj));if(!d)return;count=std::min(count,unsigned(Count));
 d->count=count;d->selected=std::min(d->selected,count-1);memcpy(d->colors,palette,count*sizeof(uint32_t));
 for(unsigned i=FileIcon;i<count;++i)d->colors[i]=tab5theme::visibleColor(palette,i);
 for(auto* sw:d->switches){Tab5UiStyle::switchColors(sw,palette[count>SwitchOff?SwitchOff:Raised],palette[count>SwitchOn?SwitchOn:Active],palette[count>SwitchKnobOff?SwitchKnobOff:Text],palette[count>SwitchKnobOn?SwitchKnobOn:Surface]);
  lv_obj_set_style_border_color(sw,lv_color_hex(palette[Border]),0);lv_obj_set_style_border_width(sw,1,0);
  for(auto part:{LV_PART_MAIN,LV_PART_INDICATOR,LV_PART_KNOB})lv_obj_set_style_radius(sw,Tab5UiStyle::radius(),part);
 }
 lv_obj_invalidate(obj);
}
}
