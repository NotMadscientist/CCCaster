// Title: Jeff Benson's Console Library
// File: ConsoleFormat.h
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
#ifndef _PRAGMA_ONCE_CONSOLEFORMAT_H_
#define _PRAGMA_ONCE_CONSOLEFORMAT_H_

#include <string>

//		ConsoleFormat
//	Contains a bitset of 8 bits that represents the coloring used
//	when writing to the screen.  The bits for coloring, and their
//	order, can be found in the Bit enumeration.  Also provides
//	some predefined colors that are common.
class ConsoleFormat
{
public:
	//	The locations for each part of a format's color in the bitset.
	enum Bit
	{
		FRONT_ALPHA = 0
		, FRONT_RED
		, FRONT_GREEN
		, FRONT_BLUE
		, BACK_ALPHA
		, BACK_RED
		, BACK_GREEN
		, BACK_BLUE
		, NUMBER_OF_BITS
	};

	// Values for commonly used colors.
	enum Colors
	{
		BLACK,
		BLUE,
		GREEN,
		CYAN,
		RED,
		PURPLE,
		YELLOW,
		SYSTEM,	//This is the standard format of a command window.
		//It is called SYSTEM because WHITE is reserved
		//to be the opposite of black on black.
		GREY,
		BRIGHTBLUE,
		BRIGHTGREEN,
		BRIGHTCYAN,
		BRIGHTRED,
		BRIGHTPURPLE,
		BRIGHTYELLOW,
		BRIGHTWHITE,
		// Foreground colors
		ONBLUE = 16,
		ONGREEN = 32,
		ONCYAN = 48,
		ONRED = 64,
		ONPURPLE = 80,
		ONYELLOW = 96,
		ONSYSTEM = 112,
		ONGREY = 128,
		ONBRIGHTBLUE = 144,
		ONBRIGHTGREEN = 160,
		ONBRIGHTCYAN = 176,
		ONBRIGHTRED = 192,
		ONBRIGHTPURPLE = 208,
		ONBRIGHTYELLOW = 224,
		ONBRIGHTWHITE = 240,
		// Background colors
		WHITE = 255
	};

public:
	//		ConsoleFormat
	//	Creates a black on black format.
	ConsoleFormat();

	//		ConsoleFormat
	//	Copies a ConsoleFormat
	//	Arguments:
	//		const ConsoleFormat& rhs:	What to copy.
	ConsoleFormat(const ConsoleFormat& rhs);

	//		ConsoleFormat
	//	Converts an unsigned character into a ConsoleFormat
	//	Arguments:
	//		unsigned char bits:	the color value.
	ConsoleFormat(unsigned char bits);

	//		ConsoleFormat
	//	Converts a color code (string) into a ConsoleFormat
	//	Arguments:
	//		string bits:	A color code
	//	Notes:
	//		This method uses the unsecure version of atoi.
	//		Expect problems if you pass a string that atoi cannot handle...
	ConsoleFormat(const std::string& bits);

	//		Set
	//	Sets a bit
	//	Arguments:
	//		Bit bit:	Which bit to set
	//		bool value:	On or off
	void Set(Bit bit, bool value);

	//		Get
	//	Gets a bit
	//	Arguments:
	//		Bit bit:	Which bit to get
	//	Returns: on or off.
	bool Get(Bit bit) const;

	//		Color
	//	Gets the color bits
	//	Returns: the value
	unsigned char Color() const;

	//		Color
	//	Sets the color bits
	//	Arguments:
	//		unsigned char bits:	the value.
	void Color(unsigned char bits);

	//		operator=
	//	Assigns a ConsoleFormat to this.
	//	Arguments:
	//		const ConsoleFormat& rhs:	What to copy.
	//	Returns: this
	ConsoleFormat& operator=(const ConsoleFormat& rhs);
private:
	//		BitValid
	//	Ensures that Bit is not out of range of possible bits.
	//	Arguments:
	//		Bit bit:	Which bit.
	//	Returns: that the bit location exists.
	bool BitValid(Bit bit) const;

	// The color as a single byte
	unsigned char m_color;
};

ConsoleFormat operator|(const ConsoleFormat& rhs, const ConsoleFormat& lhs);
ConsoleFormat operator~(const ConsoleFormat& rhs);

#endif
