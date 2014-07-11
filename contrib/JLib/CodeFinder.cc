// Title: Jeff Benson's Console Library
// File: CodeFinder.h
// Author: Jeff Benson
// Date: 7/28/11
// Last Updated: 8/2/2011
// Contact: pepsibot@hotmail.com
//
// Copyright (C) 2011
// This file is part of JLib.
//
// JLib is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// JLib is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with JLib.  If not, see <http://www.gnu.org/licenses/>.
#include "CodeFinder.h"
using namespace std;

vector<unsigned> FindCodeLocations(string text)
{
	vector<unsigned> codeStartPositions;
	string::iterator nextCode = string::iterator();
	// string::iterator phraseStart = string::iterator();
	for(unsigned x = 0; x < text.length(); x++)
	{
		if(text[x] == '$')
		{
			nextCode = text.begin();
			advance(nextCode,x);
			string::iterator end = nextCode
				,codeStart = nextCode;
			advance(codeStart,1);
			if((x+3) >= text.length())
			{
				// Not enough characters left to make a code.
				return codeStartPositions;
			}
			advance(end,4);
			if(ThreeDigitSequence(codeStart,end))
				codeStartPositions.push_back(x);
		}
	}
	// Note: All codes locations found include the leading $.
	return codeStartPositions;
}

bool ThreeDigitSequence(string::iterator start, string::iterator end)
{
	int count = 0;
	while((count < 3) && (start != end))
	{
		if(isdigit(*start))
			++count;
		else
			return false;
		++start;
	}
	return count == 3;
}

CodeFinder<>::CodePhraseVector RemoveCodes(CodeFinder<>::PhraseLocationVector::iterator e)
{
	string text = e->first;
	CodeFinder<>::CodePhraseVector codesAndPhrases;
	CodeFinder<>::LocationVector locations = e->second;
	CodeFinder<>::LocationVector::reverse_iterator it = locations.rbegin()
		,end = locations.rend();
	for(;it != end; it++)
	{
		string remainingPhrase;
		string toRemove;
		string::iterator strIt= text.begin()
			,strEnd;
		advance(strIt,*it);
		strEnd = strIt;
		advance(strEnd,4);
		copy(strIt,strEnd,inserter(toRemove,toRemove.begin()));
		copy(strEnd,text.end(),inserter(remainingPhrase,remainingPhrase.begin()));
		CodeFinder<string>::CodePhrasePair next(toRemove,remainingPhrase);
		codesAndPhrases.push_back(next);
		text.erase(strIt,text.end());
	}
	if(text.length() != 0)
	{
		// Remaining text will have no code (means use whatever the default color is for the console)
		CodeFinder<>::CodePhrasePair final("",text);
		codesAndPhrases.push_back(final);
	}

	return codesAndPhrases;
}
