#pragma once

#include "ShaderTranslationTypes.h"

namespace translator
{

void FindAndCountBraces(std::ostringstream& source, const String& line, size_t lineStart, size_t& scopes);

}