// Title: Jeff Benson's Console Library
// File: ConsoleMenu.cpp
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
#include "ConsoleMenu.h"
#include "ConsoleCore.h"
using namespace std;

ConsoleMenu::ConsoleMenu(COORD origin, ConsoleFormat format
    , ConsoleFormat selectionFormat)
    :m_origin(origin)
    ,m_format(format)
    ,m_selectionFormat(selectionFormat)
    ,m_longestItem(0)
    ,m_selected(m_items.end())
    ,m_enableEscape(true)
    ,m_enableDelete(0)
{
}

ConsoleMenu::ConsoleMenu(const ConsoleMenu& rhs)
    :m_origin(rhs.m_origin)
    ,m_format(rhs.m_format)
    ,m_selectionFormat(rhs.m_selectionFormat)
    ,m_longestItem(rhs.m_longestItem)
    ,m_items(rhs.m_items)
    ,m_selected(m_items.end())
    ,m_enableEscape(rhs.m_enableEscape)
    ,m_enableDelete(rhs.m_enableDelete)
{
}

COORD ConsoleMenu::Origin() const
{
    return m_origin;
}
COORD ConsoleMenu::Origin(COORD origin)
{
    COORD old = m_origin;
    m_origin = origin;
    return old;
}
const ConsoleFormat& ConsoleMenu::DisplayFormat() const
{
    return m_format;
}
ConsoleFormat ConsoleMenu::DisplayFormat(ConsoleFormat format)
{
    ConsoleFormat old = m_format;
    m_format = format;
    return old;
}
const ConsoleFormat& ConsoleMenu::SelectionFormat() const
{
    return m_selectionFormat;
}
ConsoleFormat ConsoleMenu::SelectionFormat(ConsoleFormat format)
{
    ConsoleFormat old = m_selectionFormat;
    m_selectionFormat = format;
    return old;
}

unsigned ConsoleMenu::Count() const
{
    return m_items.size();
}
void ConsoleMenu::Append(ConsoleMenuItem item)
{
    if(item.Text().length() > m_longestItem)
        m_longestItem = item.Text().length();
    m_items.push_back(item);
}

void ConsoleMenu::Append(const string& text, DWORD value)
{
    if(text.length() > m_longestItem)
        m_longestItem = text.length();
    m_items.push_back(ConsoleMenuItem(text,value));
}
void ConsoleMenu::InsertAfter(ConsoleMenuItem what, Iterator where)
{
    if(what.Text().length() > m_longestItem)
        m_longestItem = what.Text().length();
    m_items.insert(where,what);
}
void ConsoleMenu::Clear()
{
    m_items.clear();
    m_selected = m_items.end();
}

ConsoleMenu::ConstIterator ConsoleMenu::GetIterator(BOOL back) const
{
    ConstIterator it = m_items.begin();
    if(back)
        advance(it,m_items.size()-1);
    return it;
}

ConsoleMenu::Iterator ConsoleMenu::GetIterator(BOOL back)
{
    Iterator it = m_items.begin();
    if(back)
        advance(it,m_items.size()-1);
    return it;
}

ConsoleMenu::ConstIterator ConsoleMenu::GetEnd() const
{
    return m_items.end();
}

ConsoleMenu::Iterator ConsoleMenu::GetEnd()
{
    return m_items.end();
}

unsigned ConsoleMenu::LongestItem() const
{
    return m_longestItem;
}

