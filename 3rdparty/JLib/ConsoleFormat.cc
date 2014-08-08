// Title: Jeff Benson's Console Library
// File: ConsoleFormat.cpp
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
#include "ConsoleFormat.h"
#include <cstdlib>
using namespace std;

ConsoleFormat::ConsoleFormat()
	:m_color(ConsoleFormat::BLACK)
{
}

ConsoleFormat::ConsoleFormat(const ConsoleFormat& rhs)
	:m_color(rhs.m_color)
{
}

ConsoleFormat::ConsoleFormat(unsigned char bits)
	:m_color(bits)
{
}

ConsoleFormat::ConsoleFormat(const string& bits)
	:m_color(0)
{
	if(bits.length())
		m_color = atoi(bits.c_str());
}
void ConsoleFormat::Set(Bit bit, bool value)
{
	if(BitValid(bit))
		m_color |= ( unsigned ( value ) << bit );
	else
		throw "bit out of range";
}

bool ConsoleFormat::Get(Bit bit) const
{
	if(BitValid(bit))
		return ( m_color & ( 1u << bit ) );
	return false;
}

unsigned char ConsoleFormat::Color() const
{
	return (unsigned char)m_color;
}

void ConsoleFormat::Color(unsigned char bits)
{
	m_color = bits;
}

ConsoleFormat& ConsoleFormat::operator=(const ConsoleFormat& rhs)
{
	if(this != &rhs)
		m_color = rhs.m_color;
	return *this;
}

ConsoleFormat operator|(const ConsoleFormat& rhs, const ConsoleFormat& lhs)
{
	return ConsoleFormat(rhs.Color() | lhs.Color());
}

ConsoleFormat operator~(const ConsoleFormat& rhs)
{
	return ConsoleFormat(~rhs.Color());
}

bool ConsoleFormat::BitValid(Bit bit) const
{
	if((bit >= 0) && (bit < NUMBER_OF_BITS))
		return true;
	return false;
}
