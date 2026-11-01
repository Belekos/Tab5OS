#pragma once
#include <Arduino.h>
namespace Tab5Python {
bool run(const char* code,const char* path,const char* argument=nullptr);
bool runFile(const char* path,const char* argument=nullptr);
void stop();
bool running();
uint32_t vmStartCount();
const char* runningPath();
const char* runningArgument();
bool failed();
String output();
size_t copyOutput(char* destination,size_t capacity);
uint32_t outputRevision();
void clearOutput();
const char* status();
}
