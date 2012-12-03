#pragma once

#include "ShaderTranslationTypes.h"

namespace translator
{

class ShaderTranslationUniverseImpl;
struct ShaderSemantic
{
	String Name;
	String Type;
	String HLSLSemantic;
};
typedef std::map<String, ShaderSemantic> ShaderSemantics;

struct ExpandableFunction
{
	String ReturnType;
	String Name;
	String Params;
	std::set<String> Needs;
	String Source;
};
typedef std::map<String, ExpandableFunction> Combinators;
typedef std::map<String, ExpandableFunction> Atoms;

class ShaderTranslationUniverse
{
public:

	enum TranslationUniverseError
	{
		Ok,
		InvalidSemantic,
		Invalidcombinator,
		InvalidAtom
	};

	ShaderTranslationUniverse();
	~ShaderTranslationUniverse();
	
	TranslationUniverseError AddSemantics(const char* data);
	TranslationUniverseError AddCombinators(const char* data);
	TranslationUniverseError AddAtoms(const char* data);

	const ShaderSemantics& GetSemantics() const;
	const Combinators& GetCombinators() const;
	const Atoms& GetAtoms() const;

	const std::string& GetLastError() const;

private:
	ShaderTranslationUniverseImpl* m_Impl;
};

}