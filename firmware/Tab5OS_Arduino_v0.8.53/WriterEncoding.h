#pragma once
#include <stddef.h>
#include <stdint.h>
namespace writing {
enum class TextEncoding : uint8_t { Auto, Utf8, Utf8Bom, Utf16LE, Utf16BE, Windows1252 };
const char* encodingName(TextEncoding encoding);
enum class CodecError : uint8_t { None, InvalidSequence, Unsupported, Unrepresentable, OutputRejected };
// Streaming conversion with fixed working memory; no heap allocation.
class TextDecoder {
 public:
  using Sink = bool (*)(const char*, size_t, void*);
  void reset(TextEncoding requested = TextEncoding::Auto);
  bool feed(const char* bytes, size_t length, bool final, Sink sink, void* context);
  TextEncoding encoding() const { return encoding_; }
  CodecError error() const { return error_; }
 private:
  TextEncoding requested_ = TextEncoding::Auto, encoding_ = TextEncoding::Auto;
  CodecError error_ = CodecError::None;
  uint8_t prefix_[4]{}; size_t prefixSize_ = 0;
  char output_[512]{}; size_t outputSize_ = 0;
  uint32_t point_ = 0, minimum_ = 0; uint16_t high_ = 0;
  uint8_t remaining_ = 0, byte_ = 0; bool haveByte_ = false;
  bool initialize(Sink sink, void* context);
  bool byte(uint8_t value, Sink sink, void* context);
  bool emit(uint32_t point, Sink sink, void* context);
  bool flush(Sink sink, void* context);
  bool fail(CodecError error) { error_ = error; return false; }
};
class TextEncoder {
 public:
  void reset(const char* utf8, size_t length, TextEncoding encoding);
  // Writes complete codepoints only. Caller capacity must be at least four.
  bool next(char* bytes, size_t capacity, size_t& written);
  bool done() const { return offset_ == length_ && bomWritten_; }
  size_t offset() const { return offset_; }
  CodecError error() const { return error_; }
 private:
  const char* input_ = nullptr; size_t length_ = 0, offset_ = 0;
  TextEncoding encoding_ = TextEncoding::Utf8;
  CodecError error_ = CodecError::None; bool bomWritten_ = false;
};
}
