#pragma once
#include <functional>
#include <string>
#include <vector>
namespace Tab5Ssh {
using Output = std::function<void(const char*,size_t)>;
using Input = std::function<size_t(char*,size_t)>;
using Cancel = std::function<bool()>;
using Translate = std::function<std::string(const char*)>;
// Runs on the existing terminal worker. Owns one complete NetworkGate lease;
// never performs network I/O on the LVGL task. Returns -1 for other commands.
int run(const std::vector<std::string>& args, Output output, Cancel cancelled,
        Translate translate, const std::string& cwd, Output errors = {},
        Input input = {});
}
