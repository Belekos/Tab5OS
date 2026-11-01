#include "Tab5TextInput.h"
#include "WriterDocument.h"
#include "Tab5Focus.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <new>
namespace Tab5TextInput {
namespace {
char* copied=nullptr;bool rich=false,secretCopy=false;uint32_t secretCopiedAt=0;
void wipeText(void* data,size_t length){volatile unsigned char* p=static_cast<volatile unsigned char*>(data);while(length--)*p++=0;}
void protectedEvent(lv_event_t*){}
lv_obj_t* owner=nullptr;bool overwrite=false;
constexpr size_t HistoryFieldLimit=32768,HistoryTotalLimit=131072,HistoryTextLimit=16384;
size_t historyBytes=0;
struct Snapshot {char* text=nullptr;size_t bytes=0;int32_t caret=0;};
struct History {std::vector<Snapshot> states;size_t position=0,bytes=0;uint32_t lastAt=0;int direction=0;bool restoring=false;};
void releaseSnapshot(History& h,Snapshot& s){if(s.text)wipeText(s.text,s.bytes);writing::release(s.text);h.bytes-=s.bytes;historyBytes-=s.bytes;s={};}
void resetHistory(History& h){for(auto& s:h.states)releaseSnapshot(h,s);h.states.clear();h.position=0;h.direction=0;}
void historyEvent(lv_event_t* e);
History* history(lv_obj_t* o){
 if(!o||!lv_obj_is_valid(o)||!lv_obj_check_type(o,&lv_textarea_class))return nullptr;
 for(uint32_t i=0;i<lv_obj_get_event_count(o);++i){auto* d=lv_obj_get_event_dsc(o,i);if(lv_event_dsc_get_cb(d)==historyEvent)return static_cast<History*>(lv_event_dsc_get_user_data(d));}return nullptr;
}
void checkpoint(lv_obj_t* o,History& h){
 if(h.restoring)return;if(readOnly(o)||(lv_textarea_get_password_mode(o)||protectedField(o))){resetHistory(h);return;}
 const char* text=lv_textarea_get_text(o);size_t bytes=strlen(text)+1;
 if(bytes>HistoryTextLimit){resetHistory(h);return;}
 if(!h.states.empty()&&!strcmp(h.states[h.position].text,text))return;
 while(h.states.size()>h.position+1){releaseSnapshot(h,h.states.back());h.states.pop_back();}
 int delta=h.states.empty()?0:int(bytes)-int(h.states.back().bytes),direction=delta>0?1:delta<0?-1:0;
 bool group=h.states.size()>1&&direction&&direction==h.direction&&std::abs(delta)<=4&&lv_tick_elaps(h.lastAt)<600;
 auto* copy=static_cast<char*>(writing::allocate(bytes));if(!copy)return;memcpy(copy,text,bytes);
 if(group){releaseSnapshot(h,h.states.back());h.states.pop_back();}
 while(!h.states.empty()&&(h.states.size()>=32||h.bytes+bytes>HistoryFieldLimit||historyBytes+bytes>HistoryTotalLimit)){releaseSnapshot(h,h.states.front());h.states.erase(h.states.begin());}
 if(historyBytes+bytes>HistoryTotalLimit){writing::release(copy);h.position=h.states.empty()?0:h.states.size()-1;return;}
 h.states.push_back({copy,bytes,int32_t(lv_textarea_get_cursor_pos(o))});h.bytes+=bytes;historyBytes+=bytes;h.position=h.states.size()-1;h.lastAt=lv_tick_get();h.direction=direction;
}
void historyEvent(lv_event_t* e){auto* h=static_cast<History*>(lv_event_get_user_data(e));auto* o=lv_event_get_current_target_obj(e);if(lv_event_get_target_obj(e)!=o)return;
 if(lv_event_get_code(e)==LV_EVENT_DELETE){resetHistory(*h);delete h;return;}if(lv_event_get_code(e)==LV_EVENT_VALUE_CHANGED)checkpoint(o,*h);
}
bool restore(lv_obj_t* o,bool forward){auto* h=history(o);if(!h||readOnly(o)||(lv_textarea_get_password_mode(o)||protectedField(o))||h->states.empty()||(forward?h->position+1>=h->states.size():!h->position))return false;
 h->position+=forward?1:size_t(-1);const auto& s=h->states[h->position];h->restoring=true;lv_textarea_set_text(o,s.text);lv_textarea_set_cursor_pos(o,s.caret);h->restoring=false;h->direction=0;auto* label=lv_textarea_get_label(o);lv_label_set_text_selection_start(label,LV_DRAW_LABEL_NO_TXT_SEL);lv_label_set_text_selection_end(label,LV_DRAW_LABEL_NO_TXT_SEL);return true;
}
void deleted(lv_event_t* e){if(owner==lv_event_get_target_obj(e))owner=nullptr;}
void observe(lv_obj_t* field){track(field);if(owner==field)return;owner=field;overwrite=false;
 bool watched=false;for(uint32_t i=0;i<lv_obj_get_event_count(field);++i)if(lv_event_dsc_get_cb(lv_obj_get_event_dsc(field,i))==deleted)watched=true;
 if(!watched)lv_obj_add_event_cb(field,deleted,LV_EVENT_DELETE,nullptr);
 lv_textarea_set_text_selection(field,true);
}
void readOnlyEvent(lv_event_t* e){if(lv_event_get_code(e)==LV_EVENT_INSERT)lv_textarea_set_insert_replace(lv_event_get_target_obj(e),"");}
struct LabelSelection {size_t anchor=0,caret=0;};
void labelEvent(lv_event_t* e){auto* o=lv_event_get_current_target_obj(e);if(lv_event_get_target_obj(e)!=o)return;auto* state=static_cast<LabelSelection*>(lv_event_get_user_data(e));auto code=lv_event_get_code(e);
 if(code==LV_EVENT_DELETE){delete state;return;}
 if(code!=LV_EVENT_PRESSED&&code!=LV_EVENT_PRESSING)return;auto* indev=lv_event_get_indev(e);if(!indev)return;
 lv_point_t point;lv_indev_get_point(indev,&point);lv_area_t a;lv_obj_get_coords(o,&a);point.x-=a.x1;point.y-=a.y1;size_t p=lv_label_get_letter_on(o,&point,false);
 if(code==LV_EVENT_PRESSED){Tab5Focus.focusObject(o);state->anchor=p;}state->caret=p;
 lv_label_set_text_selection_start(o,std::min(state->anchor,p));lv_label_set_text_selection_end(o,std::max(state->anchor,p));
}
LabelSelection* labelState(lv_obj_t* o){for(uint32_t i=0;i<lv_obj_get_event_count(o);++i){auto* e=lv_obj_get_event_dsc(o,i);if(lv_event_dsc_get_cb(e)==labelEvent)return static_cast<LabelSelection*>(lv_event_dsc_get_user_data(e));}return nullptr;}
bool labelKey(lv_obj_t* o,const writing::KeyEvent& e){auto* state=labelState(o);if(!state)return false;using K=writing::Key;const char* text=lv_label_get_text(o);size_t length=writing::charCount(text,strlen(text));state->caret=std::min(state->caret,length);state->anchor=std::min(state->anchor,length);bool command=e.ctrl||e.meta;
 if(command&&e.key==K::Text&&(e.text[0]=='a'||e.text[0]=='A')){state->anchor=0;state->caret=length;}
 else if((command&&e.key==K::Text&&(e.text[0]=='c'||e.text[0]=='C'||e.text[0]=='x'||e.text[0]=='X'))||(command&&e.key==K::Insert)){copySelection(o);return true;}
 else {size_t dest=state->caret;
  if(e.key==K::Home)dest=0;else if(e.key==K::End)dest=length;
  else if(e.key==K::Left)dest=dest?dest-1:0;else if(e.key==K::Right)dest=std::min(length,dest+1);
  else if(e.key==K::Up||e.key==K::Down||e.key==K::PageUp||e.key==K::PageDown){lv_point_t p;lv_label_get_letter_pos(o,dest,&p);int line=lv_obj_get_style_text_font(o,LV_PART_MAIN)->line_height+lv_obj_get_style_text_line_space(o,LV_PART_MAIN);int step=e.key==K::PageUp||e.key==K::PageDown?std::max(line,int(lv_obj_get_height(lv_obj_get_parent(o)))-line):line;p.y=std::max(0,int(p.y)+(e.key==K::Up||e.key==K::PageUp?-step:step));dest=lv_label_get_letter_on(o,&p,false);}
  else return false;
  state->caret=dest;if(!e.shift)state->anchor=dest;
 }
 lv_label_set_text_selection_start(o,std::min(state->anchor,state->caret));lv_label_set_text_selection_end(o,std::max(state->anchor,state->caret));return true;
}
void selection(lv_obj_t* o,size_t a,size_t b){auto* l=lv_textarea_get_label(o);lv_label_set_text_selection_start(l,a==b?LV_DRAW_LABEL_NO_TXT_SEL:std::min(a,b));lv_label_set_text_selection_end(l,a==b?LV_DRAW_LABEL_NO_TXT_SEL:std::max(a,b));}
bool replace(lv_obj_t* o,size_t a,size_t b,const char* add){
 const char* old=lv_textarea_get_text(o);size_t length=strlen(old),from=writing::byteOffset(old,a),to=writing::byteOffset(old,b);
 if(from>to||to>length)return false;
 size_t n=strlen(add);if(n>writing::Document::Limit||length-(to-from)>writing::Document::Limit-n)return false;
 auto* next=static_cast<char*>(writing::allocate(length-(to-from)+1));if(!next)return false;
 memcpy(next,old,from);memcpy(next+from,old+to,length-to+1);
 auto* editHistory=history(o);if(editHistory){editHistory->restoring=true;if(a!=b&&n)editHistory->direction=0;}
 // Let LVGL enforce accepted characters and max_length on the inserted value.
 lv_textarea_set_text(o,next);wipeText(next,length-(to-from));writing::release(next);if(!lv_obj_is_valid(o))return false;
 lv_textarea_set_cursor_pos(o,a);selection(o,a,a);if(n)lv_textarea_add_text(o,add);
 if(!lv_obj_is_valid(o))return false;if(editHistory){editHistory->restoring=false;checkpoint(o,*editHistory);}return true;
}
}
bool copy(const char* text,size_t n,bool markdown){if(!text||n>writing::Document::Limit)return false;char* next=static_cast<char*>(writing::allocate(n+1));if(!next)return false;memcpy(next,text,n);next[n]=0;if(copied)wipeText(copied,strlen(copied));writing::release(copied);copied=next;rich=markdown;secretCopy=false;return true;}
bool copySecret(const char* text,size_t n){if(!copy(text,n))return false;secretCopy=true;secretCopiedAt=lv_tick_get();return true;}
void clearSecrets(){if(!secretCopy)return;if(copied)wipeText(copied,strlen(copied));writing::release(copied);copied=nullptr;rich=secretCopy=false;}
void serviceSecrets(){if(secretCopy&&lv_tick_elaps(secretCopiedAt)>=30000)clearSecrets();}
const char* clipboard(){serviceSecrets();return copied?copied:"";}
bool protectedField(lv_obj_t* o){if(!o||!lv_obj_is_valid(o))return false;for(uint32_t i=0;i<lv_obj_get_event_count(o);++i)if(lv_event_dsc_get_cb(lv_obj_get_event_dsc(o,i))==protectedEvent)return true;return false;}
void protect(lv_obj_t* o){if(!o||!lv_obj_check_type(o,&lv_textarea_class))return;if(!protectedField(o))lv_obj_add_event_cb(o,protectedEvent,LV_EVENT_DELETE,nullptr);if(auto* h=history(o))resetHistory(*h);}
bool markdown(){return rich;}
void track(lv_obj_t* o){if(!o||!lv_obj_is_valid(o)||!lv_obj_check_type(o,&lv_textarea_class)||readOnly(o)||(lv_textarea_get_password_mode(o)||protectedField(o))||history(o))return;auto* h=new(std::nothrow) History;if(!h)return;lv_obj_add_event_cb(o,historyEvent,LV_EVENT_ALL,h);checkpoint(o,*h);}
bool canUndo(lv_obj_t* o){auto* h=history(o);return h&&!readOnly(o)&&!(lv_textarea_get_password_mode(o)||protectedField(o))&&h->position>0;}
bool canRedo(lv_obj_t* o){auto* h=history(o);return h&&!readOnly(o)&&!(lv_textarea_get_password_mode(o)||protectedField(o))&&h->position+1<h->states.size();}
void setReadOnly(lv_obj_t* o){if(!o||!lv_obj_check_type(o,&lv_textarea_class))return;if(!readOnly(o))lv_obj_add_event_cb(o,readOnlyEvent,LV_EVENT_INSERT,nullptr);if(auto* h=history(o))resetHistory(*h);lv_textarea_set_max_length(o,0);lv_textarea_set_text_selection(o,true);}
bool readOnly(lv_obj_t* o){if(!o)return false;for(uint32_t i=0;i<lv_obj_get_event_count(o);++i)if(lv_event_dsc_get_cb(lv_obj_get_event_dsc(o,i))==readOnlyEvent)return true;return false;}
void selectable(lv_obj_t* o){if(!o||!lv_obj_check_type(o,&lv_label_class)||labelState(o))return;lv_obj_add_flag(o,static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_USER_1));lv_obj_add_event_cb(o,labelEvent,LV_EVENT_ALL,new LabelSelection);}
bool hasSelection(lv_obj_t* o){if(!o||!lv_obj_is_valid(o))return false;auto* l=lv_obj_check_type(o,&lv_textarea_class)?lv_textarea_get_label(o):o;if(!lv_obj_check_type(l,&lv_label_class))return false;auto a=lv_label_get_text_selection_start(l),b=lv_label_get_text_selection_end(l);return a!=LV_DRAW_LABEL_NO_TXT_SEL&&b!=LV_DRAW_LABEL_NO_TXT_SEL&&a!=b;}
bool copySelection(lv_obj_t* o){if(!hasSelection(o)||(lv_obj_check_type(o,&lv_textarea_class)&&(lv_textarea_get_password_mode(o)||protectedField(o))))return false;auto* l=lv_obj_check_type(o,&lv_textarea_class)?lv_textarea_get_label(o):o;const char* text=lv_label_get_text(l);size_t a=lv_label_get_text_selection_start(l),b=lv_label_get_text_selection_end(l);if(a>b)std::swap(a,b);a=writing::byteOffset(text,a);b=writing::byteOffset(text,b);return copy(text+a,b-a);}
bool insert(lv_obj_t* o,const char* text){
 if(!o||!text||!lv_obj_is_valid(o)||!lv_obj_check_type(o,&lv_textarea_class)||readOnly(o))return false;
 observe(o);
 auto* label=lv_textarea_get_label(o);size_t a=lv_label_get_text_selection_start(label),b=lv_label_get_text_selection_end(label);
 if(a==LV_DRAW_LABEL_NO_TXT_SEL||b==LV_DRAW_LABEL_NO_TXT_SEL)a=b=lv_textarea_get_cursor_pos(o);if(a>b)std::swap(a,b);return replace(o,a,b,text);
}
bool key(lv_obj_t* o,const writing::KeyEvent& e){
 using K=writing::Key;
 if(o&&lv_obj_is_valid(o)&&lv_obj_check_type(o,&lv_label_class))return labelKey(o,e);
 if(!o||!lv_obj_is_valid(o)||!lv_obj_check_type(o,&lv_textarea_class)||lv_obj_has_state(o,LV_STATE_DISABLED))return false;
 observe(o);const char* text=lv_textarea_get_text(o);const size_t length=writing::charCount(text,strlen(text));
 const size_t caret=std::min<size_t>(lv_textarea_get_cursor_pos(o),length);auto* label=lv_textarea_get_label(o);
 size_t a=lv_label_get_text_selection_start(label),b=lv_label_get_text_selection_end(label);
 if(a==LV_DRAW_LABEL_NO_TXT_SEL||b==LV_DRAW_LABEL_NO_TXT_SEL)a=b=caret;
 a=std::min(a,length);b=std::min(b,length);if(a>b)std::swap(a,b);
 const bool command=e.ctrl||e.meta;char c=e.text[0];if(c>='A'&&c<='Z')c+=32;
 if(command&&e.key==K::Text&&(c=='z'||c=='y')){restore(o,c=='y'||e.shift);return true;}
 if((command&&e.key==K::Text&&c=='a')){lv_textarea_set_cursor_pos(o,length);selection(o,0,length);return true;}
 if((command&&e.key==K::Text&&(c=='c'||c=='x'))||(e.key==K::Insert&&command)||(e.key==K::Delete&&e.shift)){
  if(a<b&&!(lv_textarea_get_password_mode(o)||protectedField(o))){size_t first=writing::byteOffset(text,a),last=writing::byteOffset(text,b);if(copy(text+first,last-first)&&!readOnly(o)&&((e.key==K::Text&&c=='x')||e.key==K::Delete))replace(o,a,b,"");}return true;
 }
 if(readOnly(o)&&((e.key==K::Text&&!command)||e.key==K::Backspace||e.key==K::Delete||e.key==K::Enter||e.key==K::Insert||(command&&e.key==K::Text&&c=='v')))return true;
 if((command&&e.key==K::Text&&c=='v')||(e.key==K::Insert&&e.shift)){if(*clipboard())replace(o,a,b,clipboard());return true;}
 if(e.key==K::Insert){overwrite=!overwrite;return true;}
 auto cp=[&](size_t pos){return (unsigned char)text[writing::byteOffset(text,std::min(pos,length))];};
 auto previousWord=[&](size_t pos){while(pos&&cp(pos-1)<=32)--pos;while(pos&&cp(pos-1)>32)--pos;return pos;};
 auto nextWord=[&](size_t pos){while(pos<length&&cp(pos)>32)++pos;while(pos<length&&cp(pos)<=32)++pos;return pos;};
 if(e.key==K::Backspace||e.key==K::Delete){if(a==b){if(e.key==K::Backspace)a=command?previousWord(caret):caret?caret-1:0;else b=command?nextWord(caret):std::min(length,caret+1);}if(a!=b)replace(o,a,b,"");return true;}
 if(e.key==K::Text&&!command&&!e.alt){if(a!=b||overwrite)replace(o,a,overwrite&&a==b?std::min(length,b+1):b,e.text);else lv_textarea_add_text(o,e.text);return true;}
 if(e.key==K::Enter){if(lv_textarea_get_one_line(o))lv_obj_send_event(o,LV_EVENT_READY,nullptr);else replace(o,a,b,"\n");return true;}
 if(e.key==K::Tab)return false;
 size_t dest=caret;
 if(e.key==K::Left)dest=command?previousWord(caret):(!e.shift&&a!=b?a:caret?caret-1:0);
 else if(e.key==K::Right)dest=command?nextWord(caret):(!e.shift&&a!=b?b:std::min(length,caret+1));
 else if(e.key==K::Home){dest=command?0:caret;while(dest&&cp(dest-1)!='\n')--dest;}
 else if(e.key==K::End){dest=command?length:caret;while(dest<length&&cp(dest)!='\n')++dest;}
 else if(e.key==K::Up||e.key==K::Down||e.key==K::PageUp||e.key==K::PageDown){
  int steps=1;if(e.key==K::PageUp||e.key==K::PageDown)steps=std::max(1,int(lv_obj_get_content_height(o))/std::max(1,int(lv_obj_get_style_text_font(o,LV_PART_MAIN)->line_height)));
  for(int i=0;i<steps;++i){if(e.key==K::Up||e.key==K::PageUp)lv_textarea_cursor_up(o);else lv_textarea_cursor_down(o);}dest=lv_textarea_get_cursor_pos(o);
 }else return false;
 const size_t anchor=a==b?caret:caret==a?b:a;
 lv_textarea_set_cursor_pos(o,dest);selection(o,e.shift?anchor:dest,dest);return true;
}
}
