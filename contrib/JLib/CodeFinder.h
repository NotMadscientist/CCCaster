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
#ifndef __PRAGMAONCE_CODEFINDER_H__
#define __PRAGMAONCE_CODEFINDER_H__

#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <iterator>

typedef std::pair<std::string, unsigned> PhraseColorPair;

std::vector<unsigned> FindCodeLocations(std::string text);
bool ThreeDigitSequence(std::string::iterator start, std::string::iterator end);

// A function object used with for_each to extract locations
// of color codes and phrases in a string.
template <class Arg1 = std::string>
struct CodeFinder
{
	typedef std::pair<Arg1, Arg1> CodePhrasePair;
	typedef std::vector<CodePhrasePair> CodePhraseVector;
	typedef std::vector<unsigned> LocationVector;
	typedef std::pair<Arg1, LocationVector> PhraseLocationPair;
	typedef std::vector<PhraseLocationPair> PhraseLocationVector;

	PhraseLocationVector m_phrasesAndLocations;

	void operator()(Arg1 text)
	{
		PhraseLocationPair codes (text, FindCodeLocations(text));
		m_phrasesAndLocations.push_back(codes);
	}

	void WriteCodes()
	{
		typename PhraseLocationVector::iterator it = m_phrasesAndLocations.begin()
			, end = m_phrasesAndLocations.end();
		for( ; it != end; ++it )
		{
			std::cout << "Text: " << it->first << std::endl;
			std::cout << "Codes Origins: ";

			std::copy(it->second.begin(), it->second.end(), std::ostream_iterator<unsigned>(std::cout, " "));
			std::cout << std::endl;
		}
	}
};

//		RemoveCodes
//	Breaks a string into a pair of strings, one for the color code and one for the text
//	to be colored.
CodeFinder<>::CodePhraseVector RemoveCodes(CodeFinder<>::PhraseLocationVector::iterator e);

#endif