DWORD ConsoleMenu::Show()
{
    if(m_items.empty())
        return BADMENU;
    ConsoleCore* pCore = ConsoleCore::GetInstance();
    ConsoleFormat oldFormat = pCore->Color();
    COORD bufferOrigin = m_origin;

    COORD bufferSize = {m_longestItem,m_items.size()};
    PCHAR_INFO menuBuffer = new CHAR_INFO[bufferSize.X*bufferSize.Y];
    memset(menuBuffer,0,sizeof(CHAR_INFO)*bufferSize.X*bufferSize.Y);

    // Use the last selected item if possible
    if (m_selected == m_items.end())
        m_selected = GetIterator();

    bool selected = false;
    do
    {
        // restore the screen.
        COORD cursorOffset = m_origin;
        pCore->LoadScreen(menuBuffer,bufferSize,cursorOffset);
        ConsoleMenu::Iterator it = GetIterator()
            ,end = GetEnd();
        for(;it != end; ++it)
        {
            if(it == m_selected)
                pCore->Prints(it->Text(),FALSE,&SelectionFormat(),cursorOffset.X,cursorOffset.Y);
            else
                pCore->Prints(it->Text(),FALSE,&DisplayFormat(),cursorOffset.X,cursorOffset.Y);
            cursorOffset.X = m_origin.X;
            cursorOffset.Y++;
        }
        pCore->CursorPosition(&cursorOffset);
        // Draw the menu
        pCore->Color(&oldFormat);
        // Return if selection has been made
        if (selected)
        {
            delete [] menuBuffer;
            return 0;
        }
        // reset the old format
        pCore->SaveScreen(menuBuffer,bufferSize,bufferOrigin);
        switch(int c = _getch())
        {
        case UP_KEY:
            if(m_selected == GetIterator())
                m_selected = GetIterator(TRUE);
            else
                --m_selected;
            break;
        case DOWN_KEY:
            if(m_selected == GetIterator(TRUE))
                m_selected = GetIterator();
            else
                ++m_selected;
            break;
        case RETURN_KEY:
            delete [] menuBuffer;
            pCore->Color(&oldFormat);
            return 0;
        case ESCAPE_KEY:
            if (!m_enableEscape)
                break;
            delete [] menuBuffer;
            pCore->Color(&oldFormat);
            return USERESC;
        case LEFT_KEY:
        case RIGHT_KEY:
            if (m_enableDelete != ENABLE_LR_DELETE)
                break;
        case DELETE_KEY:
        case BACKSPACE_KEY:
            if (m_enableDelete == DISABLE_DELETE)
                break;
            delete [] menuBuffer;
            pCore->Color(&oldFormat);
            return USERDELETE;
        case '0' ... '9':
            if ((c - '0') >= Count())
                break;
            if (c == '0')
                c = Count()-1;
            else
                c = (c - '1');
            m_selected = GetIterator();
            while (c--)
                ++m_selected;
            selected = true;
            break;
        }
    }
    while(TRUE);
}

string ConsoleMenu::SelectedText() const
{
    if(m_selected == m_items.end())
        return "";
    return m_selected->Text();
}

DWORD ConsoleMenu::SelectedValue() const
{
    if(m_selected == m_items.end())
        return -1;
    return m_selected->Value();
}

ConsoleMenuItem ConsoleMenu::SelectedItem()
{
    if(m_selected == m_items.end())
        return ConsoleMenuItem();
    return *m_selected;
}

BOOL ConsoleMenu::SelectedItem(int position)
{
    if(position >= m_items.size())
        return FALSE;
    m_selected = GetIterator();
    while (position--)
        ++m_selected;
    return TRUE;
}

void ConsoleMenu::EnableEscape(bool enableEscape)
{
    m_enableEscape = enableEscape;
}

void ConsoleMenu::EnableDelete(int enableDelete)
{
    m_enableDelete = enableDelete;
}

void ConsoleMenu::Selection(ConsoleMenu::Iterator it)
{
    m_selected = it;
}
ConsoleMenu::Iterator ConsoleMenu::Selection()
{
    return m_selected;
}
////////////////////////////////

ScrollingMenu::ScrollingMenu(COORD origin, unsigned maxToShow
    ,ConsoleFormat format, ConsoleFormat selectionFormat)
    :ConsoleMenu(origin, format,selectionFormat)
    ,m_maxToShow(maxToShow)
    ,m_menuAnchor(m_items.end())
{
}

ScrollingMenu::ScrollingMenu(const ScrollingMenu& rhs)
    :ConsoleMenu(rhs)
    ,m_maxToShow(rhs.m_maxToShow)
    ,m_menuAnchor(m_items.end())
{
}

unsigned ScrollingMenu::MaxToShow() const
{
    return m_maxToShow;
}

void ScrollingMenu::MaxToShow(unsigned max)
{
    m_maxToShow = max;
}

