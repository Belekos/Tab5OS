from pathlib import Path
import subprocess,shutil,re,sys
r=Path(__file__).resolve().parents[2];t=r/'tools/settings-ui-tests-0840';s=r/'Tab5OS_Arduino_v0.8.40_Ecriture';w=Path('/workspace/scratch/644cc83c43c8/build_0840/settings-balance');w.mkdir(exist_ok=True)
current=Path('/workspace/scratch/644cc83c43c8/build_0840/settings-ui');shutil.copyfile(current/'settings-translations.inc',w/'settings-translations.inc')
prod=(current/'settings-production.inc').read_text();old='L::columns(settingsSectionPanels[0],{displayCard,mouseCard},{editCard,gamepadCard});';new='L::columns(settingsSectionPanels[0],{displayCard,gamepadCard},{editCard,mouseCard});';prod=prod.replace(new,old);assert old in prod
(w/'settings-production.inc').write_text(prod.replace(old,'if(balanceCandidate){'+new+'}else{'+old+'}'))
code=(t/'layout.cpp').read_text();code=code[:code.index('int main(')];code='static bool balanceCandidate=false;\n'+code
code+=r'''
int main(int argc,char**argv){FILE*f=fopen(argv[1],"rb");fseek(f,0,SEEK_END);size_t size=ftell(f);rewind(f);std::vector<uint8_t> bytes(size);fread(bytes.data(),1,size,f);fclose(f);lv_init();lv_display_create(1280,720);
 for(unsigned px:{22u,24u}){font=lv_tiny_ttf_create_data(bytes.data(),bytes.size(),px);font->fallback=px==22?&lv_font_montserrat_22:&lv_font_montserrat_24;
 for(lang=0;lang<languageCount;++lang){language(lang);for(int width:{1216,1280})for(bool candidate:{false,true}){balanceCandidate=candidate;WORK_W=width;Tab5Shell::workspaceWidth=width;pageHost=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(pageHost);lv_obj_set_size(pageHost,width,PAGE_H);lv_obj_set_style_text_font(pageHost,font,0);createSettingsPage();
 for(unsigned section=0;section<2;++section){showSettingsSection(section);lv_obj_update_layout(pageHost);geometry(settingsSectionPanels[section]);compact(settingsSectionPanels[section]);auto* left=lv_obj_get_child(settingsSectionPanels[section],0);auto*right=lv_obj_get_child(settingsSectionPanels[section],1);unsigned a=lv_obj_get_height(left),b=lv_obj_get_height(right);printf("%u,%u,%d,%d,%u,%u,%u\n",px,lang,width,int(candidate),section,a,b);}
 lv_obj_delete(pageHost);
 }}lv_tiny_ttf_destroy(font);}lv_deinit();return 0;}
'''
(w/'measure.cpp').write_text(code)
lvgl=Path('/workspace/scratch/644cc83c43c8/build_0840/host-dependencies/lvgl-9.6.0');lib=Path('/workspace/scratch/644cc83c43c8/host_lvgl_0840')
cmd=['g++','-std=c++17','-g','-O1','-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-Wno-cpp','-Wno-deprecated-declarations','-DLV_CONF_PATH="'+str(lib/'lv_conf.h')+'"','-I'+str(t/'mocks'),'-I'+str(w),'-I'+str(s),'-I'+str(lvgl),str(w/'measure.cpp'),str(lib/'liblvgl.a'),'-pthread','-o',str(w/'measure')]
b=subprocess.run(cmd,capture_output=True,text=True);(w/'build.log').write_text(b.stdout+b.stderr);b.check_returncode()
import os,json
run=subprocess.run([str(w/'measure'),str(s/'sdcard/System/font.ttf')],env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),capture_output=True,text=True);(w/'measure.csv').write_text(run.stdout);(r/'tools/verification/settings-ui-0840/balance.csv').write_text('font,language,width,candidate,section,leftHeight,rightHeight\n'+run.stdout);run.check_returncode()
rows=[list(map(int,row.split(','))) for row in run.stdout.strip().splitlines()];fr=int(re.search(r'frenchIndex=(\d+)',(w/'settings-translations.inc').read_text()).group(1))
print(json.dumps({'frenchIndex':fr,'french': [row for row in rows if row[1]==fr],'maxima':[{'font':px,'candidate':candidate,'section':section,'maxHeight':max(max(row[-2:]) for row in rows if row[0]==px and row[3]==candidate and row[4]==section),'maxImbalance':max(abs(row[-2]-row[-1]) for row in rows if row[0]==px and row[3]==candidate and row[4]==section)} for px in [22,24] for candidate in [0,1] for section in [0,1]]},indent=2))
