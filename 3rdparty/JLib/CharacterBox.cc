// Title: Jeff Benson's Console Library
// File: CharacterBox.cpp
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
#include "CharacterBox.h"
#include "ConsoleCore.h"
using namespace std;

CharacterBox::CharacterBox()
	:m_fill('*')
	,m_border(ConsoleFormat::SYSTEM)
	,m_client(ConsoleFormat::BLACK)
{
	m_upperLeft.X = m_lowerRight.X = m_upperLeft.Y = m_lowerRight.Y = 0;
}

CharacterBox::CharacterBox(const CharacterBox& rhs)
	:m_upperLeft(rhs.m_upperLeft)
	,m_lowerRight(rhs.m_lowerRight)
	,m_fill(rhs.m_fill)
	,m_border(rhs.m_border)
	,m_client(rhs.m_client)
{
}

CharacterBox::CharacterBox(COORD upperLeft, COORD lowerRight
		,ConsoleFormat border
		,ConsoleFormat client
		,char fill)
	:m_upperLeft(upperLeft)
	,m_lowerRight(lowerRight)
	,m_fill(fill)
	,m_border(border)
	,m_client(client)
{
	Normalize();
}


COORD CharacterBox::UpperLeft() const
{
	return m_upperLeft;
}
COORD CharacterBox::UpperLeft(COORD ul)
{
	COORD old = m_upperLeft;
	m_upperLeft = ul;
	Normalize();
	return old;
}
COORD CharacterBox::LowerRight() const
{
	return m_lowerRight;
}
COORD CharacterBox::LowerRight(COORD lr)
{
	COORD old = m_lowerRight;
	m_lowerRight = lr;
	Normalize();
	return old;
}
char CharacterBox::Fill() const
{
	return m_fill;
}
char CharacterBox::Fill(char fill)
{
	char old = m_fill;
	m_fill = fill;
	return old;
}
const ConsoleFormat& CharacterBox::BorderColor() const
{
	return m_border;
}
ConsoleFormat CharacterBox::BorderColor(ConsoleFormat border)
{
	ConsoleFormat old = m_border;
	m_border = border;
	return old;
}
const ConsoleFormat& CharacterBox::ClientColor() const
{
	return m_client;
}
ConsoleFormat CharacterBox::ClientColor(ConsoleFormat client)
{
	ConsoleFormat old = m_client;
	m_client = client;
	return old;
}
CharacterBox& CharacterBox::operator=(const CharacterBox& rhs)
{
	if(this != &rhs)
	{
		m_upperLeft = rhs.m_upperLeft;
		m_lowerRight = rhs.m_lowerRight;
		m_fill = rhs.m_fill;
		m_border = rhs.m_border;
		m_client = rhs.m_client;
	}
	return *this;
}

void CharacterBox::Normalize()
{
    // Get the actual console window size
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    MAXSCREENX = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    MAXSCREENY = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    // Clamp values to console window size
    if(m_upperLeft.X > MAXSCREENX)
        m_upperLeft.X = MAXSCREENX;
    else if(m_upperLeft.X < 0)
        m_upperLeft.X = 0;

    if(m_upperLeft.Y > MAXSCREENY)
        m_upperLeft.Y = MAXSCREENY;
    else if(m_upperLeft.Y < 0)
        m_upperLeft.Y = 0;

    if(m_lowerRight.X > MAXSCREENX)
        m_lowerRight.X = MAXSCREENX;
    else if(m_lowerRight.X < 0)
        m_lowerRight.X = 0;

    if(m_lowerRight.Y > MAXSCREENY)
        m_lowerRight.Y = MAXSCREENY;
    else if(m_lowerRight.Y < 0)
        m_lowerRight.Y = 0;
}

SHORT CharacterBox::Width() const
{
	return m_lowerRight.X - m_upperLeft.X;
}

SHORT CharacterBox::Height() const
{
	return m_lowerRight.Y - m_upperLeft.Y;
}

#define FAST_BOX(UL, LR, FILL)                                 \
    ConsoleCore* pCore = ConsoleCore::GetInstance();           \
    COORD size = {LR.X - UL.X, LR.Y - UL.Y};                   \
    PCHAR_INFO buffer = new CHAR_INFO[size.X * size.Y];        \
    pCore->SaveScreen(buffer, size, UL);                       \
    for (int x = 0; x < size.X; x++)                           \
    {                                                          \
        buffer[x].Char.AsciiChar = FILL;                       \
        buffer[x].Attributes = 0x7;                            \
        buffer[x + (size.Y-1) * size.X].Char.AsciiChar = FILL; \
        buffer[x + (size.Y-1) * size.X].Attributes = 0x7;      \
    }                                                          \
    for (int y = 1; y < size.Y-1; y++)                         \
    {                                                          \
        buffer[y * size.X].Char.AsciiChar = FILL;              \
        buffer[y * size.X].Attributes = 0x7;                   \
        buffer[(size.X-1) + y * size.X].Char.AsciiChar = FILL; \
        buffer[(size.X-1) + y * size.X].Attributes = 0x7;      \
    }                                                          \
    for (int x = 1; x < (size.X-1); x++)                       \
    {                                                          \
        for (int y = 1; y < (size.Y-1); y++)                   \
        {                                                      \
            buffer[x + y * size.X].Char.AsciiChar = ' ';       \
            buffer[x + y * size.X].Attributes = 0x7;           \
        }                                                      \
    }                                                          \
    pCore->LoadScreen(buffer, size, UL);                       \
    delete[] buffer

