// Included after settings helpers, in the main UI translation unit.
static lv_obj_t *serialPortChoice=nullptr,*serialModeChoice=nullptr,*serialBaudChoice=nullptr,*serialFrameChoice=nullptr,*serialEndingChoice=nullptr,*serialStateLabel=nullptr;
static bool serialSettingsPending=false;
static Tab5Serial::Config serialSettingsRequested;
static constexpr uint32_t serialBauds[]={1200,2400,4800,9600,19200,38400,57600,115200,230400,460800,921600};
static void serialSettingsRefresh(){
 const bool uart=lv_dropdown_get_selected(serialPortChoice)==1;
 for(auto* item:{serialBaudChoice,serialFrameChoice}){if(uart)lv_obj_remove_state(item,LV_STATE_DISABLED);else lv_obj_add_state(item,LV_STATE_DISABLED);}
 Tab5Serial::Stats state;if(!Tab5Serial::tryStats(state))return;
 setLabelIfChanged(serialStateLabel,T(state.configuring?"Chargement":state.ready?(state.connected?"Connecté":"Déconnecté"):"Série indisponible."));
}
static void serialSettingsService(){
 if(!serialSettingsPending)return;
 Tab5Serial::Stats state;if(!Tab5Serial::tryStats(state)||state.configuring)return;
 serialSettingsPending=false;
 toast(state.configurationSucceeded&&state.ready&&state.config==serialSettingsRequested?"Réglages série enregistrés.":"Série : réglage impossible ou envoi en cours.",2600);
 serialSettingsRefresh();
}
static lv_obj_t* createSerialSettings(lv_obj_t* panel){
 namespace L=Tab5SettingsLayout;
 auto* card=makeSettingsCard(panel,16,0,WORK_W-32,200,"Série");
 const auto config=Tab5Serial::config();
 auto choice=[&](const char* options,unsigned selected){auto* object=lv_dropdown_create(card);lv_dropdown_set_options(object,options);lv_dropdown_set_selected(object,selected);styleSettingsDropdown(object);L::action(object);return object;};
 serialPortChoice=choice("USB-C\nRS485",unsigned(config.port));
 serialModeChoice=choice(T("Débogage\nDonnées"),unsigned(config.mode));
 serialBaudChoice=choice("1200\n2400\n4800\n9600\n19200\n38400\n57600\n115200\n230400\n460800\n921600",7);
 for(unsigned i=0;i<sizeof(serialBauds)/sizeof(serialBauds[0]);++i)if(serialBauds[i]==config.baud)lv_dropdown_set_selected(serialBaudChoice,i);
 bool standardBaud=false;for(auto baud:serialBauds)if(baud==config.baud)standardBaud=true;
 if(!standardBaud){char value[16];snprintf(value,sizeof(value),"%lu",static_cast<unsigned long>(config.baud));lv_dropdown_add_option(serialBaudChoice,value,LV_DROPDOWN_POS_LAST);lv_dropdown_set_selected(serialBaudChoice,sizeof(serialBauds)/sizeof(serialBauds[0]));}
 serialFrameChoice=choice("8N1\n8E1\n8O1\n8N2",unsigned(config.frame));
 serialEndingChoice=choice(T("Aucune\nLF\nCRLF"),unsigned(config.ending));
 auto label=[&](const char* text){return makeSettingsText(card,text,0,0,140,false);};
 L::row(card,{{label("Port"),130},{serialPortChoice,0,1}});
 L::row(card,{{label("Mode"),130},{serialModeChoice,0,1}});
 L::row(card,{{label("Débit"),130},{serialBaudChoice,0,1}});
 L::row(card,{{label("Format"),130},{serialFrameChoice,0,1}});
 L::row(card,{{label("Fin de ligne"),130},{serialEndingChoice,0,1}});
 auto* apply=makeButton(card,"Appliquer",200,48);
 auto* terminal=makeButton(card,"Terminal",200,48);
 auto* cancel=makeButton(card,"Annuler l'envoi",240,48);
 serialStateLabel=label("");L::row(card,{{apply,0,1},{terminal,0,1}});
 L::row(card,{{cancel,0,1}});L::row(card,{{serialStateLabel,0,1}});
 lv_obj_add_event_cb(serialPortChoice,[](lv_event_t*){serialSettingsRefresh();},LV_EVENT_VALUE_CHANGED,nullptr);
 lv_obj_add_event_cb(apply,[](lv_event_t*){if(serialSettingsPending)return;Tab5Serial::Stats state;if(!Tab5Serial::tryStats(state)){toast("Série : réglage impossible ou envoi en cours.",2600);return;}auto next=state.config;next.port=static_cast<Tab5Serial::Port>(lv_dropdown_get_selected(serialPortChoice));next.mode=static_cast<Tab5Serial::Mode>(lv_dropdown_get_selected(serialModeChoice));unsigned baud=lv_dropdown_get_selected(serialBaudChoice);if(baud<sizeof(serialBauds)/sizeof(serialBauds[0]))next.baud=serialBauds[baud];next.frame=static_cast<Tab5Serial::Frame>(lv_dropdown_get_selected(serialFrameChoice));next.ending=static_cast<Tab5Serial::Ending>(lv_dropdown_get_selected(serialEndingChoice));if(Tab5Serial::tryRequestConfigure(next)){serialSettingsRequested=next;serialSettingsPending=true;serialSettingsRefresh();}else toast("Série : réglage impossible ou envoi en cours.",2600);},LV_EVENT_CLICKED,nullptr);
 lv_obj_add_event_cb(terminal,[](lv_event_t*){setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Terminal);Tab5Terminal::submit("serial");},LV_EVENT_CLICKED,nullptr);
 lv_obj_add_event_cb(cancel,[](lv_event_t*){if(!Tab5Serial::tryClear())toast("Série : réglage impossible ou envoi en cours.",2600);serialSettingsRefresh();},LV_EVENT_CLICKED,nullptr);
 for(auto* action:{apply,terminal,cancel})L::action(action);
 L::card(card);serialSettingsRefresh();return card;
}
