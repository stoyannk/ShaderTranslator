//
// Copyright 2012 Stoyan Nikolov. All rights reserved.
// Licensed under: http://www.opensource.org/licenses/BSD-2-Clause
//
#pragma once

#include "ShaderTranslationTypes.h"

namespace translator
{

void FindAndCountBraces(std::ostringstream& source, const String& line, size_t lineStart, size_t& scopes);

}