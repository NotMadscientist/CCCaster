// Title: Jeff Benson's Console Library
// File: ConsoleMenuItem.h
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
#ifndef _PRAGMA_ONCE_CONSOLEITEM_H_
#define _PRAGMA_ONCE_CONSOLEITEM_H_
#include <windows.h>
#include <string>
#include <utility>

//		ConsoleMenuItem
//	This represents an item in a menu.  Menu items have a text label
// and a userdefined value.  The user defined value is of type DWORD
// so you could potentially store something more meaningful than a
// number, like a function address.
class ConsoleMenuItem
{
public:
	//		ConsoleMenuItem
	//	Creates an empty menu item.
	ConsoleMenuItem();

	//		ConsoleMenuItem
	//	Copies a menu item.
	//	Arguments:
	//		const ConsoleMenuItem& rhs:	what to copy.
	ConsoleMenuItem(const ConsoleMenuItem& rhs);

	//		ConsoleMenuItem
	//	Creates a menu item with 0 as the user defined value.
	//	Arguments:
	//		string text:	the menu item label.
	ConsoleMenuItem(const std::string& text);

	//		ConsoleMenuItem
	//	Creates a menu item.
	//	Arguments:
	//		string text:	the menu item label.
	//		DWORD value:	a user defined value.
	ConsoleMenuItem(const std::string& text, DWORD value);

	//		Text
	//	Gets the text of the item.
	//	Returns: the text of the item.
	std::string Text() const;

	//		Text
	//	Sets the text of the item.
	//	Arguments:
	//		string text:	the label.
	std::string Text(const std::string& text);

	//		Value
	//	Gets the user defined value of the item.
	//	Returns: the user defined value of the item.
	DWORD Value() const;

	//		Value
	//	Sets the user defined value of the item.
	//	Arguments:
	//		DWORD value:	a user defined value.
	//	Returns: the old value.
	DWORD Value(DWORD value);

	//		operator=
	//	Assigns a menu item to this.
	//	Arguments:
	//		const ConsoleMenuItem& rhs:	what to copy.
	//	Returns: this.
	ConsoleMenuItem& operator=(const ConsoleMenuItem& rhs);

private:
	typedef std::pair<std::string, DWORD> MenuItemPair;
	MenuItemPair m_data;
};

bool operator==(const ConsoleMenuItem& lhs, const ConsoleMenuItem& rhs);
#endif

