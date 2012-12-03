#include "stdafx.h"

#include "ShaderTranslator.h"
#include "ShaderTranslationUniverse.h"

#include <iostream>
#include <fstream>
#include <exception>

using namespace translator;

struct TestCase
{
	std::string FileName;
	std::string OutputName;
	ShaderTranslationParams Params;
};

enum Test
{
	TE_GBufferPS,
	TE_LightPass,

	TE_Count
};

TestCase Cases[TE_Count];

void FillTestCases()
{
	Cases[TE_GBufferPS].FileName = "Tests/GBuffer_PS.txt";
	Cases[TE_GBufferPS].OutputName = "Tests/output.txt";
	Cases[TE_GBufferPS].Params.insert(std::make_pair("MakeDepth", "CalculateProjectionDepth"));
	Cases[TE_GBufferPS].Params.insert(std::make_pair("GetWorldNormal", "NormalFromMap"));

	Cases[TE_LightPass].FileName = "Tests/LightPass.txt";
	Cases[TE_LightPass].OutputName = "Tests/output.txt";
	Cases[TE_LightPass].Params.insert(std::make_pair("GetAlpha", "AlphaFromMap"));
	Cases[TE_LightPass].Params.insert(std::make_pair("GetAlbedo", "AlbedoFromMap"));
	Cases[TE_LightPass].Params.insert(std::make_pair("GetSpecularColor", "SpecularFromMap"));
}

std::string ReadWholeFile(const std::string& filename)
{
	std::ifstream fin(filename.c_str());

	if(!fin.is_open())
	{
		throw std::exception("Unable to open file");
	}

	return std::string(std::istreambuf_iterator<char>(fin), std::istreambuf_iterator<char>());
}

int main(int argc, char* argv[])
try
{
	FillTestCases();
	Test currentTest = TE_LightPass;

	ShaderTranslationUniverse universe;
	
	auto semantics = ReadWholeFile("Tests/semantics.txt");
	auto error = universe.AddSemantics(semantics.c_str());
	if(error != ShaderTranslationUniverse::Ok)
	{
		std::cerr << "Unable to read semantics: " << universe.GetLastError() << std::endl;
	}

	auto atoms = ReadWholeFile("Tests/atoms.txt");
	error = universe.AddAtoms(atoms.c_str());
	if(error != ShaderTranslationUniverse::Ok)
	{
		std::cerr << "Unable to read atoms: " << universe.GetLastError() << std::endl;
	}

	auto combinators = ReadWholeFile("Tests/combinators.txt");
	error = universe.AddCombinators(combinators.c_str());
	if(error != ShaderTranslationUniverse::Ok)
	{
		std::cerr << "Unable to read combinators: " << universe.GetLastError() << std::endl;
	}

	ShaderTranslator transl;

	auto shader = ReadWholeFile(Cases[currentTest].FileName);
	std::string output;
		
	auto translError =  transl.TranslateToHLSL(shader, Cases[currentTest].Params, &universe, output);
	if(translError != ShaderTranslator::Ok)
	{
		std::cerr << "Unable to translate shader: " << transl.GetLastError() << std::endl;
	}

	std::ofstream fout(Cases[currentTest].OutputName);
	fout << output;

	std::cout << "Resulting shader: " << std::endl;
	std::cout << output;

	return 0;
}
catch(std::exception& ex)
{
	std::cerr << "Exception: " << ex.what() << std::endl;
}
