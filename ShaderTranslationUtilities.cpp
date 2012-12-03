#include "stdafx.h"

#include "ShaderTranslationUtilities.h"

namespace translator
{

void FindAndCountBraces(std::ostringstream& source, const String& line, size_t lineStart, size_t& scopes)
{
	for(size_t i = lineStart; i < line.size(); ++i)
	{
		if(line[i] == '{')
		{
			++scopes;
		}
		else if(line[i] == '}')
		{
			--scopes;
			if(!scopes) break;
		}

		source << line[i]; 
	}
}

}