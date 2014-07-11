// Title: Jeff Benson's Console Library
// File: CharacterBox.h
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
#ifndef _PRAGMA_ONCE_CHARACTERBOX_H_
#define _PRAGMA_ONCE_CHARACTERBOX_H_
#include "ConsoleDefinitions.h"
#include "ConsoleFormat.h"
#include <windows.h>

//		CharacterBox
//	A class that uses the ConsoleCore to render an ASCII box to the screen.
//  You can specify what color formatting to use for the border and the client.
//  You can set the character that is used to draw the outline of the box.
class CharacterBox
{
public:
	//		CharacterBox
	//	Initializes an empty box with 0 dimensions.
	CharacterBox();

	//		CharacterBox
	//	Copies a character box.
	//	Arguments:
	//		const CharacterBox& rhs:	What is being copied.
	CharacterBox(const CharacterBox& rhs);

	//		CharacterBox
	//	Creates a character box.
	//	Arguments:
	//		COORD upperLeft:	The origin of the box.
	//		COORD lowerRight:	The opposite corner of the origin.
	//		ConsoleFormat border:	The coloring to use on the bounding area of the box.
	//		ConsoleFormat client:	The coloring to use on the inside of the box.
	//		char fill:	The character to use for the borders of the box.
	//	Notes:
	//		This function calls Normalize which ensures that the components of upperLeft
	//		and lowerRight are within range of MAX_SCREEN_X, MAX_SCREEN_Y, and 0.
	CharacterBox(COORD upperLeft, COORD lowerRight
		,ConsoleFormat border = ConsoleFormat::SYSTEM
		,ConsoleFormat client = ConsoleFormat::BLACK
		,char fill = '*');

	//		UpperLeft
	//	Gets the origin.
	//	Returns: The origin of the box.
	COORD UpperLeft() const;

	//		UpperLeft
	//	Sets the origin.
	//	Arguments:
	//		COORD ul: The new origin
	//	Returns: the old origin.
	COORD UpperLeft(COORD ul);

	//		LowerRight
	//	Gets the opposite corner from the origin.
	//	Returns: The lower right corner.
	COORD LowerRight() const;

	//		LowerRight
	//	Sets the lower right corner.
	//	Arguments:
	//		COORD lr:	The new corner.
	//	Returns: the old corner.
	COORD LowerRight(COORD lr);

	//		Fill
	//	Gets the fill character
	//	Returns: the fill character.
	char Fill() const;

	//		Fill
	//	Sets the fill character
	//	Arguments:
	//		char fill:	The character used to outline the box.
	// Returns: the old fill character.
	char Fill(char fill);

	//		BorderColor
	//	Gets the color used to draw the outline of the box.
	//	Returns: the old color.
	const ConsoleFormat& BorderColor() const;

	//		BorderColor
	//	Sets the color used to draw the outline of the box.
	//	Arguments:
	//		ConsoleFormat border:	The new color.
	//	Returns: the old color.
	ConsoleFormat BorderColor(ConsoleFormat border);

	//		ClientColor
	//	Gets the color used to draw the inside of the box.
	//	Returns: the old color.
	const ConsoleFormat& ClientColor() const;

	//		ClientColor
	//	Sets the color used to draw the inside of the box.
	//	Arguments:
	//		ConsoleFormat client:	The new color.
	//	Returns: the old color.
	ConsoleFormat ClientColor(ConsoleFormat client);

	//		operator=
	//	Assigns one CharacterBox to this CharacteBox
	//	Arguments:
	//		const CharacterBox& rhs:	What to copy
	//	Return:	this character box (for cascading).
	CharacterBox& operator=(const CharacterBox& rhs);

	//		Normalize
	//	Ensures the upper left and lower right coordinats are withing range of
	//	MAX_SCREEN_X, MAX_SCREEN_Y, and 0.
	//	Notes:
	//		Does not ensure that the upper left is above and left of lower right.
	void Normalize();

	//		Width
	//	Returns: The distance between upper left and lower right with respect to the horizontal.
	SHORT Width() const;

	//		Height
	//	Returns: The distance between upper left and lower right with respect to y vertical.
	SHORT Height() const;

	//		Draw
	//	Draws the box to the screen using ConsoleCore.
	virtual void Draw() const;

	//		Draw
	//	Draws a box.
	//	Arguments:
	//		COORD upperLeft:	The origin.
	//		COORD lowerRight:	Opposite corner of the origin.
	//		char fill:	The character to use for the border.
	//	Notes:
	//		This method uses whatever formatting the ConsoleCore is currently using.
	static void Draw(COORD upperLeft, COORD lowerRight, char fill = '*');
private:
	COORD m_upperLeft		// origin
		,m_lowerRight;		// opposite origin
	char m_fill;			// Outline character
	ConsoleFormat m_border	// Format used on the outline
		,m_client;			// Format used for interior.
};

//		CharacterWindow
//	A CharacterBox with a title.  It has the same properties as CharacterBox
//	but it is drawn differently.  The title portion is encased in its own
//	box.  The title is formated with the client format.
//	You must make sure to make the dimensions of the box large enough
//	to encase the title or the title can overrun the box.
class CharacterWindow : public CharacterBox
{
public:
	//		CharacterWindow
	//	Initializes a window with no title.
	CharacterWindow();
	//		CharacterWindow
	//	Copies a window.
	//	Arguments:
	//		const CharacterWindow& rhs:	What to copy.
	CharacterWindow(const CharacterWindow& rhs);

	//		CharacterWindow
	//	Creates a character window.
	//	Arguments:
	//		COORD upperLeft:	The origin of the box.
	//		COORD lowerRight:	The opposite corner of the origin.
	//		ConsoleFormat border:	The coloring to use on the bounding area of the box.
	//		ConsoleFormat client:	The coloring to use on the inside of the box.
	//		char fill:	The character to use for the borders of the box.
	//	Notes:
	//		This function invokes the parents creation method which
	//		also causes Normalize to be called.
	CharacterWindow(COORD upperLeft, COORD lowerRight, string title
		,ConsoleFormat border = ConsoleFormat::SYSTEM
		,ConsoleFormat client = ConsoleFormat::BLACK
		,char fill = '*');

	//		Title
	//	Get the title of the window.
	//	Returns: the title.
	string Title() const;

	//		Title
	//	Sets the title of the window.
	//	Arguments:
	//		string title:	The title text.
	void Title(string title);

	//		Draw
	//	Draws the window.
	//	Notes:
	//		Creates two CharacterBoxes, draws them, and then draws the title.
	virtual void Draw() const;
private:
	string m_title;
};
#endif