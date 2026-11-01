#pragma once
#include "WriterDocument.h"
namespace writing {
struct JsonValidation { bool valid = false; size_t offset = 0; const char* message = "JSON invalide"; };
// Syntax validation without building a DOM. Constant memory, nesting <= 64.
JsonValidation validateJson(const Document& document);
}
