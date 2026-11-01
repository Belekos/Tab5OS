#pragma once
#include "WriterSyntax.h"
namespace writing {
// At most one small lexical checkpoint per 4 KiB, stored in PSRAM. A far jump
// displays plain code until background scanning reaches the requested page;
// it never guesses that a block comment/string ended at a page boundary.
class SyntaxIndex {
 public:
  SyntaxIndex()=default;
  ~SyntaxIndex();
  SyntaxIndex(const SyntaxIndex&)=delete;
  SyntaxIndex& operator=(const SyntaxIndex&)=delete;
  void reset();
  void edited(const Document&,size_t offset);
  void step(const Document&,SyntaxLanguage,size_t budget=4096);
  bool stateAt(const Document&,SyntaxLanguage,size_t offset,SyntaxState&);
  bool failed() const { return failed_; }
  size_t scannedBytes() const { return scanned_; }
  size_t bytes() const;
 private:
  struct Checkpoint {size_t offset=0;SyntaxState state;};
  static constexpr size_t Chunk=4096,Capacity=514;
  Checkpoint* checkpoints_=nullptr;
  size_t used_=0,scanned_=0;
  uint64_t revision_=UINT64_MAX;
  SyntaxLanguage language_=SyntaxLanguage::Auto;
  bool failed_=false;
  void prepare(const Document&,SyntaxLanguage);
};
}
