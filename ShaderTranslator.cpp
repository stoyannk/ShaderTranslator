#include "stdafx.h"
#include "ShaderTranslator.h"
#include "ShaderTranslationUniverse.h"
#include "ShaderTranslationUtilities.h"

namespace translator
{

boost::thread_specific_ptr<TlsScratchAllocator> TlsScratchAllocator::sPtr;

static const char* MAP_PREFIX       = "MAP_";
static const char* SAMPLER_PREFIX   = "SAMPLER_";
static const char* VOID_TYPE        = "VOID";
static const char* VS_POSITION      = "float4 Position : POSITION;";
static const char* PS_POSITION      = "float4 Position : SV_POSITION;";
static const char* TEXCOORD_STRING  = "TEXCOORD";
static const char* VS_INPUT_STRING  = "VS_INPUT";
static const char* VS_OUTPUT_STRING = "VS_OUTPUT";
static const char* PS_INPUT_STRING  = "PS_INPUT";
static const char* POSITION_STRING  = "POSITION";

enum TranslatorShaderType
{
	VertexShader,
	PixelShader
};

typedef std::multimap<ScratchString, ScratchString>	Polymorphics;
struct ParsingState
{
	Polymorphics Functions;
	std::vector<ScratchString> Code;
};

class ShaderTranslatorImpl
{
public:
	const std::string& GetError();

	ShaderTranslator::ShaderTranslatorError TranslateToHLSL(const std::string& shader, const ShaderTranslationParams& params, const ShaderTranslationUniverse* universe, std::string& output);

private:
	struct CodeState
	{
		TranslatorShaderType Type;
		ScratchString InputName;
		ScratchString OutputName;
		ScratchString ShaderSignature;

		std::set<ScratchString> InputSemantics;
		std::set<ScratchString> OutputSemantics;
		std::set<ScratchString> ContextSemantics;
		std::set<ScratchString> AvailableSemantics;
		std::set<ScratchString> InputTextures;
		std::set<ScratchString> InputSamplers;

		ScratchString InnerSource;
	};

