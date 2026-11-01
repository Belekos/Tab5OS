#include "WriterSyntaxIndex.h"
#include "WriterDocument.h"
#include <algorithm>
#include <new>
namespace writing {
SyntaxIndex::~SyntaxIndex(){release(checkpoints_);}
void SyntaxIndex::reset(){used_=scanned_=0;revision_=UINT64_MAX;language_=SyntaxLanguage::Auto;failed_=false;}
void SyntaxIndex::discard(){release(checkpoints_);checkpoints_=nullptr;reset();}
size_t SyntaxIndex::bytes()const{return checkpoints_?sizeof(Checkpoint)*Capacity:0;}
void SyntaxIndex::prepare(const Document& doc,SyntaxLanguage language){
 if(language_==language&&revision_==doc.revision())return;
 language_=language;revision_=doc.revision();used_=scanned_=0;failed_=false;
 if(language==SyntaxLanguage::Auto||language==SyntaxLanguage::Plain)return;
 if(!checkpoints_)checkpoints_=static_cast<Checkpoint*>(allocate(sizeof(Checkpoint)*Capacity));
 if(!checkpoints_){failed_=true;return;}
 new(checkpoints_) Checkpoint{};used_=1;
}
void SyntaxIndex::edited(const Document& doc,size_t offset){
 revision_=doc.revision();
 if(!checkpoints_||!used_)return;
 // A delimiter can inspect up to 18 bytes ahead (C++ raw-string tag).
 const size_t safe=offset>32?offset-32:0;
 while(used_>1&&checkpoints_[used_-1].offset>safe)--used_;
 scanned_=checkpoints_[used_-1].offset;
}
void SyntaxIndex::step(const Document& doc,SyntaxLanguage language,size_t budget){
 prepare(doc,language);
 if(!used_||scanned_>=doc.size()||!budget)return;
 // Checkpoints stay 4 KiB apart, keeping stateAt bounded. Smaller caller
 // budgets are supported without consuming another slot for every tiny step.
 const size_t next=std::min(doc.size(),checkpoints_[used_-1].offset+Chunk);
 const size_t end=std::min(next,scanned_+std::min(budget,Chunk));
 // The partial state is held in the next (unpublished) slot.
 if(scanned_==checkpoints_[used_-1].offset){if(used_>=Capacity){failed_=true;return;}new(checkpoints_+used_) Checkpoint{scanned_,checkpoints_[used_-1].state};}
 scanSyntax(doc,scanned_,end,language_,checkpoints_[used_].state);scanned_=end;checkpoints_[used_].offset=end;
 if(end==next)++used_;
}
bool SyntaxIndex::stateAt(const Document& doc,SyntaxLanguage language,size_t offset,SyntaxState& result){
 prepare(doc,language);offset=std::min(offset,doc.size());
 if(!offset||language==SyntaxLanguage::Plain){result={};return true;}
 if(!used_||offset>scanned_)return false;
 size_t a=0,b=used_;while(a<b){const size_t m=a+(b-a)/2;if(checkpoints_[m].offset<=offset)a=m+1;else b=m;}
 const auto& cp=checkpoints_[a?a-1:0];result=cp.state;scanSyntax(doc,cp.offset,offset,language,result);return true;
}
}
