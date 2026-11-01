#pragma once
#include <stdint.h>
#include <stddef.h>
namespace m5 { enum class board_t { board_M5Tab5, other }; }
struct FakeI2C {
  int32_t mv = 6800, ma = -300;
  int fail = -1, reads = 0;
  uint16_t id = 0x2260, config = 0x4127;
  bool charging = false;
  bool readRegister(uint8_t address, uint8_t reg, uint8_t* data, size_t size, uint32_t frequency) {
    ++reads;
    if (frequency != 400000) return false;
    int slot = address == 0x44 ? 4 : reg == 0xff ? 0 : reg == 0 ? 1 : reg == 2 ? 2 : 3;
    if (fail == slot) return false;
    if (address == 0x44 && reg == 0x0f && size == 1) { data[0] = charging ? 64 : 0; return true; }
    if (address != 0x41 || size != 2) return false;
    const uint16_t value = reg == 0xff ? id : reg == 0 ? config : reg == 2 ? uint16_t(mv * 4 / 5) : uint16_t(-ma * 2);
    data[0] = uint8_t(value >> 8); data[1] = uint8_t(value); return true;
  }
};
struct FakePower { unsigned off = 0; void powerOff() { ++off; } };
struct FakeM5 {
  FakeI2C In_I2C;
  FakePower Power;
  m5::board_t board = m5::board_t::board_M5Tab5;
  m5::board_t getBoard() { return board; }
};
extern FakeM5 M5;