DWORD ScrollingMenu::Show()
{
    DWORD result = BADMENU;
    if(Count() == 0)
        return result;
    if(m_maxToShow > Count())
        return BADMENU;
    ConsoleCore* pCore = ConsoleCore::GetInstance();
    unsigned menuOffset = 0;
    COORD bufferOrigin = {Origin().X,Origin().Y}
        ,cursorOffset
        ,bufferSize = {LongestItem(),m_maxToShow};
    PCHAR_INFO menuBuffer = new CHAR_INFO[bufferSize.X*bufferSize.Y];
    memset(menuBuffer,0,sizeof(CHAR_INFO)*bufferSize.X*bufferSize.Y);
    ConsoleFormat oldFormat = pCore->Color();
    pCore->SaveScreen();

    // Use the last selected item if possible
    // The "anchor" is the top-most displayed element.
    // It is used to guide drawing relative items and to scroll the menu.
    if (m_selected == m_items.end())
    {
        Selection(GetIterator());
        m_menuAnchor = Selection();
    }
    else
    {
        Selection(m_selected);
        if (m_menuAnchor == m_items.end())
            m_menuAnchor = Selection();
    }

    bool selected = false;
    do
    {
        cursorOffset = Origin();
        pCore->LoadScreen(menuBuffer,bufferSize,cursorOffset);
        // Draw only as many items as needed.
        ConstIterator toDraw;
        unsigned shown = 0;
        for(toDraw = m_menuAnchor; shown < (menuOffset+m_maxToShow); ++shown,++toDraw)
        {
            pCore->Prints(string(LongestItem(),' '),FALSE,&DisplayFormat(),cursorOffset.X,cursorOffset.Y);
            if(toDraw == Selection())
                pCore->Prints(toDraw->Text(),FALSE,&SelectionFormat(),cursorOffset.X,cursorOffset.Y);
            else
                pCore->Prints(toDraw->Text(),FALSE,&DisplayFormat(),cursorOffset.X,cursorOffset.Y);
            cursorOffset.X = Origin().X;
            cursorOffset.Y++;
        }
        pCore->CursorPosition(&cursorOffset);
        pCore->Color(&oldFormat);
        // Return if selection has been made
        if (selected)
        {
            delete [] menuBuffer;
            return 0;
        }
        pCore->SaveScreen(menuBuffer,bufferSize,bufferOrigin);
        switch(int c = _getch())
        {
        case UP_KEY:
            {
                // Move up when highlighting the anchor
                Iterator selection = Selection();
                if(selection == m_menuAnchor)
                {
                    // Not at the top of the menu?  Just go back one element
                    if(m_menuAnchor != GetIterator())
                    {
                        m_menuAnchor--;
                        selection--;
                    }
                    else    // At the top?  Selection becomes bottom, anchor becomes bottom-maxToShow
                    {
                        selection = GetIterator(TRUE);
                        m_menuAnchor = selection;
                        advance(m_menuAnchor,-(int)(m_maxToShow-1));
                    }
                }
                else
                    selection--;
                Selection(selection);
            }
            break;
        case DOWN_KEY:
            {
                Iterator selection = Selection();
                Iterator bottomAnchor = m_menuAnchor;
                advance(bottomAnchor,m_maxToShow-1);
                // Moving down when at bottom of list
                if(selection == bottomAnchor)
                {
                    // Not at bottom end of list?
                    if(bottomAnchor != GetIterator(TRUE))
                    {
                        m_menuAnchor++;
                        selection++;
                    }
                    else // At bottom? Selection becomes top, anchor becomes top.
                    {
                        selection = GetIterator();
                        m_menuAnchor = selection;
                    }
                }
                else
                    selection++;
                Selection(selection);
            }
            break;
        case RETURN_KEY:
            delete [] menuBuffer;
            return 0;
        case ESCAPE_KEY:
            if (!m_enableEscape)
                break;
            delete [] menuBuffer;
            return USERESC;
        case LEFT_KEY:
        case RIGHT_KEY:
            if (m_enableDelete != 2)
                break;
        case DELETE_KEY:
        case BACKSPACE_KEY:
            if (!m_enableDelete)
                break;
            delete [] menuBuffer;
            return USERDELETE;
        case '0' ... '9':
            if ((c - '0') >= Count())
                break;
            if (c == '0')
                c = Count()-1;
            else
                c = (c - '1');
            m_selected = GetIterator();
            while (c--)
                ++m_selected;
            Selection(m_selected);
            selected = true;
            break;
        case 'a' ... 'z':
            if ((c - 'a') + 10 >= Count())
                break;
            c = (c - 'a') + 9;
            m_selected = GetIterator();
            while (c--)
                ++m_selected;
            Selection(m_selected);
            selected = true;
            break;
        }
    }
    while(TRUE);
}

