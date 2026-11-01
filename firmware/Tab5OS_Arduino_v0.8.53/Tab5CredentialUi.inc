// Included inside Tab5ToolsApp::Impl. All callbacks run on the UI task.
 lv_obj_t *legacyPassword=nullptr,*backupMode=nullptr,*backupHint=nullptr,*portableLabel=nullptr,*confirmLabel=nullptr;
 lv_obj_t *secretList=nullptr,*secretName=nullptr,*secretValue=nullptr,*secretDelete=nullptr,*keyDelete=nullptr;
 std::vector<String> secretNames;
 String deletionIdentity;
 bool securityBusy=false,recoveryPage=false,rebuildingSecurity=false;unsigned pendingSecurity=0;
 lv_obj_t *currentPinField=nullptr,*resetConfirmation=nullptr;
 struct CredentialAction{lv_obj_t* object;int action;};
 std::vector<CredentialAction> credentialActions;
 lv_obj_t* credentialButton(const char* text,int x,int y,int width,int action){
  auto* b=button(text,x,y,width,action);auto* title=lv_obj_get_child(b,0);
  lv_obj_set_width(title,lv_pct(100));lv_label_set_long_mode(title,LV_LABEL_LONG_WRAP);
  lv_obj_set_height(title,LV_SIZE_CONTENT);lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_set_style_min_height(b,44,0);lv_obj_set_height(b,LV_SIZE_CONTENT);credentialActions.push_back({b,action});return b;
 }
 bool securityTool()const{return tool==Tab5Tool::Vault||tool==Tab5Tool::Keys||tool==Tab5Tool::Secrets;}
 // Enter follows the form's visible primary action. On a multi-field PIN
 // form it advances first, so changing/creating a PIN cannot skip confirmation.
 void securitySubmit(lv_obj_t* field){
  if(!securityTool()||!field||lv_obj_has_state(field,LV_STATE_DISABLED))return;
  if(securityBusy||Tab5KeyStore::busy()||Tab5Vault::busy()){note("Opération en cours : patienter ou annuler");return;}
  auto next=[&](lv_obj_t* target){inputKeyboard(target);};
  int action=0;
  if(vaultGate){
   if(field==currentPinField){next(password);return;}
   if(field==password&&confirmation){next(confirmation);return;}
   if(field==password||field==confirmation)action=47;
   else if(field==legacyPassword)action=44;
   else if(field==resetConfirmation)action=431;
  }else if(tool==Tab5Tool::Secrets){
   if(field==secretName){next(secretValue);return;}
   if(field==secretValue)action=421;
  }else if(tool==Tab5Tool::Keys){
   if(field==keyName)action=40;
   else if(field==password&&confirmation){next(confirmation);return;}
   else if(field==confirmation)action=41;
  }else if(tool==Tab5Tool::Vault&&field==password&&legacyFile){act(30);return;}
  if(!action)return;
  updateCredentialActions();
  for(const auto& item:credentialActions)if(item.action==action){
   if(!lv_obj_has_state(item.object,LV_STATE_DISABLED))act(action);
   else if(action==47)validPin(true);
   return;
  }
 }
 bool securityKeyboard(const writing::KeyEvent& event){
  if(!securityTool())return false;
  auto* focused=Tab5Focus.focused();using K=writing::Key;
  if(event.key==K::Escape){
   if(focused&&lv_obj_check_type(focused,&lv_dropdown_class)&&lv_dropdown_is_open(focused)){
    lv_dropdown_close(focused);Tab5Focus.finishEditing();return true;
   }
   // Cancelling the PIN screen invalidates the worker's generation before
   // its UI is released. It must not unlock a vault after the user has left.
   if(securityBusy||vaultUnlocking){Tab5KeyStore::lock();Tab5KeyStore::finishUi();}
   if(Tab5Vault::busy())Tab5Vault::cancel();
   hideKeyboard();close();return true;
  }
  if(event.key==K::Enter&&!event.ctrl&&!event.meta&&!event.alt&&focused&&
     lv_obj_check_type(focused,&lv_textarea_class)){
   securitySubmit(focused);return true;
  }
  return false;
 }
 void wipeSecurityFields(){erasePassword(password);erasePassword(confirmation);erasePassword(legacyPassword);erasePassword(secretValue);erasePassword(currentPinField);}
 void resetSecurity(){
  if(securityTool()&&!rebuildingSecurity){
   if(securityBusy||vaultUnlocking)Tab5KeyStore::lock();
   Tab5KeyStore::finishUi();
  }
  wipeSecurityFields();legacyPassword=backupMode=backupHint=portableLabel=confirmLabel=nullptr;
  credentialActions.clear();
  secretList=secretName=secretValue=secretDelete=keyDelete=nullptr;
  std::vector<String>().swap(secretNames);deletionIdentity="";keyJob=securityBusy=recoveryPage=false;pendingSecurity=0;currentPinField=resetConfirmation=nullptr;
 }
 lv_obj_t* credentialField(int x,int y,int width,unsigned limit,bool pin=false){
  auto* o=field("",x,y,width);Tab5TextInput::protect(o);lv_textarea_set_password_mode(o,true);
  lv_textarea_set_password_show_time(o,0);lv_textarea_set_max_length(o,limit);
  if(pin)lv_textarea_set_accepted_chars(o,"0123456789");return o;
 }
 void securityReopen(bool change=false,bool recover=false){pendingSecurity=recover?3:change?2:1;}
 void beginSecurityJob(bool started){
  if(started){securityBusy=true;hideKeyboard();wipeSecurityFields();}
  updateCredentialActions();
  note(Tab5KeyStore::status());
 }
 void updateCredentialActions(){
  const bool keyBusy=Tab5KeyStore::busy(),fileBusy=Tab5Vault::busy(),idle=!securityBusy&&!keyBusy&&!fileBusy;
  auto text=[](lv_obj_t* field){return field?lv_textarea_get_text(field):"";};
  auto pinValid=[&](lv_obj_t* field){const char* value=text(field);size_t n=strlen(value);if(n<4||n>12)return false;for(size_t i=0;i<n;++i)if(value[i]<'0'||value[i]>'9')return false;return true;};
  const bool selectedKey=keyList&&lv_dropdown_get_selected(keyList)<keys.size();
  bool selectedSecret=false;for(const auto& name:secretNames)if(name==text(secretName)){selectedSecret=true;break;}
  const bool storeReady=tool!=Tab5Tool::Secrets||Tab5Secrets::ready();
  for(const auto& item:credentialActions){
   bool active=idle;
   switch(item.action){
    case 43:case 32:active=fileBusy;break;
    case 47:active=idle&&pinValid(password)&&(!currentPinField||pinValid(currentPinField))&&(!confirmation||!strcmp(text(password),text(confirmation)));break;
    case 44:active=idle&&strlen(text(legacyPassword))<=256&&utf8Characters(text(legacyPassword))>=12;break;
    case 431:active=idle&&!strcmp(text(resetConfirmation),"RESET");break;
    case 40:active=idle&&*text(keyName);break;
    case 410:active=idle&&selectedKey;break;
    case 41:case 42:{
     bool portable=backupMode&&lv_dropdown_get_selected(backupMode)==1;
     active=idle&&*text(path)&&(item.action==42||selectedKey)&&(!portable||(strlen(text(password))<=256&&utf8Characters(text(password))>=12&&(item.action==42||!strcmp(text(password),text(confirmation)))));break;
    }
    case 420:active=idle&&storeReady;break;
    case 421:active=idle&&storeReady&&*text(secretName)&&!(*text(secretName)>='0'&&*text(secretName)<='9');break;
    case 422:active=idle&&storeReady&&selectedSecret;break;
    case 423:case 424:active=idle&&storeReady&&*text(secretValue);break;
   }
   enabled(item.object,active);
  }
  for(auto* field:{password,confirmation,currentPinField,legacyPassword,resetConfirmation,keyName,keyList,path})enabled(field,idle);
  for(auto* field:{secretName,secretValue,secretList})enabled(field,idle&&storeReady);
  if(backupMode)enabled(backupMode,idle&&Tab5KeyStore::usingDevice());
 }
 bool validPin(bool required){
  const char* pin=password?lv_textarea_get_text(password):"";size_t n=strlen(pin);
  if((required||n)&&(n<4||n>12)){note("PIN : 4 à 12 chiffres");return false;}
  if(confirmation&&strcmp(pin,lv_textarea_get_text(confirmation))){note("Les PIN diffèrent");return false;}return true;
 }
 void buildSecurityGate(){
  vaultGate=true;
  if(recoveryPage){
   label("Réinitialiser le coffre local",0,78,1128);
   label("Les clés de ce Tab5 seront supprimées. Sans sauvegarde portable, leurs fichiers ne pourront plus être déchiffrés.",0,136,1128);
   label("Les fichiers chiffrés et l'ancien coffre sont conservés. Les identifiants sont archivés mais resteront inaccessibles après cette réinitialisation.",0,226,1128);
   label("Saisir RESET pour confirmer",0,344,620);resetConfirmation=field("",0,384,280);lv_textarea_set_max_length(resetConfirmation,5);
   credentialButton("Réinitialiser le coffre",0,454,550,431);credentialButton("Retour",578,454,550,49);
   activeField=resetConfirmation;updateCredentialActions();Tab5UiStyle::tree(root);captureLayout();return;
  }
  bool hasPin=Tab5KeyStore::pinConfigured();
  bool change=vaultChanging&&Tab5KeyStore::unlocked()&&Tab5KeyStore::usingDevice();
  bool lockedPin=hasPin&&!change;
  bool migrate=Tab5KeyStore::unlocked()&&!Tab5KeyStore::usingDevice();
  label(change?"Modifier le PIN":lockedPin?"Déverrouiller le coffre":"Créer le PIN du coffre",0,72,1128);
  if(change&&hasPin){label("PIN actuel",0,138,300);currentPinField=credentialField(0,174,280,12,true);}
  const int x=change&&hasPin?374:0;
  label(lockedPin?"PIN du coffre":"Nouveau PIN",x,138,350);password=credentialField(x,174,280,12,true);
  if(!lockedPin)lv_textarea_set_placeholder_text(password,tr("4 à 12 chiffres"));
  if(!lockedPin){label("Confirmer le PIN",x+374,138,350);confirmation=credentialField(x+374,174,280,12,true);}
  credentialButton(lockedPin?"Déverrouiller":change?"Enregistrer le PIN":migrate?"Migrer le coffre":"Créer le PIN",0,242,550,47);
  if(Tab5KeyStore::unlocked())credentialButton("Retour au coffre",578,242,550,49);
  if(Tab5KeyStore::legacyAvailable()&&!migrate&&!change){
   label("Ancien coffre à mot de passe",0,330,1128);legacyPassword=credentialField(0,370,730,256);
   credentialButton("Ouvrir l'ancien coffre",750,370,378,44);
  }
  credentialButton("Coffre inaccessible ?",0,468,550,430);
  activeField=currentPinField?currentPinField:password;updateCredentialActions();Tab5UiStyle::tree(root);captureLayout();
 }
 void keyBackupMode(){
  bool portable=backupMode&&lv_dropdown_get_selected(backupMode)==1;
  for(auto* o:{password,confirmation,portableLabel,confirmLabel})visible(o,portable);
  if(!portable){erasePassword(password);erasePassword(confirmation);}
  if(backupHint)lv_label_set_text(backupHint,tr(portable?"Copie portable : mot de passe de 12 caractères minimum. La dérivation prend du temps ; Annuler reste disponible.":"Copie rapide : restauration uniquement sur ce Tab5. Pour changer d’appareil, choisir une copie portable."));
 }
 void buildKeyPage(const char* selected){
  credentialButton("Coffre de mots de passe",0,58,440,415);credentialButton("Accès du coffre",460,58,330,46);credentialButton("Verrouiller",810,58,318,45);
  keyList=dropdown("",0,118,790);refreshKeys();keyDelete=credentialButton("Supprimer la clé",810,118,318,410);
  lv_obj_add_event_cb(keyList,[](lv_event_t* e){auto* self=static_cast<Impl*>(lv_event_get_user_data(e));self->deletionIdentity="";lv_label_set_text(lv_obj_get_child(self->keyDelete,0),self->tr("Supprimer la clé"));},LV_EVENT_VALUE_CHANGED,this);
  keyName=field("",0,180,790);lv_textarea_set_max_length(keyName,48);lv_textarea_set_placeholder_text(keyName,tr("Nom de la nouvelle clé"));credentialButton("Générer",810,180,318,40);
  label("Sauvegarde chiffrée / restauration (.t5key)",0,244,1128);
  path=field(selected&&*selected?selected:"/Documents/cle.t5key",0,280,790);credentialButton("Parcourir",810,280,318,411);
  String modes=String(tr("Sur ce Tab5 (rapide)"))+"\n"+tr("Portable (mot de passe)");backupMode=dropdown(modes.c_str(),0,342,1128);
  if(!Tab5KeyStore::usingDevice()){lv_dropdown_set_selected(backupMode,1);enabled(backupMode,false);}
  backupHint=label("",0,402,1128);
  portableLabel=label("Mot de passe de sauvegarde",0,474,550);confirmLabel=label("Confirmation pour sauvegarder",578,474,550);
  password=credentialField(0,512,550,256);confirmation=credentialField(578,512,550,256);
  credentialButton("Sauvegarder la clé",0,582,350,41);credentialButton("Restaurer une clé",370,582,350,42);credentialButton("Annuler",740,582,388,43);
  lv_obj_set_pos(status,0,646);keyBackupMode();
  lv_obj_add_event_cb(backupMode,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->keyBackupMode();},LV_EVENT_VALUE_CHANGED,this);
  updateCredentialActions();
 }
 void secretLoad(){
  erasePassword(secretValue);deletionIdentity="";lv_label_set_text(lv_obj_get_child(secretDelete,0),tr("Supprimer"));
  unsigned n=lv_dropdown_get_selected(secretList);if(n>=secretNames.size()){lv_textarea_set_text(secretName,"");return;}
  lv_textarea_set_text(secretName,secretNames[n].c_str());char value[Tab5Secrets::MaxValueBytes+1]{};
  if(Tab5Secrets::get(secretNames[n].c_str(),value,sizeof(value)))lv_textarea_set_text(secretValue,value);else note(Tab5Secrets::status());
  tab5vault::wipe(value,sizeof(value));lv_textarea_set_password_mode(secretValue,true);
 }
 void secretRefresh(const char* preferred=nullptr){
  secretNames=Tab5Secrets::names();String options;unsigned selection=0;
  for(unsigned i=0;i<secretNames.size();++i){if(i)options+='\n';options+=secretNames[i];if(preferred&&secretNames[i]==preferred)selection=i;}
  lv_dropdown_set_options(secretList,options.length()?options.c_str():tr("Aucun identifiant enregistré"));lv_dropdown_set_selected(secretList,selection);secretLoad();
 }
 void buildSecretPage(){
  credentialButton("Clés de chiffrement",0,58,440,416);credentialButton("Accès du coffre",460,58,330,46);credentialButton("Verrouiller",810,58,318,45);
  if(!Tab5KeyStore::usingDevice()){
   label("Copiez les clés vers le coffre appareil pour utiliser le gestionnaire de mots de passe.",0,138,1128);credentialButton("Accès du coffre",0,230,550,46);updateCredentialActions();return;
  }
  label("Identifiants enregistrés",0,124,1128);secretList=dropdown("",0,160,790);credentialButton("Nouveau",810,160,318,420);
  label("Nom de variable · lettres, chiffres et _",0,226,1128);secretName=field("",0,264,1128);lv_textarea_set_max_length(secretName,Tab5Secrets::MaxNameBytes);
  lv_textarea_set_accepted_chars(secretName,"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
  label("Identifiant, mot de passe ou token",0,328,1128);secretValue=credentialField(0,366,790,Tab5Secrets::MaxValueBytes);credentialButton("Afficher / masquer",810,366,318,424);
  credentialButton("Enregistrer",0,436,350,421);credentialButton("Copier",370,436,350,423);secretDelete=credentialButton("Supprimer",740,436,388,422);
  lv_obj_set_pos(status,0,590);secretRefresh();
  if(!Tab5Secrets::ready()){note(Tab5Secrets::status());credentialButton("Réessayer",0,504,350,425);}
  else label("Accessible par son nom dans le terminal, Python et les applications. Verrouiller le coffre coupe l'accès aux valeurs protégées.",0,504,1128);
  lv_obj_add_event_cb(secretList,[](lv_event_t* e){static_cast<Impl*>(lv_event_get_user_data(e))->secretLoad();},LV_EVENT_VALUE_CHANGED,this);
  for(auto* o:{secretName,secretValue})lv_obj_add_event_cb(o,[](lv_event_t* e){auto* self=static_cast<Impl*>(lv_event_get_user_data(e));self->deletionIdentity="";if(self->secretDelete)lv_label_set_text(lv_obj_get_child(self->secretDelete,0),self->tr("Supprimer"));},LV_EVENT_VALUE_CHANGED,this);
  updateCredentialActions();
 }
 bool securityAction(int action){
  if(!securityTool())return false;
  if(action==45){Tab5KeyStore::lock();Tab5TextInput::clearSecrets();close();return true;}
  if(action==43||action==32){Tab5Vault::cancel();return true;}
  if(Tab5Vault::busy()||Tab5KeyStore::busy()){note("Opération en cours : patienter ou annuler");return true;}
  vaultUpdates=false;
  if(action==46){securityReopen(true);return true;}
  if(action==430){securityReopen(false,true);return true;}
  if(action==431){if(!resetConfirmation||strcmp(lv_textarea_get_text(resetConfirmation),"RESET")){note("Saisir RESET pour confirmer");return true;}beginSecurityJob(Tab5KeyStore::beginReset(true));return true;}
  if(action==49){securityReopen();return true;}
  if(action==47){
   if(!validPin(true))return true;
   bool change=vaultChanging&&Tab5KeyStore::unlocked()&&Tab5KeyStore::usingDevice();
   bool started=change?Tab5KeyStore::beginPinChange(currentPinField?lv_textarea_get_text(currentPinField):"",lv_textarea_get_text(password),true):
    Tab5KeyStore::beginDeviceUnlock(lv_textarea_get_text(password),!Tab5KeyStore::pinConfigured(),Tab5KeyStore::unlocked()&&!Tab5KeyStore::usingDevice(),true);
   beginSecurityJob(started);return true;
  }
  if(vaultGate){
   if(action==44&&legacyPassword){if(strlen(lv_textarea_get_text(legacyPassword))>256){note("Valeur trop longue");return true;}Tab5KeyStore::lock();vaultUnlocking=Tab5KeyStore::unlock(lv_textarea_get_text(legacyPassword),false);erasePassword(legacyPassword);note(Tab5KeyStore::status());}return true;
  }
  if(action==415){pendingSecurity=4;return true;}if(action==416){pendingSecurity=5;return true;}
  if(!Tab5KeyStore::unlocked()){securityReopen();return true;}
  if(action==425){beginSecurityJob(Tab5KeyStore::beginDeviceUnlock(nullptr,false,false,true));return true;}
  if(tool==Tab5Tool::Keys){
   if(action==40){int created=Tab5KeyStore::create(lv_textarea_get_text(keyName));if(created>=0){refreshKeys();for(unsigned i=0;i<keys.size();++i)if(keys[i].slot==unsigned(created)){lv_dropdown_set_selected(keyList,i);break;}lv_textarea_set_text(keyName,"");deletionIdentity="";lv_label_set_text(lv_obj_get_child(keyDelete,0),tr("Supprimer la clé"));}note(Tab5KeyStore::status());}
   if(action==410){unsigned n=lv_dropdown_get_selected(keyList);if(n>=keys.size()){note("Aucune clé sélectionnée");return true;}String identity=keys[n].id;
    if(identity!=deletionIdentity){deletionIdentity=identity;lv_label_set_text(lv_obj_get_child(keyDelete,0),tr("Confirmer la suppression"));note("Sans sauvegarde de cette clé, ses fichiers chiffrés ne pourront plus être ouverts.");return true;}
    bool ok=Tab5KeyStore::erase(keys[n].slot);deletionIdentity="";if(ok)refreshKeys();lv_label_set_text(lv_obj_get_child(keyDelete,0),tr("Supprimer la clé"));note(Tab5KeyStore::status());
   }
   if(action==411)pick(path);
   if(action==41||action==42){
    const char* pass=lv_textarea_get_text(password);bool portable=lv_dropdown_get_selected(backupMode)==1;
    if(portable&&strlen(pass)>256){note("Valeur trop longue");return true;}
    if(portable&&utf8Characters(pass)<12){note("Mot de passe : au moins 12 caractères");return true;}
    if(portable&&action==41&&strcmp(pass,lv_textarea_get_text(confirmation))){note("Les mots de passe diffèrent");return true;}
    String backup=lv_textarea_get_text(path);if(!backup.length()){note("Choisir un chemin de sauvegarde .t5key");return true;}
    bool ok=portable?(action==41?Tab5Vault::backupKey(selectedKey(),backup,pass):Tab5Vault::restoreKey(backup,pass)):(action==41?Tab5Vault::backupDevice(selectedKey(),backup):Tab5Vault::restoreDevice(backup));
    if(ok){keyJob=vaultUpdates=true;erasePassword(password);erasePassword(confirmation);}note(Tab5Vault::status());
   }
   return true;
  }
  if(tool==Tab5Tool::Secrets){
   if(!secretValue)return true;
   if(action==420){erasePassword(secretValue);lv_textarea_set_text(secretName,"");lv_textarea_set_password_mode(secretValue,true);inputKeyboard(secretName);}
   if(action==421){String name=lv_textarea_get_text(secretName);if(Tab5Secrets::set(name.c_str(),lv_textarea_get_text(secretValue))){bool bound=tab5_env_bind(name.c_str(),name.c_str())==0;secretRefresh(name.c_str());note(bound?"Identifiant enregistré":"Identifiant enregistré · environnement plein");}else note(Tab5Secrets::status());}
   if(action==422){String name=lv_textarea_get_text(secretName);if(!name.length())return true;
    if(deletionIdentity!=name){deletionIdentity=name;lv_label_set_text(lv_obj_get_child(secretDelete,0),tr("Confirmer la suppression"));return true;}
    if(Tab5Secrets::erase(name.c_str())){Tab5TextInput::clearSecrets();secretRefresh();note("Identifiant supprimé");}else note(Tab5Secrets::status());
   }
   if(action==423)note(Tab5TextInput::copySecret(lv_textarea_get_text(secretValue),strlen(lv_textarea_get_text(secretValue)))?"Copié":"Copie impossible");
   if(action==424)lv_textarea_set_password_mode(secretValue,!lv_textarea_get_password_mode(secretValue));
   return true;
  }
  return false;
 }
 void securityService(){
  if(!root||!securityTool())return;
  updateCredentialActions();
  if(pendingSecurity){unsigned next=pendingSecurity;pendingSecurity=0;String selected=vaultSelection;
   rebuildingSecurity=true;tab5DiagnosticMark(6,"ui-rebuild","",1);
   open(next==4?Tab5Tool::Secrets:next==5?Tab5Tool::Keys:tool,selected.c_str(),false,next==2,next==3);
   rebuildingSecurity=false;Tab5KeyStore::finishUi();return;
  }
  if(securityBusy){
   note(Tab5KeyStore::status());
   if(!Tab5KeyStore::busy()){securityBusy=false;if(Tab5KeyStore::operationSucceeded())securityReopen();}
   return;
  }
  if(vaultGate){
   if(vaultUnlocking||Tab5KeyStore::busy())note(Tab5KeyStore::status());
   if(vaultUnlocking&&!Tab5KeyStore::busy()){vaultUnlocking=false;if(Tab5KeyStore::unlocked())securityReopen();}return;
  }
  if(!Tab5KeyStore::unlocked()){Tab5TextInput::clearSecrets();securityReopen();return;}
  if(vaultUpdates||Tab5Vault::busy())note(Tab5Vault::status());
  if(tool==Tab5Tool::Keys&&keyJob&&!Tab5Vault::busy()){keyJob=false;refreshKeys();vaultUpdates=false;}
 }
