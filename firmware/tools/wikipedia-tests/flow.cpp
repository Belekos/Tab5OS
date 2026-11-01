#include <lvgl.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>
#include <filesystem>
#include <cerrno>
#ifndef ESTALE
#define ESTALE 116
#endif
#include "Tab5ReaderLines.h"
#include "Tab5WikiTable.h"
static unsigned checks=0,failures=0;
static void check(bool condition,const char* text){++checks;if(!condition){++failures;if(failures<20)printf("FAIL %s\n",text);}}
static size_t allocated=0,peak=0;static bool failAllocation=false;
static std::map<void*,size_t> allocations;
static void* heap_caps_realloc(void* p,size_t n,int){if(failAllocation){failAllocation=false;return nullptr;}size_t old=p?allocations[p]:0;void* q=realloc(p,n);if(q){allocations.erase(p);allocations[q]=n;allocated+=n-old;peak=std::max(peak,allocated);}return q;}
static void heap_caps_free(void* p){if(p){allocated-=allocations[p];allocations.erase(p);free(p);}}
template<class T,size_t Limit> class Tab5PsramTable {T* p=nullptr;size_t n=0;public:~Tab5PsramTable(){clear();}bool ensure(size_t count){if(count>Limit)return false;if(count<=n)return true;auto* q=static_cast<T*>(heap_caps_realloc(p,count*sizeof(T),0));if(!q)return false;p=q;n=count;return true;}void clear(){heap_caps_free(p);p=nullptr;n=0;}T& operator[](size_t i){return p[i];}};
namespace Tab5PythonUI {struct ReaderRequest{char path[256]{},title[192]{};bool rich=true,illustrated=false,canSave=false,canUpdate=false;uint32_t revision=1;};}
namespace Tab5ImageDecode {struct Image {void clear(){}};}
namespace Tab5UiStyle {static uint32_t color(unsigned){return 0x123456;}}
namespace Tab5PythonReader {struct Hooks{};}
using Tab5PythonReader::Hooks;
static std::map<std::string,std::shared_ptr<std::string>> files;
struct File {std::shared_ptr<std::string> data;size_t pos=0;operator bool()const{return bool(data);}bool isDirectory(){return false;}size_t size(){return data?data->size():0;}bool seek(size_t at){if(!data||at>data->size())return false;pos=at;return true;}size_t read(uint8_t* bytes,size_t n){if(!data)return 0;n=std::min(n,data->size()-pos);memcpy(bytes,data->data()+pos,n);pos+=n;return n;}};
static struct {File open(const char* path,int=0){return {files.count(path)?files[path]:nullptr};}} tab5FS;
static struct Focus {void clear(){}void describe(lv_obj_t*,const char*){}lv_obj_t* focused(){return nullptr;}bool contains(lv_obj_t*,lv_obj_t*){return false;}} Tab5Focus;
struct Tab5StorageUse{operator bool()const{return true;}};
static uint32_t millis(){static uint32_t now=0;return ++now;}static void delay(int){}
static void tab5StorageFault(const char*,const char*,int){}
static const char* tr(const char* value){return value;}static lv_color_t color(unsigned role){return lv_color_hex(role==2?0xffffff:0x202020);}
static void end(int,const char* =nullptr){}
static size_t sidecarRead(void* context,uint32_t at,uint8_t* data,size_t n){auto& f=*static_cast<File*>(context);return f.seek(at)?f.read(data,n):0;}static void sidecarYield(){}
#include "wiki-state.inc"
static void status(){}
static void clearPicture(){if(state->picture)lv_obj_delete(state->picture);state->picture=nullptr;state->imageSpace=0;}
#if CURRENT_FLOW
static void pictureLayout(uint32_t,int top,int){state->picture=lv_obj_create(state->screen);lv_obj_set_pos(state->picture,24,top);lv_obj_set_size(state->picture,120,120);state->imageSpace=132;}
#else
static void pictureLayout(){state->height=state->fullHeight;}
#endif
#include "wiki-functions.inc"
static lv_font_t* font=nullptr;
static void begin(const std::string& text,const std::string& links){files.clear();files["/article"]=std::make_shared<std::string>(text);files["/article.links"]=std::make_shared<std::string>(links);state=new State;strcpy(state->request.path,"/article");state->size=text.size();state->width=672;state->height=state->fullHeight=1120;state->font=font;state->window=static_cast<char*>(malloc(WindowSize+1));state->screen=lv_obj_create(nullptr);lv_obj_set_size(state->screen,720,1280);state->text=lv_label_create(state->screen);lv_obj_set_size(state->text,state->width,state->height);lv_obj_set_style_text_font(state->text,font,0);lv_obj_set_style_text_line_space(state->text,state->spacing,0);lv_screen_load(state->screen);loadTables();}
static void finish(){auto* fallback=lv_obj_create(nullptr);lv_screen_load(fallback);lv_obj_delete(state->screen);free(state->window);delete state;state=nullptr;lv_obj_delete(fallback);check(allocated==0,"all PSRAM containers released after reader close");}
static std::pair<std::string,std::string> fixture(unsigned rows,unsigned cols,bool prose=true,unsigned cellSize=0){std::string text=prose?"Introduction courte.\n\n":"",links="B\t"+std::to_string(text.size())+"\t1\n";for(unsigned r=0;r<rows;++r)for(unsigned c=0;c<cols;++c){unsigned begin=text.size();text+=cellSize?std::string(cellSize,'x'):"cell-"+std::to_string(r)+"-"+std::to_string(c);text+='\n';links+="C\t"+std::to_string(begin)+"\t"+std::to_string(text.size())+"\t1\t"+std::to_string(r)+"\t"+std::to_string(c)+"\t1\t1\t0\t1\n";}links+="E\t"+std::to_string(text.size())+"\t1\t"+std::to_string(cols)+"\n";if(prose)text+="Conclusion après le tableau.\n";return {text,links};}
int main(int argc,char** argv){lv_init();lv_display_create(720,1280);std::ifstream input(argv[1],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});font=lv_tiny_ttf_create_data(bytes.data(),bytes.size(),24);check(font!=nullptr,"shipped font loaded");
 const auto fonts=std::filesystem::path(argv[1]).parent_path()/"fonts";std::ifstream jaFile(fonts/"cjk-ja.ttf",std::ios::binary),koFile(fonts/"cjk-ko.ttf",std::ios::binary);std::vector<char> jaBytes((std::istreambuf_iterator<char>(jaFile)),{}),koBytes((std::istreambuf_iterator<char>(koFile)),{});auto* ja=lv_tiny_ttf_create_data(jaBytes.data(),jaBytes.size(),24);auto* ko=lv_tiny_ttf_create_data(koBytes.data(),koBytes.size(),24);check(ja&&ko,"shipped CJK fallback fonts loaded");font->fallback=ja;ja->fallback=ko;
 {auto f=fixture(2,2);begin(f.first,f.second);check(!render(),"mixed page renders");check(state->next==state->size,"intro + all four cells + conclusion share one page");check(state->cellViews.size()>=6,"four cells and two prose slices retained");check(state->tablePanel!=nullptr,"real table widgets created");finish();}
 {auto f=fixture(100,3,false);begin(f.first,f.second);unsigned pages=0,cells=0;while(state->offset<state->size&&pages<400){check(!render(),"large table renders");check(state->next>state->offset,"table always advances");cells+=state->cellViews.size();state->offset=state->next;++pages;}check(cells==300,"all 300 table cells reachable once");check(pages<=5,"table rows grouped, not one cell per page");finish();}
 {auto f=fixture(1,1,false,12000);begin(f.first,f.second);check(!render(),"oversize cell renders");check(state->next==state->size,"long cell retained");lv_obj_update_layout(state->screen);check(state->tablePanel&&lv_obj_get_scroll_bottom(state->tablePanel)>0,"long cell is vertically scrollable");finish();}
