#include "WriterEncoding.h"
#include <cstring>
namespace writing {
namespace {
constexpr uint16_t cp1252[32] = {0x20ac,0,0x201a,0x0192,0x201e,0x2026,0x2020,0x2021,0x02c6,0x2030,0x0160,0x2039,0x0152,0,0x017d,0,0,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x02dc,0x2122,0x0161,0x203a,0x0153,0,0x017e,0x0178};
bool scalar(uint32_t p) { return p && p <= 0x10ffff && (p < 0xd800 || p > 0xdfff); }
size_t utf8(uint32_t p,char* b) {
 if(p<0x80){b[0]=char(p);return 1;}
 if(p<0x800){b[0]=char(0xc0|(p>>6));b[1]=char(0x80|(p&63));return 2;}
 if(p<0x10000){b[0]=char(0xe0|(p>>12));b[1]=char(0x80|((p>>6)&63));b[2]=char(0x80|(p&63));return 3;}
 b[0]=char(0xf0|(p>>18));b[1]=char(0x80|((p>>12)&63));b[2]=char(0x80|((p>>6)&63));b[3]=char(0x80|(p&63));return 4;
}
bool readPoint(const char* s,size_t length,size_t& at,uint32_t& p){
 if(at>=length)return false;
 uint8_t c=uint8_t(s[at++]);unsigned n=0;uint32_t minimum=0;
 if(c<128)p=c;
 else if(c>=0xc2&&c<=0xdf){p=c&31;n=1;minimum=0x80;}
 else if(c>=0xe0&&c<=0xef){p=c&15;n=2;minimum=0x800;}
 else if(c>=0xf0&&c<=0xf4){p=c&7;n=3;minimum=0x10000;}
 else return false;
 if(length-at<n)return false;
 while(n--){c=uint8_t(s[at++]);if((c&0xc0)!=0x80)return false;p=(p<<6)|(c&63);}
 return p>=minimum&&scalar(p);
}
}
const char* encodingName(TextEncoding e){switch(e){case TextEncoding::Auto:return "Auto";case TextEncoding::Utf8:return "UTF-8";case TextEncoding::Utf8Bom:return "UTF-8 BOM";case TextEncoding::Utf16LE:return "UTF-16 LE";case TextEncoding::Utf16BE:return "UTF-16 BE";case TextEncoding::Windows1252:return "Windows-1252";}return "UTF-8";}
void TextDecoder::reset(TextEncoding e){*this=TextDecoder{};requested_=e;}
bool TextDecoder::flush(Sink sink,void* context){if(outputSize_&&!sink(output_,outputSize_,context))return fail(CodecError::OutputRejected);outputSize_=0;return true;}
bool TextDecoder::emit(uint32_t p,Sink sink,void* context){
 if(!scalar(p))return fail(CodecError::InvalidSequence);
 if(sizeof(output_)-outputSize_<4&&!flush(sink,context))return false;
 outputSize_+=utf8(p,output_+outputSize_);return true;
}
bool TextDecoder::byte(uint8_t c,Sink sink,void* context){
 if(encoding_==TextEncoding::Windows1252){uint32_t p=c>=0x80&&c<0xa0?cp1252[c-0x80]:c;return emit(p,sink,context);}
 if(encoding_==TextEncoding::Utf16LE||encoding_==TextEncoding::Utf16BE){
  if(!haveByte_){byte_=c;haveByte_=true;return true;}
  uint16_t w=encoding_==TextEncoding::Utf16LE?uint16_t(byte_|uint16_t(c)<<8):uint16_t(uint16_t(byte_)<<8|c);haveByte_=false;
  if(high_){if(w<0xdc00||w>0xdfff)return fail(CodecError::InvalidSequence);uint32_t p=0x10000+(uint32_t(high_-0xd800)<<10)+(w-0xdc00);high_=0;return emit(p,sink,context);}
  if(w>=0xd800&&w<=0xdbff){high_=w;return true;}return emit(w,sink,context);
 }
 if(remaining_){if((c&0xc0)!=0x80)return fail(CodecError::InvalidSequence);point_=(point_<<6)|(c&63);if(--remaining_)return true;if(point_<minimum_)return fail(CodecError::InvalidSequence);return emit(point_,sink,context);}
 if(c<128)return emit(c,sink,context);
 if(c>=0xc2&&c<=0xdf){point_=c&31;remaining_=1;minimum_=0x80;return true;}
 if(c>=0xe0&&c<=0xef){point_=c&15;remaining_=2;minimum_=0x800;return true;}
 if(c>=0xf0&&c<=0xf4){point_=c&7;remaining_=3;minimum_=0x10000;return true;}
 return fail(CodecError::InvalidSequence);
}
bool TextDecoder::initialize(Sink sink,void* context){
 size_t skip=0;TextEncoding found=TextEncoding::Auto;
 if(prefixSize_>=4&&((prefix_[0]==0xff&&prefix_[1]==0xfe&&!prefix_[2]&&!prefix_[3])||(!prefix_[0]&&!prefix_[1]&&prefix_[2]==0xfe&&prefix_[3]==0xff)))return fail(CodecError::Unsupported);
 if(prefixSize_>=3&&prefix_[0]==0xef&&prefix_[1]==0xbb&&prefix_[2]==0xbf){found=TextEncoding::Utf8Bom;skip=3;}
 else if(prefixSize_>=2&&prefix_[0]==0xff&&prefix_[1]==0xfe){found=TextEncoding::Utf16LE;skip=2;}
 else if(prefixSize_>=2&&prefix_[0]==0xfe&&prefix_[1]==0xff){found=TextEncoding::Utf16BE;skip=2;}
 encoding_=requested_==TextEncoding::Auto?(found==TextEncoding::Auto?TextEncoding::Utf8:found):requested_;
 if(requested_==TextEncoding::Windows1252)skip=0;
 else if(found!=TextEncoding::Auto&&found!=encoding_){if(encoding_==TextEncoding::Utf8&&found==TextEncoding::Utf8Bom)encoding_=found;else return fail(CodecError::InvalidSequence);}
 for(size_t i=skip;i<prefixSize_;++i)if(!byte(prefix_[i],sink,context))return false;
 return true;
}
bool TextDecoder::feed(const char* data,size_t n,bool final,Sink sink,void* context){
 if(error_!=CodecError::None||!sink||(!data&&n))return false;
 size_t i=0;if(encoding_==TextEncoding::Auto){while(i<n&&prefixSize_<4)prefix_[prefixSize_++]=uint8_t(data[i++]);if(prefixSize_<4&&!final)return true;if(!initialize(sink,context))return false;}
 for(;i<n;++i)if(!byte(uint8_t(data[i]),sink,context))return false;
 if(final&&(remaining_||haveByte_||high_))return fail(CodecError::InvalidSequence);
 return flush(sink,context);
}
void TextEncoder::reset(const char* s,size_t n,TextEncoding e){input_=s;length_=n;offset_=0;encoding_=e==TextEncoding::Auto?TextEncoding::Utf8:e;error_=CodecError::None;bomWritten_=false;}
bool TextEncoder::next(char* out,size_t cap,size_t& written){
 written=0;if(error_!=CodecError::None||!out||cap<4||(!input_&&length_))return false;
 if(!bomWritten_){if(encoding_==TextEncoding::Utf8Bom){out[written++]=char(0xef);out[written++]=char(0xbb);out[written++]=char(0xbf);}else if(encoding_==TextEncoding::Utf16LE){out[written++]=char(0xff);out[written++]=char(0xfe);}else if(encoding_==TextEncoding::Utf16BE){out[written++]=char(0xfe);out[written++]=char(0xff);}bomWritten_=true;}
 while(offset_<length_&&cap-written>=4){size_t nextOffset=offset_;uint32_t p;if(!readPoint(input_,length_,nextOffset,p)){error_=CodecError::InvalidSequence;return false;}
  if(encoding_==TextEncoding::Windows1252){unsigned b=256;if(p<0x80||(p>=0xa0&&p<=0xff))b=p;else for(unsigned i=0;i<32;++i)if(cp1252[i]==p){b=0x80+i;break;}if(b>255){error_=CodecError::Unrepresentable;return false;}out[written++]=char(b);}
  else if(encoding_==TextEncoding::Utf16LE||encoding_==TextEncoding::Utf16BE){uint16_t words[2];size_t count=1;if(p>=0x10000){p-=0x10000;words[0]=0xd800+(p>>10);words[1]=0xdc00+(p&1023);count=2;}else words[0]=uint16_t(p);for(size_t i=0;i<count;++i){out[written++]=char(encoding_==TextEncoding::Utf16LE?words[i]&255:words[i]>>8);out[written++]=char(encoding_==TextEncoding::Utf16LE?words[i]>>8:words[i]&255);}}
  else written+=utf8(p,out+written);
  offset_=nextOffset;
 }
 return true;
}
}
