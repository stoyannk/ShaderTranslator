//
// Copyright 2012 Stoyan Nikolov. All rights reserved.
// Licensed under: http://www.opensource.org/licenses/BSD-2-Clause
//
#pragma once

#include "ShaderTranslationTypes.h"

namespace translator
{

class ShaderTranslatorImpl;
class ShaderTranslationUniverse;

typedef std::map<String, String> ShaderTranslationParams;

class ShaderTranslator
{
public:

	enum ShaderTranslatorError
	{
		PolymorphicParsingError,
		UnknownAtomUsed,
		UnknownShaderType,
		UnknownSemantic,
		UnexpectedEOF,
		MissingBindingParameter,
		UndeclaredParam,
		ContextIfNoEndBrace,
		Ok,
	};

	ShaderTranslator();
	~ShaderTranslator();

	ShaderTranslatorError TranslateToHLSL(const std::string& shader, const ShaderTranslationParams& params, const ShaderTranslationUniverse* universe, std::string& output);

	const std::string& GetLastError() const;

private:
	ShaderTranslatorImpl* m_Impl;
};

}