void CharacterBox::Draw() const
{
    /*
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	int x
		,y;
	int xRange = m_lowerRight.X - m_upperLeft.X
		,yRange = m_lowerRight.Y - m_upperLeft.Y;
	char buffer[2] = {m_fill,'\0'};
	ConsoleFormat oldFormat = pCore->Color(&m_border);
	for(x = 0; x < xRange; x++)
	{
		pCore->Prints(buffer,FALSE,&m_border,m_upperLeft.X + x,m_upperLeft.Y);
		pCore->Prints(buffer,FALSE,&m_border,m_upperLeft.X + x,m_lowerRight.Y-1);
	}
	for(y = 1; y < yRange; y++)
	{
		pCore->Prints(buffer,FALSE,&m_border,m_upperLeft.X, m_upperLeft.Y + y);
		pCore->Prints(buffer,FALSE,&m_border,m_lowerRight.X-1, m_upperLeft.Y + y);
	}
	pCore->Color(&m_client);
	for(x = 1; x < (xRange-1); x++)
	{
		for(y = 1; y < (yRange-1); y++)
			pCore->Prints(" ",FALSE,&m_client,m_upperLeft.X + x, m_upperLeft.Y + y);
	}
	pCore->Color(&oldFormat);
    */
    FAST_BOX(m_upperLeft, m_lowerRight, m_fill);
}

void CharacterBox::Draw(COORD upperLeft, COORD lowerRight, char fill)
{
    /*
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	int x
		,y;
	int xRange = lowerRight.X - upperLeft.X
		,yRange = lowerRight.Y - upperLeft.Y;
	char buffer[2] = {fill,'\0'};
	for(x = 0; x < xRange; x++)
	{
		pCore->Prints(buffer,FALSE,NULL,upperLeft.X + x,upperLeft.Y);
		pCore->Prints(buffer,FALSE,NULL,upperLeft.X + x,lowerRight.Y-1);
	}
	for(y = 1; y < yRange; y++)
	{
		pCore->Prints(buffer,FALSE,NULL,upperLeft.X, upperLeft.Y + y);
		pCore->Prints(buffer,FALSE,NULL,lowerRight.X-1, upperLeft.Y + y);
	}
	for(x = 1; x < (xRange-1); x++)
	{
		for(y = 1; y < (yRange-1); y++)
			pCore->Prints(" ",FALSE,NULL ,upperLeft.X + x, upperLeft.Y + y);
	}
    */
    FAST_BOX(upperLeft, lowerRight, fill);
}

///////////////////
CharacterWindow::CharacterWindow()
	:CharacterBox()
{}
CharacterWindow::CharacterWindow(const CharacterWindow& rhs)
	:CharacterBox(rhs)
	,m_title(rhs.m_title)
{}
CharacterWindow::CharacterWindow(COORD upperLeft, COORD lowerRight
		,const string& title
		,ConsoleFormat border, ConsoleFormat client
		,char fill)
		:CharacterBox(upperLeft, lowerRight, border, client, fill)
		,m_title(title)
{
}

const string& CharacterWindow::Title() const
{
	return m_title;
}
void CharacterWindow::Title(const string& title)
{
	m_title = title;
}
void CharacterWindow::Draw() const
{
	COORD ulTitleBox = UpperLeft(),
		brTitleBox = {LowerRight().X,UpperLeft().Y+3},
		ulClientBox = {UpperLeft().X,UpperLeft().Y+2},
		brClientBox = LowerRight();
	COORD titlePosition = {UpperLeft().X+1,UpperLeft().Y+1};
	CharacterBox titleBox(ulTitleBox,brTitleBox,BorderColor(),ClientColor(),Fill())
		,clientBox(ulClientBox,brClientBox,BorderColor(),ClientColor(),Fill());
	titleBox.Draw();
	clientBox.Draw();
	if(m_title.size())
		ConsoleCore::GetInstance()->Prints(m_title,FALSE,&ClientColor(),titlePosition.X,titlePosition.Y);
}
