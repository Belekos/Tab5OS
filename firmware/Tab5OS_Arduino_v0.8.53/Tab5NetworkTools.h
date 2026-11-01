#pragma once
#include <functional>
#include <string>
#include <vector>

namespace Tab5NetworkTools {
using Output = std::function<void(const char*, size_t)>;
using Cancel = std::function<bool()>;
using Translate = std::function<std::string(const char*)>;
// Run on a worker, never on the LVGL task. -1 means unhandled command.
// Network tests use one explicitly supplied IPv4 host, with bounded waits.
int run(const std::vector<std::string>& args, Output output, Cancel cancel,
        Translate translate, const std::string& cwd="/", Output errors={});
}
