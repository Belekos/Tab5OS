// Compile the same bounded admission parser used before loading native apps.
#include "Tab5ElfValidate.h"
#include <fstream>
#include <iostream>
#include <vector>

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
  if (!input) { std::cerr << "Cannot open ELF\n"; return 2; }
  const auto length = input.tellg();
  if (length < 0 || length > std::streamoff(Tab5ElfValidate::MaxFile)) {
    std::cerr << "ELF exceeds the production file budget\n";
    return 1;
  }
  std::vector<uint8_t> bytes(static_cast<size_t>(length));
  input.seekg(0);
  if (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()), length)) {
    std::cerr << "Incomplete ELF read\n";
    return 2;
  }
  Tab5ElfValidate::Image image;
  const char* reason = nullptr;
  if (!Tab5ElfValidate::validate(bytes.data(), bytes.size(), image, &reason)) {
    std::cerr << (reason ? reason : "ELF rejected") << '\n';
    return 1;
  }
  std::cout << "{\"base\":" << image.base << ",\"imageBytes\":" << image.bytes
            << ",\"entry\":" << image.entry << ",\"stackBytes\":" << image.stack
            << "}\n";
}
