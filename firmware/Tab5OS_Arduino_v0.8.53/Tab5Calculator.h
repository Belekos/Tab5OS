#pragma once
#include <cstddef>
namespace tab5calc {
struct Result { double value=0; const char* error=nullptr; size_t position=0; explicit operator bool() const {return !error;} };
Result evaluate(const char* expression, bool degrees, double answer=0, double memory=0);
struct Unit { const char* name; unsigned family; double scale, offset; };
const Unit* units(size_t& count);
Result convert(double value,unsigned from,unsigned to);
}
