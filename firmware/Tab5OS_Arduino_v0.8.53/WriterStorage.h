#pragma once
#include "WriterDocument.h"

namespace writing {
// One opened handle at a time. The Arduino adapter owns the SD File object.
class FileSystem {
 public:
  virtual ~FileSystem() = default;
  virtual bool prepareBackup(const char*) {return true;}
  virtual bool exists(const char*) = 0;
  // false means presence is unknown (I/O error), not that the file is absent.
  // In-memory adapters may retain the default; storage adapters must override.
  virtual bool probe(const char* path, bool& present) { present=exists(path);return true; }
  virtual bool remove(const char*) = 0;
  virtual bool rename(const char*, const char*) = 0;
  virtual bool openWrite(const char*) = 0;
  virtual bool openRead(const char*) = 0;
  virtual size_t length() = 0;
  virtual size_t write(const char*, size_t) = 0;
  virtual size_t read(char*, size_t) = 0;
  virtual void close() = 0; // flush when writing
  // Storage adapters report synchronization errors before a temporary can
  // be promoted. Memory-only adapters retain their ordinary close behavior.
  virtual bool finishWrite() { close();return true; }
};

class SaveJob {
 public:
  enum class State { Idle, Preparing, Writing, Verifying, Done, Error };
  explicit SaveJob(FileSystem& fs) : fs_(fs) {}
  ~SaveJob();
  bool begin(Document& doc, const char* path, bool markDocumentSaved = true);
  void cancel(); // Close a pending temporary copy without replacing its target.
  void step(); // <= 4096 bytes per pass, then a short rename transaction
  bool busy() const { return state_ == State::Preparing || state_ == State::Writing || state_ == State::Verifying; }
  State state() const { return state_; }
  const char* error() const { return error_; }
  unsigned progress() const;
 private:
  FileSystem& fs_;
  Document* document_ = nullptr;
  char* snapshot_ = nullptr;
  char path_[300] = {}, temp_[320] = {}, backup_[320] = {};
  size_t length_ = 0, offset_ = 0, sourceLength_ = 0;
  TextEncoder encoder_;
  TextEncoding encoding_ = TextEncoding::Utf8;
  uint64_t revision_ = 0;
  uint32_t expected_ = 2166136261u, observed_ = 2166136261u;
  State state_ = State::Idle;
  const char* error_ = "";
  void fail(const char* error);
};
class LoadJob {
 public:
  enum class State { Idle, Reading, Done, Error };
  explicit LoadJob(FileSystem& fs) : fs_(fs) {}
  ~LoadJob() { cancel(); }
  bool begin(Document& destination, const char* path, size_t limit = Document::Limit, TextEncoding encoding = TextEncoding::Auto);
  void step(); // Read at most 4096 bytes. Destination changes only on success.
  void cancel();
  bool busy() const { return state_ == State::Reading; }
  State state() const { return state_; }
  bool recovered() const { return recovered_; }
  CodecError codecError() const { return decoder_.error(); }
  const char* error() const { return error_; }
  unsigned progress() const { return length_ ? unsigned(offset_ * 100 / length_) : 100; }
 private:
  FileSystem& fs_;
  Document next_;
  Document* destination_ = nullptr;
  State state_ = State::Idle;
  size_t length_ = 0, offset_ = 0;
  bool recovered_ = false;
  size_t decodedLimit_ = Document::Limit;
  TextDecoder decoder_;
  const char* error_ = "";
  void fail(const char* error = "Ouverture impossible : stockage ou limite de taille");
};
bool backupPath(const char* source,char* destination,size_t capacity);
bool loadDocument(FileSystem& fs, const char* path, Document& destination,
                  bool& recovered, void (*yieldFn)() = nullptr,
                  TextEncoding encoding = TextEncoding::Auto, CodecError* codecError = nullptr);
} // namespace writing
