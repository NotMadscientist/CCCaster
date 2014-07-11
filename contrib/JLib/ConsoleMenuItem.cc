// Title: Jeff Benson's Console Library
// File: ConsoleMenuItem.cpp
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
#include "ConsoleMenuItem.h"
using namespace std;

ConsoleMenuItem::ConsoleMenuItem()
	:m_data(MenuItemPair("",0))
{
}
ConsoleMenuItem::ConsoleMenuItem(const ConsoleMenuItem& rhs)
	:m_data(rhs.m_data)
{
}

ConsoleMenuItem::ConsoleMenuItem(const string& text)
	:m_data(MenuItemPair(text,0))
{
}

ConsoleMenuItem::ConsoleMenuItem(const string& text, DWORD value)
	:m_data(MenuItemPair(text,value))
{
}

string ConsoleMenuItem::Text() const
{
	return m_data.first;
}
string ConsoleMenuItem::Text(const std::string& text)
{
	string old = m_data.first;
	m_data.first = text;
	return old;
}
DWORD ConsoleMenuItem::Value() const
{
	return m_data.second;
}
DWORD ConsoleMenuItem::Value(DWORD value)
{
	DWORD old = m_data.second;
	m_data.second = value;
	return old;
}

ConsoleMenuItem& ConsoleMenuItem::operator=(const ConsoleMenuItem& rhs)
{
	if(this != &rhs)
		m_data = rhs.m_data;
	return *this;
}

bool operator==(const ConsoleMenuItem& lhs, const ConsoleMenuItem& rhs)
{
	if(&lhs == &rhs)
		return true;
	else if(lhs.Text() != rhs.Text())
		return false;
	else if(lhs.Value() != rhs.Value())
		return false;
	else
		return true;
}
