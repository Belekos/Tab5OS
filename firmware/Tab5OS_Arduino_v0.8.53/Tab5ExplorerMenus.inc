// Explorer commands share one contextual menu and one eligibility calculation.
enum class FileMenu { Create, Edit, Actions, View, Sort, Git, Context, ImageOpenWith };
enum class FileCommand { Open, Read, Execute, OpenWith, Copy, Cut, Paste, Rename, Delete,
  Info, Print, Vault, UnlockVault, ViewImage, EditImage, OpenWithOthers, Hide, SelectAll, MultiSelect, ShowHidden, Refresh,
  NewDocument, NewFolder, NewGit, ImportGit, InitGit, Git, GitMenu, Terminal,
  Search, ClearSearch, SortNameAsc, SortNameDesc, SortDateAsc, SortDateDesc, SortSizeAsc, SortSizeDesc };

static void openFileGit(Tab5GitView::Start start=Tab5GitView::Start::Browse) {
  if(fileSelection.paths.size()>1&&start!=Tab5GitView::Start::Create&&start!=Tab5GitView::Start::Import)return;
  const bool creation=start==Tab5GitView::Start::Create||start==Tab5GitView::Start::Import;
  const String path=creation?fileContextDirectory():fileGitDirectory();
  if(!pathStorageReady(path)||!fileGitHost)return;
  rememberFileSession();setKeyboardVisible(false);cancelFolderPreview();freeFilePreviewImage();
  Tab5GitView::Hooks hooks;
  hooks.translate=[](const char* text){return T(text);};
  hooks.font=[](){return uiFont();};
  hooks.keyboard=[](bool show,lv_obj_t* input){if(!show&&!input)keyboardTarget=nullptr;setKeyboardVisible(show,input);};
  hooks.changed=[](){if(activePage==Page::Files)refreshFileList();};
  hooks.directoryChanged=[](const char* directory){openFileContentDirectory(directory);};
  hooks.closed=[](){if(fileGitHost)lv_obj_add_flag(fileGitHost,LV_OBJ_FLAG_HIDDEN);if(activePage==Page::Files&&!Tab5Tools.scope()){restoreFileContent();Tab5Focus.restore(filesPage);}};
  hooks.dialogParent=root;
  hooks.terminal=[](const char* directory){
    if(Tab5Terminal::busy())return false;
    String command="cd '";
    for(const char* p=directory;*p;++p){if(*p=='\'')command+="'\\''";else command+=*p;}
    command+="'";Tab5Tools.open(Tab5Tool::Terminal);return Tab5Terminal::submit(command.c_str());
  };
  lv_obj_remove_flag(fileGitHost,LV_OBJ_FLAG_HIDDEN);
  if(!Tab5GitView::open(fileGitHost,path.c_str(),hooks,start,!creation&&!selectedIsDir?selectedPath.c_str():nullptr)){
    if(!Tab5GitView::scope())lv_obj_add_flag(fileGitHost,LV_OBJ_FLAG_HIDDEN);
    toast("Impossible d'ouvrir le projet Git.");
  }
}
static bool fileCanEditImage(){
  return selectedPath.length()&&!selectedIsDir&&
      (selectedIsImage||Tab5ImageFormats::supported(selectedPath.c_str()));
}
static void openSelectedWith(lv_obj_t* anchor);
static bool showFileMenu(FileMenu menu,int x,int y);
static int fileMenuX=0,fileMenuY=0;
static void fileMenuCommand(void* data) {
  const auto command=static_cast<FileCommand>(uintptr_t(data));
  const bool busy=Tab5GitView::busy()||Tab5Git::busy()||Tab5FileOperation::busy()||filePasteBatch.active||bool(fileSelectAllDirectory)||fileSelectAllSearch;
  if(busy&&command!=FileCommand::Info&&command!=FileCommand::ShowHidden)return;
  switch(command){
    case FileCommand::Open:selectedOpenEvent(nullptr);break;
    case FileCommand::Read:selectedReadEvent(nullptr);break;
    case FileCommand::Execute:selectedExecuteEvent(nullptr);break;
    case FileCommand::OpenWith:openSelectedWith(fileOpenWithBtn);break;
    case FileCommand::OpenWithOthers:if(selectedPath.length()&&!selectedIsDir){setKeyboardVisible(false);Tab5Tools.openWith(selectedPath.c_str());}break;
    case FileCommand::ViewImage:if(fileCanEditImage())startImageViewer(selectedPath);break;
    case FileCommand::Copy:copySelectedEvent(nullptr);break;
    case FileCommand::Cut:cutSelectedEvent(nullptr);break;
    case FileCommand::Paste:pasteClipboardEvent(nullptr);break;
    case FileCommand::Rename:renameSelectedEvent(nullptr);break;
    case FileCommand::Delete:deleteDocEvent(nullptr);break;
    case FileCommand::Info:showFileInfoEvent(nullptr);break;
    case FileCommand::Print:if(selectedPath.length()&&!selectedIsDir){setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Print,selectedPath.c_str());}break;
    case FileCommand::UnlockVault:if(!Tab5VaultPrompt::open())toast("Opération en cours : patienter ou annuler");break;
    case FileCommand::EditImage:if(fileCanEditImage())openImageEditor(selectedPath.c_str());break;
    case FileCommand::Vault:if(selectedPath.length()&&!selectedIsDir){setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Vault,selectedPath.c_str());}break;
    case FileCommand::Hide:toggleSelectedHidden(nullptr);break;
    case FileCommand::SelectAll:selectAllFiles();break;
    case FileCommand::MultiSelect:
      fileMultiSelect=!fileMultiSelect;
      if(!fileMultiSelect){fileSelection.clear();if(selectedPath.length()){fileSelection.add(selectedPath.c_str());previewSelectedFile();}}
      updateFileSelectionUi();break;
    case FileCommand::ShowHidden:Tab5Files::showHidden=!Tab5Files::showHidden;markSettingsDirty();refreshFileList();break;
    case FileCommand::Refresh:refreshFileList();break;
    case FileCommand::NewDocument:newDocEvent(nullptr);break;
    case FileCommand::NewFolder:newFolderEvent(nullptr);break;
    case FileCommand::NewGit:openFileGit(Tab5GitView::Start::Create);break;
    case FileCommand::ImportGit:openFileGit(Tab5GitView::Start::Import);break;
    case FileCommand::InitGit:openFileGit(Tab5GitView::Start::Initialize);break;
    case FileCommand::Git:openFileGit();break;
    case FileCommand::GitMenu:showFileMenu(FileMenu::Git,fileMenuX,fileMenuY);break;
    case FileCommand::Terminal:{
      if(Tab5Terminal::busy()){toast("Le terminal est occupé.");break;}
      const String directory=fileContextDirectory();
      String cmd="cd '";for(size_t i=0;i<directory.length();++i){if(directory[i]=='\'')cmd+="'\\''";else cmd+=directory[i];}
      cmd+="'";setKeyboardVisible(false);Tab5Tools.open(Tab5Tool::Terminal);Tab5Terminal::submit(cmd.c_str());break;
    }
    case FileCommand::Search:fileSearchEvent(nullptr);break;
    case FileCommand::ClearSearch:fileClearSearch();break;
    case FileCommand::SortNameAsc:case FileCommand::SortNameDesc:case FileCommand::SortDateAsc:case FileCommand::SortDateDesc:case FileCommand::SortSizeAsc:case FileCommand::SortSizeDesc:
      Tab5FileSort::selected=static_cast<Tab5FileSort::Order>(unsigned(command)-unsigned(FileCommand::SortNameAsc));refreshFileList();break;
  }
}
static bool showFileMenu(FileMenu menu,int x,int y) {
  fileMenuX=x;fileMenuY=y;
  Tab5ContextMenu::Item items[Tab5ContextMenu::Capacity];size_t count=0;
  const bool any=!fileSelection.paths.empty()||(!fileMultiSelect&&selectedPath.length());
  const bool one=any&&selectedPath.length()&&fileSelection.paths.size()<=1;
  const bool busy=Tab5GitView::busy()||Tab5Git::busy()||Tab5FileOperation::busy()||filePasteBatch.active||bool(fileSelectAllDirectory)||fileSelectAllSearch;
  const bool volume=selectedPath=="/"||selectedPath=="/USB"||
      std::any_of(fileSelection.paths.begin(),fileSelection.paths.end(),[](const std::string& path){return path=="/"||path=="/USB";});
  const bool editable=any&&!volume&&!busy;
  const bool folder=one&&selectedIsDir;
  const bool gitEligible=folder&&pathStorageReady(selectedPath)&&!busy;
  const bool repo=folder&&(menu==FileMenu::Git||menu==FileMenu::Actions||menu==FileMenu::Context)&&selectedFolderIsGit();
  auto add=[&](FileCommand command,const char* title,const char* shortcut="",bool enabled=true){
    if(count<Tab5ContextMenu::Capacity)items[count++]={T(title),shortcut,fileMenuCommand,reinterpret_cast<void*>(uintptr_t(command)),enabled};
  };
  if(menu==FileMenu::ImageOpenWith&&one&&fileCanEditImage()){
    add(FileCommand::ViewImage,"Visualisateur","",!busy);
    add(FileCommand::EditImage,"Dessin","",!busy);
    add(FileCommand::OpenWithOthers,"Autres applications…","",!busy);
  }
  if(menu==FileMenu::Create||menu==FileMenu::Context&&!any){
    add(FileCommand::NewDocument,"Document","",!busy&&pathStorageReady(fileContextDirectory()));
    add(FileCommand::NewFolder,"Nouveau dossier","Ctrl+Maj+N",!busy&&pathStorageReady(fileContextDirectory()));
  }
  if((menu==FileMenu::Actions||menu==FileMenu::Context)&&one){
    if(fileCanEditImage()){
      add(FileCommand::ViewImage,"Ouvrir avec le visualisateur","",!busy);
      add(FileCommand::EditImage,"Ouvrir avec Dessin","",!busy);
    }else if(selectedIsDir||selectedIsText||selectedIsAudio||selectedIsVideo)add(FileCommand::Open,"Ouvrir","Entrée",!busy);
    if(!selectedIsDir&&(selectedIsText||isEpubExtension(selectedPath)))add(FileCommand::Read,"Lire","",!busy);
    if(!selectedIsDir&&(isPythonExtension(selectedPath)||isShellExtension(selectedPath)||isExecutableExtension(selectedPath)||isFirmwareExtension(selectedPath)))add(FileCommand::Execute,isShellExtension(selectedPath)?"Exécuter le script":isFirmwareExtension(selectedPath)?"Installer un firmware":"Ouvrir l'application","",!busy);
    if(!selectedIsDir)add(FileCommand::OpenWith,"Ouvrir avec…","",!busy);
  }
  if(menu==FileMenu::Context||menu==FileMenu::Edit){
    if(any||menu==FileMenu::Edit){
      add(FileCommand::Copy,"Copier","Ctrl+C",editable);
      add(FileCommand::Cut,"Couper","Ctrl+X",editable);
    }
    if(menu==FileMenu::Edit||!fileClipboardPaths.empty()||clipboardPath.length())
      add(FileCommand::Paste,"Coller","Ctrl+V",(!fileClipboardPaths.empty()||clipboardPath.length())&&!busy&&pathStorageReady(fileContextDirectory()));
    if(any||menu==FileMenu::Edit){
      add(FileCommand::Rename,"Renommer","F2",one&&editable);
      add(FileCommand::Delete,"Supprimer","Suppr",editable);
      add(FileCommand::Hide,"Masquer ou afficher la sélection","Ctrl+Maj+H",editable);
    }
    if(menu==FileMenu::Edit){
      add(FileCommand::SelectAll,"Tout sélectionner","Ctrl+A",!busy);
      add(FileCommand::MultiSelect,fileMultiSelect?"Quitter la sélection multiple":"Sélection multiple","",!busy);
    }
  }
  if((menu==FileMenu::Actions||menu==FileMenu::Context)&&one){
    if(!selectedIsDir){
      add(FileCommand::Print,"Imprimer","Ctrl+P",!busy);
      add(FileCommand::Vault,"Chiffrer / déchiffrer le fichier","",!busy);
    }
    add(FileCommand::Info,"Informations");
  }
  // One Git entry leads to the selected folder's operations. Files and multiple
  // selections never silently redirect Git to an inferred parent/current path.
  if((menu==FileMenu::Actions||menu==FileMenu::Context)&&folder)
    add(FileCommand::GitMenu,"Git",">",gitEligible);
  if(menu==FileMenu::Git&&folder){
    if(repo)add(FileCommand::Git,"Git : état et commandes","",gitEligible);
    else{
      add(FileCommand::InitGit,"Initialiser ce dossier avec Git","",gitEligible);
      add(FileCommand::NewGit,"Créer un dossier Git","",gitEligible);
      add(FileCommand::ImportGit,"Cloner un dépôt Git","",gitEligible);
    }
  }
  if(menu==FileMenu::Actions){
    add(FileCommand::UnlockVault,"Déverrouiller le coffre","",!busy);
  }
  if(menu==FileMenu::View){
    add(FileCommand::ShowHidden,Tab5Files::showHidden?"Ne plus afficher les fichiers cachés":"Afficher les fichiers cachés","Ctrl+H",!busy);
    add(FileCommand::Refresh,"Actualiser","F5",!busy);
    if(fileSearchQuery.length())add(FileCommand::ClearSearch,"Effacer la recherche","",!busy);
  }
  if(menu==FileMenu::Sort)for(unsigned i=0;i<6;++i)add(static_cast<FileCommand>(unsigned(FileCommand::SortNameAsc)+i),Tab5FileSort::caption(i),"",!busy);
  if(menu==FileMenu::Actions||menu==FileMenu::Context)
    add(FileCommand::Terminal,"Ouvrir le dossier courant dans le terminal","",!busy&&pathStorageReady(fileContextDirectory()));
  if(!count)return false;
  return Tab5ContextMenu::show(filesPage,x,y,items,count);
}
static void openSelectedWith(lv_obj_t* anchor){
  if(!selectedPath.length()||selectedIsDir||fileSelection.paths.size()>1)return;
  if(!fileCanEditImage()){setKeyboardVisible(false);Tab5Tools.openWith(selectedPath.c_str());return;}
  if(!anchor)anchor=fileOpenWithBtn?fileOpenWithBtn:filesPage;
  lv_area_t bounds;lv_obj_get_coords(anchor,&bounds);
  showFileMenu(FileMenu::ImageOpenWith,bounds.x1,bounds.y2+4);
}
static void fileMenuEvent(lv_event_t* e){
  auto* anchor=lv_event_get_current_target_obj(e);lv_area_t bounds;lv_obj_get_coords(anchor,&bounds);
  showFileMenu(static_cast<FileMenu>(uintptr_t(lv_event_get_user_data(e))),bounds.x1,bounds.y2+4);
}
static void showFileCreateMenu(lv_event_t* e){
  if(!e)return;
  auto* anchor=lv_event_get_current_target_obj(e);lv_area_t bounds;lv_obj_get_coords(anchor,&bounds);
  showFileMenu(FileMenu::Create,bounds.x1,bounds.y2+4);
}
static bool fileContextMenuAt(lv_obj_t* hit,int x,int y){
  if(activePage!=Page::Files||!filesPage||!Tab5Focus.contains(filesPage,hit)||Tab5Tools.scope()||Tab5GitView::modalScope()||dialogOverlay||filePopupOverlay||fileDeleteOverlay||fileOperationOverlay)return false;
  auto found=std::find_if(fileSelectionRows.begin(),fileSelectionRows.end(),[hit](const FileSelectionRow& row){return Tab5Focus.contains(row.object,hit);});
  if(found!=fileSelectionRows.end()){
    const FileSelectionRow row=*found;
    if(fileActiveRight!=row.right||!fileSelection.contains(row.path))fileSelectEntry(row.path.c_str(),row.directory,row.object,row.right);
    else{selectedPath=row.path.c_str();selectedIsDir=row.directory;selectedFileBtn=row.object;previewSelectedFile();updateFileSelectionUi();}
    Tab5Focus.focusObject(row.object);
  }else if(fileList&&Tab5Focus.contains(fileList,hit)){
    fileActiveRight=false;clearFilePreview();
  }else if(folderPreviewHost&&Tab5Focus.contains(folderPreviewHost,hit)){
    fileActiveRight=true;clearFilePreview();
  }
  return showFileMenu(FileMenu::Context,x,y);
}