#if CURRENT_FLOW
 {auto f=fixture(2,2);const unsigned at=f.first.find("cell-0-0");f.second+=std::to_string(at)+"\t"+std::to_string(at+8)+"\tArticle_link\n";begin(f.first,f.second);check(!render(),"mixed page with cell link renders");check(state->linkCount==1,"table link remains interactive");bool linked=false;for(auto& view:state->cellViews)if(view.begin==at)linked=lv_obj_get_child_count(view.box)>1;check(linked,"link box belongs to its scrollable table cell");finish();}
 {std::string text="Texte avant illustration.\nTexte après illustration.\n";begin(text,"");state->request.illustrated=true;state->mediaCount=1;state->media[0].offset=text.find("Texte après");check(!render(),"inline illustration page renders");check(state->next==state->size,"prose before/after image shares page");check(state->picture&&lv_obj_get_y(state->picture)>66,"image follows preceding paragraph");finish();}
 {auto f=fixture(2,2,false);begin(f.first,f.second);failAllocation=true;check(!render(),"allocation failure falls back to readable text");check(state->next==state->size,"failed table allocation does not discard content");finish();}
 for(const char* sample:{"العربية لغة جميلة. ","日本語で文章を読みます。", "한국어 문장을 읽습니다. ","中文文章可以显示。"}){std::string text=sample;for(int i=0;i<8;++i)text+=text;begin(text,"");check(!render(),"Arabic/CJK shaped page renders");for(auto& view:state->cellViews){lv_obj_update_layout(view.label);check(lv_obj_get_y(view.label)+lv_obj_get_height(view.label)<=66+state->fullHeight,"shaped label fits bottom of page");}finish();}
 for(unsigned cycle=0;cycle<100;++cycle){auto f=fixture(3,4);begin(f.first,f.second);check(!render(),"repeat open/table render");finish();}
#endif
 lv_tiny_ttf_destroy(font);lv_tiny_ttf_destroy(ja);lv_tiny_ttf_destroy(ko);printf("checks=%u failures=%u peakPSRAM=%zu retainedPSRAM=%zu\n",checks,failures,peak,allocated);return failures?1:0;}
