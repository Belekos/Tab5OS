#pragma once
#include "Tab5NetworkTools.h"
#include "Tab5NetworkArgs.h"
namespace Tab5NetworkTools {
int http(const Tab5NetworkArgs::Options&, Output, Cancel, Translate, const std::string& cwd, Output errors={});
}