	ShaderTranslator::ShaderTranslatorError ParsePolymorphic(std::istream& stream, Polymorphics& polys, const ShaderTranslationUniverse* universe, const ScratchString& name);
	ShaderTranslator::ShaderTranslatorError ParseShader(std::istream& stream, ParsingState& state, const ShaderTranslationParams& params, const ShaderTranslationUniverse* universe, TranslatorShaderType type, std::match_results<ScratchString::const_iterator>& match);
	ShaderTranslator::ShaderTranslatorError ExpandFunction(const ExpandableFunction& function, CodeState& state, const ShaderTranslationUniverse* universe, ScratchString& output);
	ShaderTranslator::ShaderTranslatorError ExpandShaderInput(std::ostringstream& inputStruct, CodeState& codeState, const ShaderSemantics& semantics);

private:
	std::string m_Error;
};

const std::string& ShaderTranslatorImpl::GetError()
{
	return m_Error;
}

ShaderTranslator::ShaderTranslatorError ShaderTranslatorImpl::ExpandFunction(const ExpandableFunction& function, CodeState& state, const ShaderTranslationUniverse* universe, ScratchString& output)
{
	const Combinators& combinators = universe->GetCombinators();
	// Check all needs
	std::for_each(function.Needs.begin(), function.Needs.end(), [&](const String& need)
	{
		auto found = std::find(state.AvailableSemantics.begin(), state.AvailableSemantics.end(), need);
		if(found == state.AvailableSemantics.end())
		{
			auto combinator = combinators.find(need);
			if(combinator != combinators.end())
			{
				ExpandFunction(combinator->second, state, universe, output);
			}
			else
			{
				if(need.find(MAP_PREFIX) == 0)
				{
					state.InputTextures.insert(ScratchString(need));
				}
				else if(need.find(SAMPLER_PREFIX) == 0)
				{
					state.InputSamplers.insert(ScratchString(need));
				}
				else
				{
					state.InputSemantics.insert(ScratchString(need));
					state.ContextSemantics.insert(ScratchString(need));
					state.AvailableSemantics.insert(ScratchString(need));
				}
			}
		}
	});
	
	if(function.ReturnType != VOID_TYPE)
	{
		state.ContextSemantics.insert(ScratchString(function.ReturnType));
		state.AvailableSemantics.insert(ScratchString(function.ReturnType));
	}
	
	output.append("\n{ // ");
	output.append(function.Name.begin(), function.Name.end());

	static const std::regex returnStatementFinder("return\\s+(.*);");
	std::match_results<ScratchString::const_iterator> match;
	auto start = function.Source.begin();
	auto end = function.Source.end();
	if(std::regex_search(start, end, match, returnStatementFinder))
	{
		output.append(start, match[0].first);

		output.append("context.");
		output.append(boost::algorithm::to_lower_copy(function.ReturnType));
		output.append(" = ");
		output.append(match[1].first, match[1].second);
		output.append(";");
		output.append(match[0].second, end);
	}
	else
	{
		output.append(start, end);
	}
	output.append("}\n");			
	return ShaderTranslator::Ok;
}

ShaderTranslator::ShaderTranslatorError ShaderTranslatorImpl::ParsePolymorphic(std::istream& stream, Polymorphics& polys, const ShaderTranslationUniverse* universe, const ScratchString& name)
{
	std::ostringstream source;
	ScratchString line;
	bool inCode = false;
	bool end = false;
	while(!std::getline(stream, line).eof() && !end)
	{
		if(!inCode && (line.find('{') != ScratchString::npos))
		{
			inCode = true;
		}
		else if(inCode)
		{
			for(size_t i = 0; i < line.size(); ++i)
			{
				if(line[i] == '}')
				{
					end = true;
					break;
				}
				source << line[i];
			}
		}
	}

	if(stream.eof() && !end)
	{
		m_Error = "Error parsing polymorphic types - EOF";
		return ShaderTranslator::PolymorphicParsingError;
	}

	const auto& atoms = universe->GetAtoms();
	auto CheckAtom = [&atoms](const ScratchString& atom) -> bool { return atoms.find(atom) != atoms.end(); };

	std::vector<ScratchString> ptrs;
	boost::algorithm::split(ptrs, source.str(), boost::algorithm::is_any_of(", "), boost::algorithm::token_compress_on);

	for(auto it = ptrs.begin(); it != ptrs.end(); ++it)
	{
		boost::trim(*it);
		if(!CheckAtom(*it))
		{
			m_Error = "Unknown atom used: ";
			m_Error.append(it->c_str());
			return ShaderTranslator::UnknownAtomUsed;
		}
		polys.insert(std::make_pair(name, *it));
	}
	
	return ShaderTranslator::Ok;
}

ShaderTranslator::ShaderTranslatorError ShaderTranslatorImpl::ExpandShaderInput(std::ostringstream& inputStruct, CodeState& codeState, const ShaderSemantics& semantics)
{
	inputStruct << "struct " << codeState.InputName << " { " << std::endl;
	
	switch(codeState.Type)
	{
	case VertexShader:
		inputStruct << VS_POSITION << std::endl;
		break;
	case PixelShader:
		inputStruct << PS_POSITION << std::endl;
		break;
	}

	std::map<ScratchString, size_t> semanticCounters;
	for(auto semantic = codeState.InputSemantics.cbegin(); semantic != codeState.InputSemantics.cend(); ++semantic)
	{
		if(semantic->find(MAP_PREFIX) == 0)
		{
			codeState.InputTextures.insert(*semantic);						
			continue;
		}
		else if(semantic->find(SAMPLER_PREFIX) == 0)
		{
			codeState.InputSamplers.insert(*semantic);
			continue;
		}
				
		auto regSemantic = semantics.find(*semantic);
		if(regSemantic == semantics.end())
		{
			m_Error = "Unknown semantic found: ";
			m_Error.append(semantic->begin(), semantic->end());
			return ShaderTranslator::UnknownSemantic;
		}
		inputStruct << regSemantic->second.Type << " " << boost::to_lower_copy(regSemantic->second.Name) << " : ";

		switch(codeState.Type)
		{
		case VertexShader:
			inputStruct << regSemantic->second.HLSLSemantic	<< semanticCounters[ScratchString(regSemantic->second.HLSLSemantic)]++;
			break;
		case PixelShader:
			inputStruct << TEXCOORD_STRING << semanticCounters[TEXCOORD_STRING]++;
			break;
		}
		inputStruct << ";" << std::endl;
	}
	inputStruct << "};" << std::endl;

	return ShaderTranslator::Ok;
}

ShaderTranslator::ShaderTranslatorError ShaderTranslatorImpl::ParseShader(std::istream& stream
																		, ParsingState& state
																		, const ShaderTranslationParams& params
																		, const ShaderTranslationUniverse* universe
																		, TranslatorShaderType type
																		, std::match_results<ScratchString::const_iterator>& match)
{
	CodeState codeState;
	switch(type)
	{
	case VertexShader:
		codeState.InputName = VS_INPUT_STRING;
		codeState.OutputName = VS_OUTPUT_STRING;
		break;
	case PixelShader:
		codeState.InputName = PS_INPUT_STRING;
		break;
	default:
		return ShaderTranslator::UnknownShaderType;
		break;
	}
	codeState.Type = type;
	
	codeState.ShaderSignature.assign(match[1].first, match[1].second);

	const ShaderSemantics& semantics = universe->GetSemantics();
	auto CheckSemantic = [semantics](const ScratchString& semantic) -> bool { return semantics.find(semantic) != semantics.end(); };

	// Check the needs of the shader itself
	ScratchString temp(match[7].first, match[7].second);
	std::vector<ScratchString> shaderNeeds;
	boost::algorithm::split(shaderNeeds, temp, boost::algorithm::is_any_of(", "), boost::algorithm::token_compress_on);
	// Remove empties
	shaderNeeds.erase(std::remove_if(shaderNeeds.begin(), shaderNeeds.end(), [](const ScratchString& str) -> bool { return str.empty(); }), shaderNeeds.end());

	for(auto it = shaderNeeds.cbegin(); it != shaderNeeds.cend(); ++it)
	{
		if(it->find(MAP_PREFIX) == 0)
		{
			codeState.InputTextures.insert(*it);
		}
		else if(it->find(SAMPLER_PREFIX) == 0)
		{
			codeState.InputSamplers.insert(*it);
		}
		else
		{
			if(!CheckSemantic(*it))
			{
				m_Error = "Unknown semantic found: ";
				m_Error.append(it->begin(), it->end());
				return ShaderTranslator::UnknownSemantic;
			}

			// If it is already available - skip it
			if(codeState.AvailableSemantics.find(*it) == codeState.AvailableSemantics.end())
			{
				codeState.InputSemantics.insert(*it);
				codeState.ContextSemantics.insert(*it);
				codeState.AvailableSemantics.insert(*it);
			}
		}
	}

	// Inner code parsing
	ScratchString line;
	size_t startPosition = ScratchString::npos;
	size_t scopes = 0;
	size_t lineStart = 0;
	std::ostringstream leCode;
	while(!std::getline(stream, line).eof())
	{
		if(startPosition == ScratchString::npos)
		{
			startPosition = line.find("{");
			if(startPosition != ScratchString::npos)
			{
				++scopes;						
			}
			lineStart = startPosition + 1;
			line = line.substr(lineStart).c_str();
		}
		else
		{
			lineStart = 0;
		}

		FindAndCountBraces(leCode, line, lineStart, scopes);
		
		if(!scopes) break;
		leCode << std::endl;
	}

	if (scopes != 0)
	{
		m_Error = "Unexpected end of file";
		return ShaderTranslator::UnexpectedEOF;
	}

	ScratchString shaderCode = leCode.str().c_str();

	enum CurrentTranslation
	{
		CT_Polymorphic = 0,
		CT_Output,
		CT_ContextIf,
		CT_ContextIfNot,
		CT_None
	};
	
	static const std::regex codeTranslations[] = 
	{
		std::regex("(\\n+)\\s+context\\.(\\w+)\\s+=\\s+(\\w+)\\(\\);"),
		std::regex("(\\n+)\\s+output\\.(\\w+)\\s+=.*"),
		std::regex("(\\s+)CONTEXT_IF\\(context\\.(\\w+)\\)( )*\\{"),
		std::regex("(\\s+)CONTEXT_IFNOT\\(context\\.(\\w+)\\)( )*\\{")
	};

	ScratchString::const_iterator start, end;
	start = shaderCode.begin();
	end = shaderCode.end();
	bool contextIf = false;
	while(start != shaderCode.end())
	{
		std::match_results<ScratchString::const_iterator> match;
		std::match_results<ScratchString::const_iterator> trymatch;

		CurrentTranslation currentTranslation = CT_None;
		// Search for all the code that needs translation
		for(int translationIdx = 0; translationIdx < CT_None; ++translationIdx)
		{
			bool available = std::regex_search(start, end, trymatch, codeTranslations[translationIdx]);

			if(available && (match.empty() || (trymatch[0].first < match[0].first)))
			{
				match = trymatch;
				currentTranslation = (CurrentTranslation)translationIdx;
			}
		}

		contextIf = false;

		switch(currentTranslation)
		{
		case CT_ContextIf:
			contextIf = true;
		case CT_ContextIfNot:
			{
				codeState.InnerSource.append(start, match[1].second);
				// if there is such a value in the context - expand the code - otherwise remove it
				ScratchString valueRequired(match[2].first, match[2].second);
				boost::to_upper(valueRequired);
				// find the closing brace
				auto braceFind = match[0].second;
				int ifScopes = 1;
				while(braceFind != end && ifScopes)
				{
					++braceFind;
					if(*braceFind == '{')
					{
						++ifScopes;
					}
					else if(*braceFind == '}')
					{
						--ifScopes;
					}
				}
				if(ifScopes)
				{
					m_Error = "Unable to find closing brace for CONTEXT_IF statement on value ";
					m_Error.append(valueRequired.begin(), valueRequired.end());
					return ShaderTranslator::ContextIfNoEndBrace;
				}

				const bool isSemanticAvailable = codeState.AvailableSemantics.find(valueRequired) != codeState.AvailableSemantics.end();
				
				if((contextIf && isSemanticAvailable) || (!contextIf && !isSemanticAvailable))
				{
					// inject the code in the parsed source and NOT in the ready one to allow recursive ifs and nested polymorphics
					const size_t ifIdx = match[0].first - shaderCode.begin(); 
					shaderCode.erase(match[0].first, match[0].second);
					ScratchString exp;
					exp.append("{ // conditional if on semantic ");
					exp.append(valueRequired);
					shaderCode.insert(ifIdx, exp);

					start = shaderCode.begin() + ifIdx;
					end = shaderCode.end();
				}
				else
				{
					start = braceFind + 1; // +1 is for the closing brace - we don't want it
				}
			}
		break;
		case CT_Output:
			{
				codeState.InnerSource.append(start, match[1].second);
				
				ScratchString semanticName(match[2].first, match[2].second);
				boost::to_upper(semanticName);

				if(codeState.Type == VertexShader && CheckSemantic(semanticName))
				{
					codeState.OutputSemantics.insert(semanticName);
				}

				codeState.InnerSource.append(match[0].first, match[0].second);
				start = match[0].second;
			}
		break;
		case CT_Polymorphic:
			{
				codeState.InnerSource.append(start, match[1].second);

				// Add code
				ScratchString functionName(match[3].first, match[3].second);
				// Check if it is a valid polymorphic
				auto polymorphicLower = state.Functions.lower_bound(functionName);
				if(polymorphicLower != state.Functions.end() && polymorphicLower->first == functionName)
				{
					// Select the proper atom
					auto atom = params.find(functionName);
					if(atom == params.end())
					{
						m_Error = "Missing binding parameter for polymorphic ";
						m_Error.append(functionName.begin(), functionName.end());
						return ShaderTranslator::MissingBindingParameter;
					}

					auto polymorphicUpper = state.Functions.upper_bound(functionName);
					// Check if such a binding function exists
					if(std::find_if(polymorphicLower, polymorphicUpper, [&atom](Polymorphics::value_type it) -> bool { return it.second == atom->second; } ) == state.Functions.end())
					{
						m_Error = "Undeclared binding parameter used for polymorphic ";
						m_Error.append(functionName.c_str());
						return ShaderTranslator::UndeclaredParam;
					}

					const auto& atoms = universe->GetAtoms();
					auto atomDefinition = atoms.find(atom->second);

					ScratchString expandedAtom;
					ShaderTranslator::ShaderTranslatorError err = ExpandFunction(atomDefinition->second, codeState, universe, expandedAtom);
					if(err != ShaderTranslator::Ok)
					{
						return err;
					}

					codeState.InnerSource.append(expandedAtom);
				}
				// Not a polymorphic - paste the code as is
				else
				{
					codeState.InnerSource.append(match[0].first, match[0].second);	
				}

				start = match[0].second;
			}
		break;
		case CT_None:
			codeState.InnerSource.append(start, end);
			start = end;
		break;
		}
	}

	// Create the input structure
	std::ostringstream inputStruct;
	auto err = ExpandShaderInput(inputStruct, codeState, semantics);
	if(err != ShaderTranslator::Ok)
	{
		return err;
	}
	
	std::ostringstream contextDeclaration;
	contextDeclaration << std::endl << "\tstruct {" << std::endl;
	for(auto semantic = codeState.ContextSemantics.cbegin(); semantic != codeState.ContextSemantics.end(); ++semantic)
	{
		auto semanticDefinition = semantics.find(*semantic);

		if(semanticDefinition == semantics.end())
		{
			m_Error = "Unknown semantic found: ";
			m_Error.append(semantic->begin(), semantic->end());
			return ShaderTranslator::UnknownSemantic;
		}

		contextDeclaration << "\t\t" << semanticDefinition->second.Type << " " << boost::to_lower_copy(semanticDefinition->second.Name) << ";" << std::endl;
	}
	contextDeclaration << "\t} context;" << std::endl;

	std::ostringstream outputStruct;
	if(codeState.Type == VertexShader && codeState.OutputSemantics.size())
	{
		outputStruct << std::endl << "\tstruct " << codeState.OutputName << "{" << std::endl;
		outputStruct << "\t\t " << PS_POSITION << " " << std::endl;
		
		unsigned counter = 0;
		for(auto semantic = codeState.OutputSemantics.cbegin(); semantic != codeState.OutputSemantics.end(); ++semantic)
		{
			auto semanticDefinition = semantics.find(*semantic);
			if(*semantic != POSITION_STRING)
			{
				outputStruct << "\t\t" << semanticDefinition->second.Type << " " << boost::to_lower_copy(semanticDefinition->second.Name) << " : TEXCOORD" << counter++ << ";" << std::endl;				
			}
		}

		outputStruct << "};" << std::endl;
	}

	std::ostringstream contextPopulation;
	for(auto semantic = codeState.InputSemantics.cbegin(); semantic != codeState.InputSemantics.end(); ++semantic)
	{
		ScratchString lowerSemantic = boost::to_lower_copy(*semantic);
		contextPopulation << "\tcontext." << lowerSemantic << " = input." << lowerSemantic << ";" << std::endl;
	}

	std::ostringstream textureInputs;
	size_t registerCount = 0;
	for(auto semantic = codeState.InputTextures.begin(); semantic != codeState.InputTextures.end(); ++semantic)
	{
		textureInputs << "Texture2D " << boost::to_lower_copy(*semantic) << " : register(t" << registerCount++ << ");" << std::endl;
	}

	std::ostringstream samplerInputs;
	registerCount = 0;
	for(auto semantic = codeState.InputSamplers.begin(); semantic != codeState.InputSamplers.end(); ++semantic)
	{
		textureInputs << "SamplerState " << boost::to_lower_copy(*semantic) << " : register(s" << registerCount++ << ");" << std::endl;
	}

	// Compose the final shader
	state.Code.push_back(ScratchString("//texture inputs \n"));
	state.Code.back().append(textureInputs.str().c_str());
	state.Code.back().append("//sampler inputs \n");
	state.Code.back().append(samplerInputs.str().c_str());
	state.Code.back().append("//input \n");
	state.Code.back().append(inputStruct.str().c_str());
	state.Code.back().append("\n");
	if(codeState.OutputSemantics.size())
	{
		state.Code.back().append("//output \n");
		state.Code.back().append(outputStruct.str().c_str());
		state.Code.back().append("\n");
	}
	state.Code.back().append(codeState.ShaderSignature);
	state.Code.back().append("\n{\n");
	state.Code.back().append(contextDeclaration.str().c_str());
	state.Code.back().append("\n\t//context population \n");
	state.Code.back().append(contextPopulation.str().c_str());
	state.Code.back().append("\n");
	state.Code.back().append(codeState.InnerSource);
	state.Code.back().append("}\n");

	return ShaderTranslator::Ok;
}

ShaderTranslator::ShaderTranslatorError ShaderTranslatorImpl::TranslateToHLSL(const std::string& shader
																			, const ShaderTranslationParams& params
																			, const ShaderTranslationUniverse* universe
																			, std::string& output)
{
	static const std::regex polyRegular("\\s*polymorphic\\s+(\\w+)");
	//static const std::regex vsRegular("\\s*vertex_shader\\s+((\\w+)\\s+(\\w+)\\(([\\w\\s,]+)\\)(\\s+:\\s*\\w+)?)(\\s+needs\\s+((\\w*[,\\s]?)*))?");
	//static const std::regex psRegular("\\s*pixel_shader\\s+((\\w+)\\s+(\\w+)\\(([\\w\\s,]+)\\)(\\s+:\\s*\\w+)?)(\\s+needs\\s+((\\w*[,\\s]?)*))?");
	static const std::regex vsRegular("\\s*vertex_shader\\s+((\\w+)\\s+(\\w+)\\(([\\w\\s,]+)\\)(\\s+:\\s*\\w+)?)(\\s+needs\\s+((\\w[,\\s]*)*))?");
	static const std::regex psRegular("\\s*pixel_shader\\s+((\\w+)\\s+(\\w+)\\(([\\w\\s,]+)\\)(\\s+:\\s*\\w+)?)(\\s+needs\\s+((\\w[,\\s]*)*))?");
	
	ParsingState state;
	std::ostringstream hlsl;
	std::istringstream stream(shader.c_str());

	ScratchString line;
	while(!std::getline(stream, line).eof())
	{
		ScratchString::const_iterator start, end;
		start = line.begin();
		end = line.end();
		std::match_results<ScratchString::const_iterator> match;
		// Poly
		if(std::regex_search(start, end, match, polyRegular))
		{
			ScratchString name(match[1].first, match[1].second);
			ShaderTranslator::ShaderTranslatorError err = ParsePolymorphic(stream, state.Functions, universe, name);
			if(err != ShaderTranslator::Ok)
			{
				return err;
			}
			continue;
		}
		// VS
		else if(std::regex_search(start, end, match, vsRegular))
		{
			ShaderTranslator::ShaderTranslatorError err = ParseShader(stream, state, params, universe, VertexShader, match);
			if(err != ShaderTranslator::Ok)
			{
				return err;
			}
			hlsl << state.Code.back();
			continue;
		}
		// PS
		else if(std::regex_search(start, end, match, psRegular))
		{
			ShaderTranslator::ShaderTranslatorError err = ParseShader(stream, state, params, universe, PixelShader, match);
			if(err != ShaderTranslator::Ok)
			{
				return err;
			}
			hlsl << state.Code.back();
			continue;
		}

		hlsl << line << std::endl;
	}

	output = hlsl.str().c_str();

	return ShaderTranslator::Ok;
}

///////////////////////////////////////////////////////////////
ShaderTranslator::ShaderTranslator()
	: m_Impl(new ShaderTranslatorImpl)
{}

ShaderTranslator::~ShaderTranslator()
{
	delete m_Impl;
}

ShaderTranslator::ShaderTranslatorError ShaderTranslator::TranslateToHLSL(const std::string& shader
														, const ShaderTranslationParams& params
														, const ShaderTranslationUniverse* universe
														, std::string& output)
{
	struct AllocatorScope
	{
		AllocatorScope(unsigned sz)
		{
			TlsScratchAllocator::Create(sz);
		}

		~AllocatorScope()
		{
			TlsScratchAllocator::Destroy();
		}
	};

	AllocatorScope alloc(1024*128);
	return m_Impl->TranslateToHLSL(shader, params, universe, output);	
}

const std::string& ShaderTranslator::GetLastError() const
{
	return m_Impl->GetError();
}
	
}
