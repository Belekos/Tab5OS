#include "WriterStorage.h"
#include <cstdio>
#include <cstring>
#include <algorithm>
namespace writing {
bool backupPath(const char* source,char* destination,size_t capacity){
 if(!source||source[0]!='/')return false;
 bool usb=!strncmp(source,"/USB/",5);int n=snprintf(destination,capacity,"%s/.tab5-backups%s.writer.bak",usb?"/USB":"",usb?source+4:source);return n>0&&size_t(n)<capacity;
}
static uint32_t checksum(uint32_t h, const char* p, size_t n) {
  for (size_t i = 0; i < n; ++i) { h ^= static_cast<uint8_t>(p[i]); h *= 16777619u; }
  return h;
}
SaveJob::~SaveJob() { if (busy()) fs_.close(); release(snapshot_); }
void SaveJob::fail(const char* e) {
  fs_.close(); state_ = State::Error; error_ = e;
  release(snapshot_); snapshot_ = nullptr; document_ = nullptr;
}
void SaveJob::cancel() {
  if (busy()) fail("Sauvegarde interrompue");
}
bool SaveJob::begin(Document& doc, const char* path, bool markDocumentSaved) {
  if (busy()) return false;
  if (!path || !*path || strlen(path) >= sizeof(path_)) return false;
  release(snapshot_); snapshot_ = doc.snapshot();
  if (!snapshot_) { fail("PSRAM insuffisante pour sauvegarder"); return false; }
  strcpy(path_, path); snprintf(temp_, sizeof(temp_), "%s.writer.tmp", path);
  if(!backupPath(path,backup_,sizeof(backup_))){fail("Dossier de sauvegarde inaccessible");return false;}
  sourceLength_ = doc.size(); length_ = offset_ = 0; revision_ = doc.revision(); document_ = markDocumentSaved ? &doc : nullptr;
  encoding_ = doc.encoding(); encoder_.reset(snapshot_,sourceLength_,encoding_);
  expected_ = observed_ = 2166136261u; error_ = "";
  state_ = State::Preparing; return true;
}

unsigned SaveJob::progress() const {
  if (!busy()) return state_ == State::Done ? 100 : 0;
  if(state_==State::Preparing)return sourceLength_?unsigned(encoder_.offset()*20/sourceLength_):20;
  return (state_ == State::Verifying ? 60u : 20u) + (length_ ? static_cast<unsigned>(offset_ * 40 / length_) : 40u);
}
void SaveJob::step() {
  if (state_ == State::Preparing) {
    char block[4096];size_t n=0;
    if(!encoder_.next(block,sizeof(block),n)){fail("Caractère non représentable dans cet encodage");return;}
    length_+=n;
    if(!encoder_.done())return;
    // Validate every codepoint before touching any on-disk temporary or backup.
    if(!fs_.prepareBackup(backup_)){fail("Dossier de sauvegarde inaccessible");return;}
    bool present=false;if(!fs_.probe(temp_,present)||(present&&!fs_.remove(temp_))){fail("Fichier temporaire inaccessible");return;}
    if(!fs_.openWrite(temp_)){fail("Impossible de créer le fichier temporaire");return;}
    encoder_.reset(snapshot_,sourceLength_,encoding_);state_=State::Writing;return;
  }
  if (state_ == State::Writing) {
    char block[4096];size_t n=0;
    if(!encoder_.next(block,sizeof(block),n)){fail("Conversion du texte impossible");return;}
    if (n && fs_.write(block, n) != n) { fail("Écriture SD incomplète"); return; }
    expected_ = checksum(expected_, block, n); offset_ += n;
    if (offset_ == length_) {
      if(!fs_.finishWrite()){fail("Écriture SD incomplète");return;}
      if (!fs_.openRead(temp_) || fs_.length() != length_) { fail("Taille de sauvegarde incorrecte"); return; }
      state_ = State::Verifying; offset_ = 0;
    }
    return;
  }
  if (state_ != State::Verifying) return;
  char block[4096];
  const size_t n = std::min<size_t>(sizeof(block), length_ - offset_);
  if (n && fs_.read(block, n) != n) { fail("Relecture SD incomplète"); return; }
  observed_ = checksum(observed_, block, n); offset_ += n;
  if (offset_ != length_) return;
  fs_.close();
  if (observed_ != expected_) { fail("Vérification SD échouée"); return; }
  bool original=false;if(!fs_.probe(path_,original)){fail("Impossible de préserver l'original");return;}
  // If power failed after old->backup, preserve that backup until the new
  // primary is committed. Never delete the only surviving previous version.
  if (original) {
    bool present=false;if (!fs_.probe(backup_,present)||(present&&!fs_.remove(backup_))) { fail("Copie précédente inaccessible"); return; }
    if (!fs_.rename(path_, backup_)) { fail("Impossible de préserver l'original"); return; }
  }
  if (!fs_.rename(temp_, path_)) {
    if (original) fs_.rename(backup_, path_);
    fail("Validation finale impossible ; original conservé"); return;
  }
  if(document_)document_->markSaved(revision_,encoding_);
  document_ = nullptr;
  release(snapshot_); snapshot_ = nullptr; state_ = State::Done;
}
void LoadJob::cancel() {
  if (busy()) fs_.close();
  Document empty; next_.swap(empty);
  destination_ = nullptr; state_ = State::Idle;
}
void LoadJob::fail(const char* error) { cancel(); error_ = error; state_ = State::Error; }
bool LoadJob::begin(Document& doc, const char* path, size_t limit, TextEncoding encoding) {
  cancel(); recovered_ = false; offset_ = length_ = 0; error_ = ""; decoder_.reset(encoding);
  decodedLimit_=std::min(limit,Document::Limit);
  char backup[320];
  if (!path || strlen(path) >= 300) { fail(); return false; }
  if(!backupPath(path,backup,sizeof(backup))){fail();return false;}
  bool present=false;if(!fs_.probe(path,present)){fail();return false;}
  const char* source = path;
  if (!present) {
    if(!fs_.probe(backup,present)){fail();return false;}
    if(!present)snprintf(backup,sizeof(backup),"%s.writer.bak",path);
    source = backup; recovered_ = true;
  }
  if (!fs_.openRead(source)) { fail(); return false; }
  state_ = State::Reading;
  length_ = fs_.length();
  if (length_ > 2*decodedLimit_+4) { fail(); return false; }
  destination_ = &doc;
  return true;
}
void LoadJob::step() {
  if (!busy()) return;
  char data[1024];
  const size_t wanted = std::min<size_t>(sizeof(data), length_ - offset_);
  if (wanted && fs_.read(data, wanted) != wanted) { fail(); return; }
  offset_ += wanted;
  auto sink=[](const char* bytes,size_t count,void* context){auto* job=static_cast<LoadJob*>(context);return count<=job->decodedLimit_-job->next_.size()&&job->next_.appendLoading(bytes,count);};
  if(!decoder_.feed(data,wanted,offset_==length_,sink,this)){
    fail(decoder_.error()==CodecError::OutputRejected?"Ouverture impossible : stockage ou limite de taille":"Encodage invalide : choisissez l'encodage du fichier");return;
  }
  if (offset_ == length_) {
    if (!next_.finishLoading()) { fail(); return; }
    next_.setEncoding(decoder_.encoding());next_.markSaved(next_.revision());
    fs_.close();
    destination_->swap(next_);
    destination_ = nullptr; state_ = State::Done;
    Document empty; next_.swap(empty);
  }
}

bool loadDocument(FileSystem& fs, const char* path, Document& doc, bool& recovered, void (*yieldFn)(), TextEncoding encoding, CodecError* codecError) {
  LoadJob job(fs);
  if (!job.begin(doc, path,Document::Limit,encoding)) { recovered = job.recovered(); if(codecError)*codecError=job.codecError(); return false; }
  while (job.busy()) { job.step(); if (yieldFn) yieldFn(); }
  recovered = job.recovered();if(codecError)*codecError=job.codecError();
  return job.state() == LoadJob::State::Done;
}
} // namespace writing
