// Title: JLib Console Library test
// File: testharness.cpp
// Author: Jeff Benson
// Date: 4/1/2008
// Last Updated: 7/28/2011
// Contact: pepsibot@hotmail.com
//
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
//
//	Abstract:
//		A demonstration of the ConsoleCore and JLib.
//
#include "ConsoleCore.h"
#include <iostream>
using namespace std;

// Test functions
void WriteString();
void PrintNumber();
void Coloring();
void FormatOutput();
void ReadString();
void ReadNumber();
void DrawBox();
void DrawWindowTitle();
void DoMenu();
void DoWindowedMenu();
void DoScrollMenu();
void DoWindowedScrollMenu();
void CustomBuffers();
void ScanFloat();

// Function pointer typdef for easy conversion.
typedef void (*VOIDFUNCVOID)(void);

// The number of items in the menu
const unsigned numberOfMainMenuItems = 15;

// The menu item text labels
const char* mainMenuItems[numberOfMainMenuItems] =
{
	"1)Write a string"
	,"2)Print a number"
	,"3)Color test"
	,"4)Formatting Output"
	,"5)Read a string"
	,"6)Read a number"
	,"7)Draw a char box"
	,"8)Draw a window with a title"
	,"9)Standard Menu"
	,"10)Windowed Menu"
	,"11)Scrolling Menu"
	,"12)Windowed Scrolling Menu"
	,"13)Custom Screen Buffers"
	,"14)Read a Float"
	,"15)Quit (or press Escape)"
};

// The menu item id values are function
// addresses that all have the signature
// defined by VOIDFUNCVOID.
DWORD menuItemValues[numberOfMainMenuItems] =
{
	(DWORD)WriteString
	,(DWORD)PrintNumber
	,(DWORD)Coloring
	,(DWORD)FormatOutput
	,(DWORD)ReadString
	,(DWORD)ReadNumber
	,(DWORD)DrawBox
	,(DWORD)DrawWindowTitle
	,(DWORD)DoMenu
	,(DWORD)DoWindowedMenu
	,(DWORD)DoScrollMenu
	,(DWORD)DoWindowedScrollMenu
	,(DWORD)CustomBuffers
	,(DWORD)ScanFloat
	,(DWORD)numberOfMainMenuItems
};

// Initial window dimensions
int MAXSCREENX = 80;
int MAXSCREENY = 25;

int main(int argc, char *argv[])
{
	UINT x = 0;
	COORD menuOrigin = {0,0};
	ConsoleFormat defaultColor = ConsoleFormat::BRIGHTWHITE;
	ConsoleCore::GetInstance()->Color(&defaultColor);
	WindowedMenu menu(menuOrigin,numberOfMainMenuItems,"JLib Demo",ConsoleFormat::BRIGHTRED);
	for(x = 0; x < numberOfMainMenuItems; x++)
		menu.Append(mainMenuItems[x],menuItemValues[x]);
	do
	{
		ConsoleCore::GetInstance()->ClearScreen();
		switch(menu.Show())
		{
			case (USERESC):
				return 0;
			case (BADMENU):
				ConsoleCore::GetInstance()->Prints("BAD MENU");
				ConsoleCore::GetInstance()->Wait();
				return 0;
			default:
				{
					VOIDFUNCVOID pFunc = (VOIDFUNCVOID)menu.SelectedValue();
					if((unsigned)pFunc == numberOfMainMenuItems)
						return 0;
					pFunc();
				}
				break;
		};
	}
	while(TRUE);
}

void WriteString()
{
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	ConsoleFormat ccf(ConsoleFormat::BRIGHTYELLOW | ConsoleFormat::ONBLUE),
					  ccfOld;
	COORD ul = {0,0}, br = {54,4};
	ConsoleFormat border(ConsoleFormat::BRIGHTYELLOW)
		,client(ConsoleFormat::ONBLUE);
	CharacterBox box(ul,br,border,client);
	box.Draw();

	ccfOld = pCore->Color();
	pCore->Prints("Hello World!",TRUE,&ccf,1,1);
	pCore->Prints("Press any key to return to the main menu.",FALSE,&ccf,1,2);
	pCore->Color(&ccfOld);

	pCore->Wait();
}

