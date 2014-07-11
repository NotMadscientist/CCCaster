// Title: Jeff Benson's Console Library
// File: ConsoleCore.h
// Author: Jeff Benson
// Date: 6/26/03
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
#ifndef _PRAGMA_ONCE_CONSOLECORE_H_
#define _PRAGMA_ONCE_CONSOLECORE_H_
#include <windows.h>
#include <string>
#include <conio.h>
#include <vector>

#include "ConsoleDefinitions.h"
#include "ConsoleCore.h"
#include "CharacterBox.h"
#include "ConsoleFormat.h"
#include "ConsoleMenu.h"

//		ConsoleCore
//	This class communicates with the standard output handle for consoles.
//	It allows for colored output, input, clearing the screen, and
//	saving/loading the screen.
//	ConsoleCore is a singleton.  ConsoleCore is not thread safe.
class ConsoleCore
{
public:
	//		GetInstance
	//	gets the console core
	//	Returns: The singleton.
	static ConsoleCore* GetInstance();

	//		~ConsoleCore
	//	Clears the screen, destroys the save screen buffer, and deletes the singleton.
	~ConsoleCore();

	//		ClearScreen
	//	Removes all formatting and characters from the screen.
	void ClearScreen();

	//		SaveScreen
	//	Copies the contents of the entire screen into an internal buffer.
	//  The dimensions of the buffer are defined by SCREEN_BUFFER_SIZE.
	void SaveScreen();

	//		SaveScreen
	//	Copies a rectangular area of the screen into a user defined buffer.
	//	Saving portions of the screen outside of MAX_SCREEN_X and MAX_SCREEN_Y
	//	is undefined.
	//	Arguments:
	//		PCHAR_INFO buffer:	User defined CHAR_INFO buffer.
	//		COORD bufferSize:	Dimensions of the buffer.
	//		COORD saveOrigin:	The upper left corner of the area to copy from the screen.
	void SaveScreen(PCHAR_INFO buffer, COORD bufferSize, COORD saveOrigin);

	//		LoadScreen
	//	Copies the internal buffer to the screen.
	void LoadScreen();

	//		LoadScreen
	//	Copies a user defined buffer to the screen
	//	Arguments:
	//		PCHAR_INFO buffer:	User defined CHAR_INFO buffer.
	//		COORD bufferSize:	Dimensions of the buffer.
	//		COORD loadOrigin:	The upper left corner of the screen to begin the copy.
	void LoadScreen(PCHAR_INFO buffer, COORD bufferSize, COORD loadOrigin);

	//		Color
	//	Returns, and optionally sets, the default output color.  If the argument is
	//	NULL, the current color in use is returned.  If the argument is not
	//  NULL, the current color is replaced but also returned.
	//	Arguments:
	//		const ConsoleFormat* newFormat:	The color to change to.
	//	Returns: The color last in use.
	ConsoleFormat Color(const ConsoleFormat* newFormat = NULL);

	//		CursorPosition
	//	Returns, and optionally sets, the current cursor position.  If the argument is
	//	NULL, the current position is returned.  If the argument is not
	//  NULL, the current position is replaced but also returned.
	//	Arguments:
	//		PCOORD lpPosition:	The new position.
	//	Returns: The last position.
	COORD CursorPosition(PCOORD lpPosition = NULL);

	//		CursorPosition
	//	Sets the cursor position by x and y components.
	//	Arguments:
	//		SHORT x:	x component
	//		SHORT y:	y component
	//	Returns: The last position.
	COORD CursorPosition(SHORT x, SHORT y);

	//		Printn
	//	Writes a number to the screen and updates the cursor position.
	//	Arguments:
	//		int number:	The value to write.
	//		BOOL endline:	Whether to move the cursor down and all the way left after writing.
	//		ConsoleFormat* color:	The color to use.
	//		SHORT x:	Column to write to.
	//		SHORT y:	Row to write to.
	//	Notes:
	//		If color is NULL, the default color is used.
	//		If x or y are -1 the current x and/or y value is used.
	void Printn(int number, BOOL endLine = FALSE, ConsoleFormat* color = NULL, SHORT x = -1, SHORT y = -1);

	//		Printd
	//	Writes a double to the screen.
	//	Arguments:
	//		double number:	The value to write.
	//		int characterLength:	The length of the output.
	//		BOOL endline:	Whether to move the cursor down and all the way left after writing.
	//		ConsoleFormat* color:	The color to use.
	//		SHORT x:	Column to write to.
	//		SHORT y:	Row to write to.
	//	Notes:
	//		If color is NULL, the default color is used.
	//		If character length is shorter than what it would take to display the entire double
	//		then the value will be displayed in scientific notation. i.e 9e-004 (or 0.0009).
	//		If x or y are -1 the current x and/or y value is used.
	void Printd(double number, int characterLength, BOOL endLine = FALSE, ConsoleFormat* color = NULL, SHORT x = -1, SHORT y = -1);