ConsoleMenuItem ScrollingMenu::SelectedItem()
{
    if(m_selected == m_items.end())
        return ConsoleMenuItem();
    return *m_selected;
}

BOOL ScrollingMenu::SelectedItem(int position)
{
    if(position >= m_items.size())
        return FALSE;
    m_selected = GetIterator();
    while (position--)
        ++m_selected;
    m_menuAnchor = m_items.begin();
    Selection(m_selected);
    return TRUE;
}

////////////////////////////////

WindowedMenu::WindowedMenu (COORD origin, unsigned maxToShow, const string& title
        , ConsoleFormat format
        , ConsoleFormat selectionFormat
        , ConsoleFormat windowColor
        , ConsoleFormat clientColor
        , char fill)
    :ScrollingMenu(origin, maxToShow, format, selectionFormat)
    ,m_title(title)
    ,m_scrollable(maxToShow != 0)
    ,m_windowColor(windowColor)
    ,m_clientColor(clientColor)
    ,m_fill(fill)
{
}
WindowedMenu::WindowedMenu(const WindowedMenu & rhs)
    :ScrollingMenu(rhs)
    ,m_title(rhs.m_title)
    ,m_scrollable(rhs.m_scrollable)
    ,m_windowColor(rhs.m_windowColor)
    ,m_clientColor(rhs.m_clientColor)
    ,m_fill(rhs.m_fill)
{
}
const string& WindowedMenu ::Title() const
{
    return m_title;
}
void WindowedMenu::Title(const string& title)
{
    m_title = title;
}
BOOL WindowedMenu::Scrollable() const
{
    return m_scrollable;
}
void WindowedMenu::Scrollable(BOOL allow)
{
    m_scrollable = allow;
}
ConsoleFormat WindowedMenu::WindowColor() const
{
    return m_windowColor;
}
ConsoleFormat WindowedMenu::WindowColor(ConsoleFormat color)
{
    ConsoleFormat old = m_windowColor;
    m_windowColor = color;
    return old;
}
ConsoleFormat WindowedMenu::ClientColor() const
{
    return m_clientColor;
}

ConsoleFormat WindowedMenu::ClientColor(ConsoleFormat color)
{
    ConsoleFormat old = m_clientColor;
    m_clientColor = color;
    return old;
}

char WindowedMenu::Fill() const
{
    return m_fill;
}
void WindowedMenu::Fill(char fill)
{
    m_fill = fill;
}
DWORD WindowedMenu::Show()
{
    DWORD result = BADMENU;
    if(Count() == 0)
        return result;
    if(!m_scrollable)
        return ShowNoScroll();

    UINT lengthOfLongestString = LongestItem();
    COORD ulWindow = Origin(),
        brWindow = {Origin().X + lengthOfLongestString+2,Origin().Y + MaxToShow()+4}
        ,menuItemOrigin = {Origin().X+1,Origin().Y+3}
        ,oldOrigin;

    CharacterWindow wnd(ulWindow,brWindow,m_title,WindowColor(),ClientColor(),Fill());
    wnd.Draw();
    oldOrigin = Origin(menuItemOrigin);
    result = ScrollingMenu::Show();
    Origin(oldOrigin);
    return result;
}
unsigned WindowedMenu::LongestItem() const
{
    return ConsoleMenu::LongestItem() > Title().length() ? ConsoleMenu::LongestItem() : Title().length();
}
DWORD WindowedMenu::ShowNoScroll()
{
    DWORD result = BADMENU;
    if(Count() == 0)
        return result;
    COORD oldOrigin = Origin()
        ,menuItemOrigin = {Origin().X+1,Origin().Y+3};

    UINT lengthOfLongestString = LongestItem();

    COORD ulWindow = Origin(),
        brWindow = {Origin().X + lengthOfLongestString+2,Origin().Y + Count()+4};
    CharacterWindow wnd(ulWindow,brWindow,m_title,WindowColor(),ClientColor(),Fill());
    wnd.Draw();

    Origin(menuItemOrigin);
    result = ConsoleMenu::Show();
    Origin(oldOrigin);
    return result;
}
