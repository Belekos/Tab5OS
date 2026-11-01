static bool probeSdWrite() {
  String path;
  for(unsigned n=0;n<32;++n){
    path=String("/.tab5-sd-check-")+millis()+"-"+n;
    bool present=false;
    if(!tab5AtomicProbe(path.c_str(),present)){
      tab5StorageFault("verification SD",path.c_str(),errno?errno:EIO);return false;
    }
    if(!present)break;
    path="";
  }
  if(!path.length()){tab5StorageFault("verification SD","/",EEXIST);return false;}
  static const uint8_t bytes[]={0x54,0x35,0x53,0x44};
  errno=0;File output=tab5FS.open(path,FILE_WRITE);
  if(!output){tab5StorageFault("verification ecriture",path.c_str(),errno?errno:EIO);return false;}
  errno=0;
  int error=output.write(bytes,sizeof(bytes))==sizeof(bytes)?0:(errno?errno:EIO);
  errno=0;output.flush();if(errno&&!error)error=errno;
  if(!error){
    errno=0;const size_t stored=output.size();
    if(errno||stored!=sizeof(bytes)||output.getWriteError())error=errno?errno:EIO;
  }
  errno=0;output.close();if(errno&&!error)error=errno;
  if(error){
    tab5FS.remove(path);tab5StorageFault("verification ecriture",path.c_str(),error);return false;
  }
  errno=0;File input=tab5FS.open(path,FILE_READ);
  if(!input)error=errno?errno:EIO;
  uint8_t readback[sizeof(bytes)]{};
  if(!error){
    errno=0;const size_t stored=input.size();
    if(errno||stored!=sizeof(bytes))error=errno?errno:EIO;
  }
  if(!error){
    errno=0;const size_t received=input.read(readback,sizeof(readback));
    if(errno||received!=sizeof(readback)||memcmp(bytes,readback,sizeof(bytes)))error=errno?errno:EIO;
  }
  errno=0;input.close();if(errno&&!error)error=errno;
  errno=0;const bool removed=tab5FS.remove(path);const int removeError=errno;
  if(error)tab5StorageFault("verification lecture",path.c_str(),error);
  else if(!removed)tab5StorageFault("verification nettoyage",path.c_str(),removeError?removeError:EIO);
  return !error&&removed;
}

static void serviceSdRecovery(uint32_t now) {
  if(!tab5StorageRecoveryDue(now))return;
  Tab5SystemResources::cancel();
  // Release UI-owned iterators before attempting the exclusive lease.
  Tab5AppCatalogue::endScan();homeAppsScanning=false;homeAppsDirty=true;
  if(activePage==Page::Files&&!fileSessionRestorePending)rememberFileSession();
  fileTree.cancel();fileSearcher.cancel();fileListDir.close();cancelFolderPreview();
  fileSelectAllDirectory.close();fileSelectAllSearch=false;
  Tab5FilePicker::close();
  bool recovered=false;
  {
    Tab5StorageRecovery exclusive;
    if(!exclusive||Tab5ImageEditor::busy()||!Tab5Audio.storageIdle()||!Tab5Writing.storageIdle()||Tab5MapPrint::busy())return;
    Tab5Map.releaseStorageHandles();
    if(tab5SdOpenFiles()!=0)return;
    tab5DiagnosticMark(3,"reconnexion SD","",1);
    tab5SdSetAvailable(false);
    SD.end();SPI.end();
    pinMode(SD_SPI_CS_PIN,OUTPUT);digitalWrite(SD_SPI_CS_PIN,HIGH);
    SPI.begin(SD_SPI_SCK_PIN,SD_SPI_MISO_PIN,SD_SPI_MOSI_PIN,SD_SPI_CS_PIN);
    sdReady=SD.begin(SD_SPI_CS_PIN,SPI,10000000U,"/sd",24,false);
    tab5SdSetAvailable(sdReady);
    if(sdReady){File check=tab5FS.open("/",FILE_READ);sdReady=check&&check.isDirectory();check.close();}
    if(sdReady)sdReady=migrateSystemDirectory();
    tab5SdSetAvailable(sdReady);
    if(sdReady&&!reloadSettingsAfterMigration()){
      sdReady=false;tab5SdSetAvailable(false);
    }
    if(!sdReady)tab5StorageFault("reconnexion","/",ENODEV);
    recovered=sdReady&&probeSdWrite();
    const int error=tab5StorageLastError();
    const bool retry=tab5StorageTransportError(error);
    tab5StorageRecoveryDone(millis(),recovered,retry);
    // Setters only invalidate catalogues. No reload while the lease is exclusive.
    Tab5Audio.setStorageAvailable(sdReady);
    Tab5Video.setStorageAvailable(sdReady);
    Tab5Map.setStorageAvailable(sdReady);
  }
  if(sdReady){ensureDirectories();Tab5SystemResources::start(true);}
  tab5DiagnosticMark(3,recovered?"SD reconnectee":"echec SD",tab5StorageErrorText(),0);
  tab5StorageLog();
  toast(systemMigrationFailed?"Migration vers System incomplète : relancez la récupération SD":recovered?"SD reconnectee : lecture et ecriture verifiees - reessayez l'enregistrement":tab5StorageErrorText(),5000);
  if(sdReady&&activePage==Page::Files)refreshFileList();
}

static const char* storageSdAction(bool eject) {
  if(!eject){tab5StorageRequestRecovery();return "Reconnexion SD demandée";}
  if(!sdReady)return "Carte SD éjectée : vous pouvez la retirer";
  if(documentDirty||Tab5ImageEditor::dirty()||Tab5ImageEditor::busy()||!Tab5Writing.prepareSleep()){
    return "Enregistrez les documents et attendez la fin des écritures";
  }
  if(settingsDirty){if(!saveSettings())return "Enregistrement des réglages impossible";settingsDirty=false;}
  if(readerProgressDirty){saveReaderProgressNow();if(readerProgressDirty)return "Enregistrement de la lecture impossible";}
  Tab5SystemResources::cancel();
  Tab5AppCatalogue::endScan();homeAppsScanning=false;homeAppsDirty=true;
  if(activePage==Page::Files&&!fileSessionRestorePending)rememberFileSession();
  fileTree.cancel();fileSearcher.cancel();fileListDir.close();cancelFolderPreview();Tab5FilePicker::close();
  fileSelectAllDirectory.close();fileSelectAllSearch=false;
  {
    Tab5StorageRecovery exclusive;
    if(!exclusive||Tab5ImageEditor::busy()||!Tab5Audio.storageIdle()||!Tab5Writing.storageIdle()||Tab5MapPrint::busy()){
      return "Carte SD occupée";
    }
    Tab5Map.releaseStorageHandles();
    if(tab5SdOpenFiles()!=0)return "Fermez les applications utilisant la microSD";
    tab5SdSetAvailable(false);SD.end();SPI.end();sdReady=false;
    tab5StorageEjected();
    Tab5Audio.setStorageAvailable(false);Tab5Video.setStorageAvailable(false);Tab5Map.setStorageAvailable(false);
  }
  clearFilePreview();if(activePage==Page::Files)refreshFileList();
  return "Carte SD éjectée : vous pouvez la retirer";
}
