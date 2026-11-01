#pragma once
#include <stdint.h>
#include <stddef.h>
namespace m5 { enum class board_t { board_M5Tab5, Other }; }
struct FakeTouch {
  struct Config { int i2c_port = 1; int i2c_addr = 0x55; };
  Config settings;
  bool spi = false;
  bool isSPI() const { return spi; }
  Config config() const { return settings; }
};
struct FakeDisplay {
  FakeTouch* active = nullptr;
  FakeTouch* touch() { return active; }
};
struct FakeM5 {
  FakeDisplay Display;
  m5::board_t board = m5::board_t::board_M5Tab5;
  m5::board_t getBoard() { return board; }
};
extern FakeM5 M5;
namespace lgfx { namespace i2c {
struct Result { bool succeeded; bool has_value() const { return succeeded; } };
Result transactionWriteRead(int port, int addr, const uint8_t* tx, size_t txlen,
                            uint8_t* rx, size_t rxlen, int speed);
} }