void PrintNumber()
{
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	ConsoleFormat ccfOld
				 ,border = ConsoleFormat::BRIGHTYELLOW
				 ,client = ConsoleFormat::ONBLUE;
	ConsoleFormat inputColoring = (ConsoleFormat::BRIGHTYELLOW | ConsoleFormat::ONBLUE);
	COORD ul = COORD()
		,br = {43,4};
	CharacterBox box(ul,br,border,client);
	DWORD number = 12;
	box.Draw();
	ccfOld = pCore->Color();
	pCore->Printn(number,FALSE,&inputColoring,1,1);
	pCore->Prints("Press any key to return to the main menu.",FALSE,&inputColoring,1,2);
	pCore->Color(&ccfOld);
	pCore->Wait();
}

void Coloring()
{
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	ConsoleFormat oldFormat;
	COORD ul = {6,2}
		 ,br = {71,20};
	CharacterBox box(ul,br,ConsoleFormat::BRIGHTWHITE);
	unsigned number = 0;
	SHORT x = 7,
		  y = 3;
	oldFormat = pCore->Color();
	pCore->ClearScreen();
	box.Draw();
	for(number = 0; number < 256; number++)
	{
		if(y == 19)
		{
			x += 4;
			y = 3;
		}
		ConsoleFormat color;
		color.Color(number);
		if(number < 10)
		{
			pCore->Printn(0,FALSE,&color,x,y);
			pCore->Printn(0);
			pCore->Printn(number);
		} else if(number < 100)
		{
			pCore->Printn(0,FALSE,&color,x,y);
			pCore->Printn(number);
		}
		else
			pCore->Printn(number,FALSE,&color,x,y);
		y++;
	}
	pCore->Color(&oldFormat);
	pCore->Prints("These are the palette indices for all color combinations.",FALSE,NULL,6,21);
	pCore->Prints("Use them with Prints(\"$###\",...) auto format syntax.",FALSE,NULL,6,22);
	pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,6,23);
	pCore->Wait();
}

void FormatOutput()
{
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	ConsoleFormat ccfString(ConsoleFormat::YELLOW | ConsoleFormat::ONBLUE),
					  ccfNumber(ConsoleFormat::BLUE | ConsoleFormat::ONYELLOW),
					  ccf(ConsoleFormat::BRIGHTYELLOW | ConsoleFormat::ONBLUE),
					  ccfOld,
					  red,
					  green,
					  blue;
	red.Set(ConsoleFormat::FRONT_RED,true);
	red.Set(ConsoleFormat::BACK_RED,false);
	green.Set(ConsoleFormat::FRONT_GREEN,true);
	green.Set(ConsoleFormat::BACK_GREEN,false);
	blue.Set(ConsoleFormat::FRONT_BLUE,true);
	blue.Set(ConsoleFormat::BACK_BLUE,false);
	COORD ul = COORD()
		,br = {49,7};
	CharacterBox box(ul,br,ConsoleFormat::BRIGHTYELLOW,ConsoleFormat::ONBLUE);
	ccfOld = pCore->Color();
	box.Draw();
	pCore->Prints("Hello World!",TRUE,&ccfString,1,1);
	pCore->Printn(1024,FALSE,&ccfNumber,1,2);
	pCore->Prints("Red ",FALSE,&red,1,3);
	pCore->Prints("Green ",FALSE,&green);
	pCore->Prints("Blue",FALSE,&blue);
	string text = "$012Bright Red $010Bright Green $009Bright Blue";
	pCore->Prints(text,FALSE,NULL,1,4);
	pCore->Color(&ccfOld);
	pCore->Wait();
}

void ReadString()
{
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	ConsoleFormat ccf(ConsoleFormat::BRIGHTYELLOW | ConsoleFormat::ONBLUE),
					  ccfOld;
	COORD scanPosition = {1,2}
		,ul = {0,0}
		,br = {43,6};
	CharacterBox box(ul,br,ConsoleFormat::BRIGHTYELLOW,ConsoleFormat::ONBLUE);
	box.Draw();
	ccfOld = pCore->Color(&ccf);
	pCore->Prints("Enter a string of up to 20 characters.",FALSE,NULL,1,1);
	pCore->CursorPosition(&scanPosition);
	string str;
	pCore->ScanString(pCore->CursorPosition(),str,21);
	pCore->Prints("You wrote: ",FALSE,NULL,1,3);
	pCore->Prints(str,TRUE,NULL);
	pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,1,4);
	pCore->Color(&ccfOld);
	pCore->Wait();
}