	//		Prints
	//	Writes a string to the screen and updates the cursor position.
	//	Arguments:
	//		string text:	The string to write.  Can contain embedded color codes.
	//		BOOL endline:	Whether to move the cursor down and all the way left after writing.
	//		ConsoleFormat* color:	The color to use.
	//		SHORT x:	Column to write to.
	//		SHORT y:	Row to write to.
	//	Notes:
	//		If color is NULL, the default color is used.
	//		If x or y are -1 the current x and/or y value is used.
	//		Text can contain color codes to format portions of the screen.
	//		Color codes are always in the format of a dollar sign followed by exactly 3 digits (i.e. $007)
	//		All text after a color code will be written to the screen in that color until another color
	//		code is encountered.
	//		Color codes supersede the current default color but do not modify it.
	void Prints(const std::string& text, BOOL endLine = FALSE, const ConsoleFormat* color = NULL, SHORT x = -1, SHORT y = -1);

	//		EndLine
	//	Moves the cursor down 1 row and all the way to the left.
	void EndLine();
	//		AdvanceCursor
	// Advances the cursor position.
	// Notes:
	//		The cursor position may wrap from right to left, bottom to top.
	void AdvanceCursor(SHORT length);

	//		Wait
	// Hangs program execution until any key is pressed, returns false if ESCAPE_KEY was pressed
	bool Wait();

	//		ScanNumber
	//	Reads input as a number from the keyboard.
	//	Arguments:
	//		COORD origin:	Where the prompt is placed.
	//		LPDOWRD lpNumber:	Out parameter to collect the input.
	//		int digitCount:	Maximum number of digits allowed.
	//	Notes:
	//		The minus sign can be used as the first character and does not count toward the limit
	//		set by digitCount.
	//		The range value of a DWORD is [-2147483648,2147483647]. If you enter a digit above
	//		or below that value, say 99999999999 to -9999999999, it will be clamped
	//		automatically before given back through lpNumber.
	bool ScanNumber(COORD origin, int& number, int digitCount = 10, bool allowNegative = true, bool hasDefault = true, int minDigit = 0, int maxDigit = 9);

	//		ScanString
	//	Reads input as a string from the keyboard.
	//	Arguments:
	//		COORD origin:	Where the prompt is placed.
	//		char* buffer:	Out parameter to collect the input.
	//		UINT maxLength:	Maximum amount of characters to be entered.
	//	Notes:
	//		maxLength should be 1 less than the maximum capacity of
	//	buffer.
	bool ScanString(COORD origin, std::string& buffer, UINT width);

	//		ScanDouble
	//	Reads input as a double from the keyboard
	//	Arguments:
	//		COORD origin: Where the prompt is placed.
	//		DOUBLE* pDouble:	Out parameter to collect the input.
	//	Notes:
	//		Can be entered as a typical floating point number (12.345)
	//	or as scientic notation (3.2e-4).
	//  The range of a double, according to MSDN 2005 documentation,
	//     is 2.2250738585072014 E – 308 to 1.7976931348623158 E + 308.
	// If you enter a digit outside of the range you may not get the desired result.
	// Additionally, because FPUs are fickle
	// the number you enter may not always be the precisely the one
	// that is returned through the out parameter.
	bool ScanDouble(COORD origin, double& number);

private:
	//		ConsoleCore
	//	Gets a handle to STD_OUT, creates an internal buffer for
	//	saving and loading the screen, and gets information about the buffer.
	//	Sets the cursor position to (0,0) and clears the screen.
	ConsoleCore();
	//		ConsoleCore
	//	Copies are not allowed.
	ConsoleCore(const ConsoleCore& rhs);
	//		ConsoleCore
	//	Assignment is not allowed.
	ConsoleCore& operator=(const ConsoleCore& rhs);


	//		_Prints
	//	This method actually does the output.  All other Print functions ultimately
	//	make calls to this method.
	//	Arguments:
	//		string text:	What to write.  Can be color coded.
	//		BOOL endLine:	Whether to end the line.
	//		const ConsoleFormat* color:	The color to use.
	//		SHORT x:	The column to write to.
	//		SHORT y:	The row to write to.
	void _Prints(const std::string& text, BOOL endLine, const ConsoleFormat* color, SHORT x, SHORT y);

	static ConsoleCore* m_theOnlyInstance;

	std::vector<CHAR_INFO> m_screenBuffer;			// For saving/loading the screen
	HANDLE m_consoleHandle;							// Handle to STD_OUT
	CONSOLE_SCREEN_BUFFER_INFO m_csbi;				// Used for clearing the screen.
	COORD m_cursorPosition;							// Where the cursor is.
	ConsoleFormat m_currentFormat;					// What format will be used when none is specified.

	void UpdateWindowSize();
};
#endif
