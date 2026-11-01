#pragma once
#include <Arduino.h>
#include <vector>
namespace Tab5Secrets{inline bool unlocked=false;inline bool available(){return unlocked;}inline std::vector<String> names(){return unlocked?std::vector<String>{"GIT_USER","GIT_TOKEN"}:std::vector<String>{};}}