void ReadNumber()
{
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	ConsoleFormat ccf(ConsoleFormat::BRIGHTYELLOW | ConsoleFormat::ONBLUE),
					  ccfOld;
	COORD scanPosition = {1,2}
		,ul = {0,0}
		,br = {46,6};
	CharacterBox box(ul,br,ConsoleFormat::BRIGHTYELLOW,ConsoleFormat::ONBLUE);
	int num = 0;
	box.Draw();
	ccfOld = pCore->Color(&ccf);
	pCore->Prints("Enter a number of up to 10 digits in length.",TRUE,NULL,1,1);
	pCore->CursorPosition(&scanPosition);
	pCore->ScanNumber(pCore->CursorPosition(),num,10);
	pCore->Prints("You entered: ",FALSE,NULL,1,3);
	pCore->Printn(num,FALSE);
	pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,1,4);
	pCore->Color(&ccfOld);
	pCore->Wait();
}

void DrawBox()
{
	ConsoleFormat boxColor = ConsoleFormat::SYSTEM
		,old;
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	COORD upperLeft = COORD(),
		lowerRight = {MAXSCREENX - 1, MAXSCREENY - 1};
	char fill = '*';
	old = pCore->Color(&boxColor);
	CharacterBox::Draw(upperLeft, lowerRight, fill);
	pCore->Color(&old);
	pCore->Prints("A charbox encompassing the entire screen (79,24)",TRUE,NULL,1,21);
	pCore->Prints("Press any key to see the next example.",FALSE,NULL,1);
	pCore->Wait();
	ConsoleFormat ccfBorder = ConsoleFormat::BRIGHTWHITE
		,ccfClient = ~ConsoleFormat::BRIGHTWHITE;
	CharacterBox box = CharacterBox(upperLeft,lowerRight,ccfBorder,ccfClient,fill);
	box.Draw();
	pCore->Prints("A charbox encompassing the entire screen using specific client/borders",TRUE,NULL,1,21);
	pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,1);
	pCore->Wait();
}

void DrawWindowTitle()
{
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	const char* title = "example";
	COORD ul = {31,0}
		  ,br = {37,6};
	ConsoleFormat client = ConsoleFormat::BRIGHTYELLOW
		,old = pCore->Color();
	CharacterWindow window(ul,br,title);
	window.ClientColor(client);
	//console.DrawCharWindow(ul,br,title,'*');
	window.Draw();
	ul.X = 39;
	br.X = ul.X + strlen(title) + 2;
	window.UpperLeft(ul);
	window.LowerRight(br);
	// +1 for the null which is not counted and
	// +1 to make the right edge hug the last
	// letter of the title.
	window.Draw();
	ul.Y += 8;
	br.Y += 8;
	window.UpperLeft(ul);
	window.LowerRight(br);
	ConsoleFormat border = ConsoleFormat::BRIGHTRED | ConsoleFormat::ONBRIGHTPURPLE;
	client = ConsoleFormat::BRIGHTYELLOW | ConsoleFormat::ONBRIGHTCYAN;
	window.ClientColor(client);
	window.BorderColor(border);
	window.Draw();
	pCore->Color(&old);
	pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,0,23);
	pCore->Wait();
}

void DoMenu()
{
	COORD origin = {31,0};
	ConsoleFormat ccfRed = ConsoleFormat::BRIGHTRED
		,ccfOld;
	ConsoleMenu menu(origin);
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	menu.Append("Menu Item 1",0);
	menu.Append("Menu Item 2",0);
	menu.Append("Menu Item 3",0);
	do
	{
		switch(menu.Show())
		{
			case USERESC:
				ccfOld = pCore->Color(&ccfRed);
				pCore->Prints("Escape was pressed",FALSE,NULL,origin.X,origin.Y+5);
				pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,origin.X,origin.Y+6);
				pCore->Color(&ccfOld);
				pCore->Wait();
			return;
			case BADMENU:
				ccfOld = pCore->Color(&ccfRed);
				pCore->Prints("This menu sucks and cannot be shown",FALSE,NULL,origin.X,origin.Y+5);
				pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,origin.X,origin.Y+6);
				pCore->Color(&ccfOld);
				pCore->Wait();
			return;
			default:
				ConsoleMenuItem selection = menu.SelectedItem();
				pCore->Prints("You selected the item with the label: ",FALSE,NULL,origin.X,origin.Y+3);
				pCore->Prints(selection.Text());
				pCore->Prints("Press any key to restart the menu.",FALSE,NULL,origin.X,origin.Y+5);
				pCore->Wait();
			break;
		}
	}
	while(TRUE);
}


