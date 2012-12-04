//
// Copyright 2012 Stoyan Nikolov. All rights reserved.
// Licensed under: http://www.opensource.org/licenses/BSD-2-Clause
//
#include "stdafx.h"
#include "ShaderTranslationUniverse.h"
#include "ShaderTranslationUtilities.h"

namespace translator
{

static const String EMPTY_STRING = String("");

class ShaderTranslationUniverseImpl
{
public:		
	const std::string& GetError();
	
	ShaderTranslationUniverse::TranslationUniverseError AddSemantics(const char* data);
	ShaderTranslationUniverse::TranslationUniverseError AddCombinators(const char* data);
	ShaderTranslationUniverse::TranslationUniverseError AddAtoms(const char* data);

	const ShaderSemantics& GetSemantics() const;
	const Combinators& GetCombinators() const;
	const Atoms& GetAtoms() const;

private:
	std::string m_Error;
	ShaderSemantics m_Semantics;
	Combinators m_Combinators;
	Atoms m_Atoms;
};

const std::string& ShaderTranslationUniverseImpl::GetError()
{
	return m_Error;
}

ShaderTranslationUniverse::TranslationUniverseError ShaderTranslationUniverseImpl::AddSemantics(const char* data)
{
	std::istringstream fin(data);

	const std::regex regular("(\\w+)\\s+(\\w+)\\s*:\\s*(\\w+);");

	String line;
	while(!std::getline(fin, line).eof())
	{
		String::const_iterator start, end;
		start = line.begin();
		end = line.end();
		std::match_results<String::const_iterator> match;
		while(std::regex_search(start, end, match, regular))
		{
			ShaderSemantic semantic;
			semantic.Type.assign(match[1].first, match[1].second);
			semantic.Name.assign(match[2].first, match[2].second);
			semantic.HLSLSemantic.assign(match[3].first, match[3].second);

			m_Semantics[semantic.Name] = semantic;

			start = match[0].second;
		}
	}

	return ShaderTranslationUniverse::Ok;
}

ShaderTranslationUniverse::TranslationUniverseError ShaderTranslationUniverseImpl::AddCombinators(const char* data)
{
	std::istringstream fin(data);
	
	const std::regex regular("combinator\\s+(\\w+)\\s+(\\w+)\\((.*)\\)(\\s+needs\\s+((\\w[,\\s]*)*))?");

	size_t lineNumber = 0;
	String line;
	String needs;
	while(!std::getline(fin, line).eof())
	{
		++lineNumber;

		String::const_iterator start, end;
		start = line.begin();
		end = line.end();
		std::match_results<String::const_iterator> match;
		while(std::regex_search(start, end, match, regular))
		{
			// parse a combinator signature
			ExpandableFunction conc;

			conc.ReturnType.assign(match[1].first, match[1].second);
			conc.Name.assign(match[2].first, match[2].second);
			conc.Params.assign(match[3].first, match[3].second);
			if(match.size() > 5)
			{
				needs.assign(match[5].first, match[5].second);
				
				boost::algorithm::split(conc.Needs, needs, boost::algorithm::is_any_of(", "), boost::algorithm::token_compress_on);
				// Remove empties
				conc.Needs.erase(EMPTY_STRING);
			}

			size_t startPosition = std::string::npos;
			size_t scopes = 0;
			size_t lineStart = 0;
			std::ostringstream source;
			while(!std::getline(fin, line).eof())
			{
				++lineNumber;
				if(startPosition == std::string::npos)
				{
					startPosition = line.find("{");
					if(startPosition != std::string::npos)
					{
						++scopes;						
					}
					lineStart = startPosition + 1;
					source << line.substr(lineStart);
				}
				else
				{
					lineStart = 0;
				}

				FindAndCountBraces(source, line, lineStart, scopes);
				
				if(!scopes) break;

				source << std::endl;
			}

			if(scopes)
			{
				std::ostringstream error;
				error << "combinator parsing error on line: " << lineNumber;
				m_Error = error.str().c_str();
				return ShaderTranslationUniverse::Invalidcombinator;
			}

			conc.Source = source.str().c_str();
			m_Combinators[conc.ReturnType] = conc;
			break;
		}
	}

	return ShaderTranslationUniverse::Ok;
}

ShaderTranslationUniverse::TranslationUniverseError ShaderTranslationUniverseImpl::AddAtoms(const char* data)
{
	const std::regex regular("atom\\s+(\\w+)\\s+(\\w+)\\((.*)\\)(\\s+needs\\s+((\\w[,\\s]*)*))?");	

	std::istringstream fin(data);
	
	size_t lineNumber = 0;
	String line;
	String needs;
	while(!std::getline(fin, line).eof())
	{
		++lineNumber;
	
		String::const_iterator start, end;
		start = line.begin();
		end = line.end();
		std::match_results<String::const_iterator> match;
		while(std::regex_search(start, end, match, regular))
		{
			// parse a atom signature
			ExpandableFunction atom;
	
			atom.ReturnType.assign(match[1].first, match[1].second);
			atom.Name.assign(match[2].first, match[2].second);
			atom.Params.assign(match[3].first, match[3].second);
			if(match.size() > 5)
			{
				needs.assign(match[5].first, match[5].second);
	
				boost::algorithm::split(atom.Needs, needs, boost::algorithm::is_any_of(", "), boost::algorithm::token_compress_on);
				// Remove empties
				atom.Needs.erase(EMPTY_STRING);
			}
	
			size_t startPosition = std::string::npos;
			size_t scopes = 0;
			size_t lineStart = 0;
			std::ostringstream source;
			while(!std::getline(fin, line).eof())
			{
				++lineNumber;
				if(startPosition == std::string::npos)
				{
					startPosition = line.find("{");
					if(startPosition != std::string::npos)
					{
						++scopes;						
					}
					lineStart = startPosition + 1;
					source << line.substr(lineStart);
				}
				else
				{
					lineStart = 0;
				}
	
				FindAndCountBraces(source, line, lineStart, scopes);
	
				if(!scopes) break;
	
				source << std::endl;
			}
	
			if(scopes)
			{
				std::ostringstream error;
				error << "Atom parsing error on line: " << lineNumber;
				m_Error = error.str().c_str();
				return ShaderTranslationUniverse::InvalidAtom;
			}
	
			atom.Source = source.str().c_str();
			m_Atoms[atom.Name] = atom;
			break;
		}
	}
	
	return ShaderTranslationUniverse::Ok;
}
	 
const ShaderSemantics& ShaderTranslationUniverseImpl::GetSemantics() const
{
	return m_Semantics;
}
const Combinators& ShaderTranslationUniverseImpl::GetCombinators() const
{
	return m_Combinators;
}
const Atoms& ShaderTranslationUniverseImpl::GetAtoms() const
{
	return m_Atoms;
}

///////////////////////////////////////////////////////////////
ShaderTranslationUniverse::ShaderTranslationUniverse()
	: m_Impl(new ShaderTranslationUniverseImpl)
{}

ShaderTranslationUniverse::~ShaderTranslationUniverse()
{
	delete m_Impl;
}

ShaderTranslationUniverse::TranslationUniverseError ShaderTranslationUniverse::AddSemantics(const char* data)
{
	return m_Impl->AddSemantics(data);
}

ShaderTranslationUniverse::TranslationUniverseError ShaderTranslationUniverse::AddCombinators(const char* data)
{
	return m_Impl->AddCombinators(data);
}

ShaderTranslationUniverse::TranslationUniverseError ShaderTranslationUniverse::AddAtoms(const char* data)
{
	return m_Impl->AddAtoms(data);
}

const ShaderSemantics& ShaderTranslationUniverse::GetSemantics() const
{
	return m_Impl->GetSemantics();
}

const Combinators& ShaderTranslationUniverse::GetCombinators() const
{
	return m_Impl->GetCombinators();
}

const Atoms& ShaderTranslationUniverse::GetAtoms() const
{
	return m_Impl->GetAtoms();
}

const std::string& ShaderTranslationUniverse::GetLastError() const
{
	return m_Impl->GetError();	
}

}
