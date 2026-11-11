#pragma once
#include <stddef.h>
#include <stdint.h>

// HTTPClient::getStreamPtr() exposes transport bytes, including HTTP chunk
// framing. Decode only that framing here; ICY metadata is a separate layer.
// Every read is nonblocking and allocation-free. The owner handles timeout,
// cancellation, connection closure and any cooperative scheduler yielding.
class Tab5RadioBodyReader {
 public:
  void reset(bool chunked) {
    _state = chunked ? State::Size : State::Plain;
    _remaining = _lineBytes = _trailerBytes = 0;
    _digits = _extension = _sizeWhitespace = false;
  }
  bool finished() const { return _state == State::Done; }
  bool failed() const { return _state == State::Error; }

  // Call only after the peer has closed AND its remaining bytes were drained.
  void endInput() {
    if (_state == State::Plain) _state = State::Done;
    else if (_state != State::Done) _state = State::Error;
  }

  template<class Client>
  uint32_t read(Client& stream, void* destination, uint32_t length) {
    if (!destination || !length || finished() || failed()) return 0;
    auto* output = static_cast<uint8_t*>(destination);
    uint32_t total = 0;
    while (total < length && !finished() && !failed()) {
      const int available = stream.available();
      if (available <= 0) break;
      if (_state == State::Plain || _state == State::Data) {
        uint32_t take = length - total;
        if (take > static_cast<uint32_t>(available)) take = static_cast<uint32_t>(available);
        if (_state == State::Data && take > _remaining) take = _remaining;
        const int got = stream.read(output + total, static_cast<size_t>(take));
        if (got <= 0) break;
        if (static_cast<uint32_t>(got) > take) { _state = State::Error; break; }
        total += static_cast<uint32_t>(got);
        if (_state == State::Data) {
          _remaining -= static_cast<uint32_t>(got);
          if (!_remaining) _state = State::DataCR;
        }
        continue;
      }
      const int value = stream.read();
      if (value < 0) break;
      consume(static_cast<uint8_t>(value));
    }
    return total;
  }

 private:
  enum class State : uint8_t { Plain, Size, SizeLF, Data, DataCR, DataLF,
                               Trailer, TrailerLF, Done, Error };
  static constexpr uint32_t MaxLineBytes = 512;
  static constexpr uint32_t MaxTrailerBytes = 8192;
  static int hex(uint8_t value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  }
  void nextSize() {
    _state = State::Size;
    _remaining = _lineBytes = 0;
    _digits = _extension = _sizeWhitespace = false;
  }
  void consume(uint8_t value) {
    switch (_state) {
      case State::Size: {
        if (++_lineBytes > MaxLineBytes) { _state = State::Error; break; }
        if (value == '\r') {
          _state = _digits ? State::SizeLF : State::Error;
          break;
        }
        if (_extension) {
          if (value < 0x20 && value != '\t') _state = State::Error;
          break;
        }
        if (_digits && value == ';') { _extension = true; break; }
        if (_digits && (value == ' ' || value == '\t')) { _sizeWhitespace = true; break; }
        const int digit = hex(value);
        if (_sizeWhitespace || digit < 0 || _remaining > (UINT32_MAX - static_cast<uint32_t>(digit)) / 16U) {
          _state = State::Error;
          break;
        }
        _digits = true;
        _remaining = _remaining * 16U + static_cast<uint32_t>(digit);
        break;
      }
      case State::SizeLF:
        if (value != '\n') { _state = State::Error; break; }
        _state = _remaining ? State::Data : State::Trailer;
        _lineBytes = 0;
        break;
      case State::DataCR:
        _state = value == '\r' ? State::DataLF : State::Error;
        break;
      case State::DataLF:
        if (value == '\n') nextSize();
        else _state = State::Error;
        break;
      case State::Trailer:
        if (++_trailerBytes > MaxTrailerBytes) { _state = State::Error; break; }
        if (value == '\r') _state = State::TrailerLF;
        else if (++_lineBytes > MaxLineBytes || (value < 0x20 && value != '\t')) _state = State::Error;
        break;
      case State::TrailerLF:
        if (++_trailerBytes > MaxTrailerBytes || value != '\n') { _state = State::Error; break; }
        if (!_lineBytes) _state = State::Done;
        else { _lineBytes = 0; _state = State::Trailer; }
        break;
      default:
        _state = State::Error;
        break;
    }
  }
  State _state = State::Plain;
  uint32_t _remaining = 0, _lineBytes = 0, _trailerBytes = 0;
  bool _digits = false, _extension = false, _sizeWhitespace = false;
};