void DoWindowedMenu()
{
	ConsoleFormat ccfRed = ConsoleFormat::BRIGHTRED
		,ccfClient = ConsoleFormat::BRIGHTWHITE
					  ,ccfOld;
	COORD origin = {31,0};

	WindowedMenu menu(origin,4,"Wnd Menu");
	menu.WindowColor(ConsoleFormat::BRIGHTBLUE);
	menu.ClientColor(ConsoleFormat::ONYELLOW);
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	menu.Append("Menu Item 1",0);
	menu.Append("Menu Item 2",1);
	menu.Append("Menu Item 3",2);
	menu.Append("Menu Item 4",3);
	menu.Append("Menu Item 5",4);
	menu.Append("Menu Item 6",5);
	menu.Append("Menu Item 7",6);
	menu.Append("Menu Item 8",7);
	menu.Scrollable(FALSE);
	do
	{
		switch(menu.Show())
		{
			case USERESC:
				ccfOld = pCore->Color(&ccfRed);
				pCore->Prints("Escape was pressed",FALSE,NULL,origin.X,origin.Y+5);
				pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,origin.X,origin.Y+6);
				pCore->Color(&ccfOld);
				pCore->Wait();
			return;
			case BADMENU:
				ccfOld = pCore->Color(&ccfRed);
				pCore->Prints("This menu sucks and cannot be shown",FALSE,NULL,origin.X,origin.Y+5);
				pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,origin.X,origin.Y+6);
				pCore->Color(&ccfOld);
				pCore->Wait();
			return;
			default:
				ConsoleMenuItem selection = menu.SelectedItem();
				pCore->Prints("You selected the item with the label: ",FALSE,NULL,origin.X,origin.Y+3);
				pCore->Prints(selection.Text());
				pCore->Prints("Press any key to restart the menu.",FALSE,NULL,origin.X,origin.Y+5);
				pCore->Wait();
			break;
		}
	}
	while(TRUE);
}

void DoScrollMenu()
{
	ConsoleFormat ccfRed = ConsoleFormat::BRIGHTRED
					  ,ccfOld;
	COORD origin = {31,0};
	ScrollingMenu menu(origin,4,ccfRed,~ccfRed);
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	menu.Append("Menu Item 1",0);
	menu.Append("Menu Item 2",1);
	menu.Append("Menu Item 3",2);
	menu.Append("Menu Item 4",3);
	menu.Append("Menu Item 5",4);
	menu.Append("Menu Item 6",5);
	menu.Append("Menu Item 7",6);
	menu.Append("Menu Item 8",7);
	do
	{
		switch(menu.Show())
		{
			case USERESC:
				ccfOld = pCore->Color(&ccfRed);
				pCore->Prints("Escape was pressed",FALSE,NULL,origin.X,origin.Y+5);
				pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,origin.X,origin.Y+6);
				pCore->Color(&ccfOld);
				pCore->Wait();
			return;
			case BADMENU:
				ccfOld = pCore->Color(&ccfRed);
				pCore->Prints("This menu sucks and cannot be shown",FALSE,NULL,origin.X,origin.Y+5);
				pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,origin.X,origin.Y+6);
				pCore->Color(&ccfOld);
				pCore->Wait();
			return;
			default:
				ConsoleMenuItem selection = menu.SelectedItem();
				pCore->Prints("You selected the item with the label: ",FALSE,NULL,origin.X,origin.Y+3);
				pCore->Prints(selection.Text());
				pCore->Prints("Press any key to restart the menu.",FALSE,NULL,origin.X,origin.Y+5);
				pCore->Wait();
			break;
		}
	}
	while(TRUE);
}

void DoWindowedScrollMenu()
{
	ConsoleFormat ccfRed = ConsoleFormat::BRIGHTRED
		,ccfClient = ConsoleFormat::BRIGHTWHITE
					  ,ccfOld;
	COORD origin = {31,0};

	WindowedMenu menu(origin,4,"Wnd Menu");
	menu.WindowColor(ConsoleFormat::BRIGHTBLUE);
	menu.ClientColor(ConsoleFormat::ONYELLOW);
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	menu.Append("Menu Item 1",0);
	menu.Append("Menu Item 2",1);
	menu.Append("Menu Item 3",2);
	menu.Append("Menu Item 4",3);
	menu.Append("Menu Item 5",4);
	menu.Append("Menu Item 6",5);
	menu.Append("Menu Item 7",6);
	menu.Append("Menu Item 8",7);
	menu.Scrollable(TRUE);
	do
	{
		switch(menu.Show())
		{
			case USERESC:
				ccfOld = pCore->Color(&ccfRed);
				pCore->Prints("Escape was pressed",FALSE,NULL,origin.X,origin.Y+12);
				pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,origin.X,origin.Y+13);
				pCore->Color(&ccfOld);
				pCore->Wait();
			return;
			case BADMENU:
				ccfOld = pCore->Color(&ccfRed);
				pCore->Prints("This menu sucks and cannot be shown",FALSE,NULL,origin.X,origin.Y+10);
				pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,origin.X,origin.Y+11);
				pCore->Color(&ccfOld);
				pCore->Wait();
			return;
			default:
				ConsoleMenuItem selection = menu.SelectedItem();
				pCore->Prints("You selected the item with the label: ",FALSE,NULL,origin.X,origin.Y+8);
				pCore->Prints(selection.Text());
				pCore->Prints("Press any key to restart the menu.",FALSE,NULL,origin.X,origin.Y+10);
				pCore->Wait();
			break;
		}
	}
	while(TRUE);
}


void CustomBuffers()
{
	COORD bufferSize = {6,4},
		  saveOrigin = {31,0},
		  loadOrigin = {31,8},
		  lowerRight;
	lowerRight.X = saveOrigin.X + bufferSize.X;
	lowerRight.Y = bufferSize.Y;
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	CharacterBox box(saveOrigin,lowerRight,ConsoleFormat::BRIGHTYELLOW,ConsoleFormat::ONBLUE);
	PCHAR_INFO buffer = new CHAR_INFO[bufferSize.X*bufferSize.Y];
	box.Draw();
	pCore->Prints("The original...",FALSE,NULL,38,1);
	pCore->SaveScreen(buffer,bufferSize,saveOrigin);
	pCore->Prints("...and the copies.",FALSE,NULL,loadOrigin.X,loadOrigin.Y - 1);
	for(loadOrigin.X; loadOrigin.X+bufferSize.X < MAXSCREENX-1; loadOrigin.X+=bufferSize.X)
	{
		for(loadOrigin.Y = 8; loadOrigin.Y+4 < MAXSCREENY-1; loadOrigin.Y+=bufferSize.Y)
		{
			pCore->LoadScreen(buffer,bufferSize,loadOrigin);
		}
	}
	pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,saveOrigin.X,loadOrigin.Y);
	pCore->Wait();
	delete [] buffer;
}

void ScanFloat()
{
	DOUBLE d = 0.0L;
	ConsoleCore* pCore = ConsoleCore::GetInstance();
	ConsoleFormat ccf(ConsoleFormat::BRIGHTYELLOW | ConsoleFormat::ONBLUE)
					  ,ccfOld;
	COORD ul = {0,0}
		,br = {44,6}
		,org = {1,2};
	CharacterBox box(ul,br,ConsoleFormat::BRIGHTYELLOW,ConsoleFormat::ONBLUE);
	box.Draw();
	ccfOld = pCore->Color(&ccf);
	pCore->Prints("argument digits is set to 4; enter a float",FALSE,NULL,1,1);
	pCore->ScanDouble(org,d);
	pCore->Prints("You entered: ",FALSE,NULL,1,3);
	pCore->Printd(d,4,FALSE,NULL);
	pCore->Prints("Press any key to return to the main menu.",FALSE,NULL,1,4);
	pCore->Color(&ccfOld);
	pCore->Wait();
}
