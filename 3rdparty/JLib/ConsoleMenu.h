// Title: Jeff Benson's Console Library
// File: ConsoleMenu.h
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
#ifndef _PRAGMA_ONCE_CONSOLEMENU_H_
#define _PRAGMA_ONCE_CONSOLEMENU_H_
#include <windows.h>
#include "ConsoleFormat.h"
#include "ConsoleMenuItem.h"
#include "CharacterBox.h"
#include <list>

#define DISABLE_DELETE      0
#define ENABLE_DELETE       1
#define ENABLE_LR_DELETE    2

//      ConsoleMenu
//  A ConsoleMenu is a collection of items.  Menus can draw themselves
//  and use ConsoleCore.  To launch a menu call Show and then check
//  the return value.  To retreive information from a selected menu item
//  after calling show, call one of the Selected functions.
//  Use the up and down arrow keys to change your selection.  Press
//  enter to make a selection.  Press escape to dismiss the menu.
//  This menu also supports wrapping from top to bottom.
//  You can also set the coloring for menu items.  The regular format
//  is the color used to draw items that are not selected.  The
//  selection format is used on the item that is currently highlighted.
class ConsoleMenu
{
public:
    typedef std::list<ConsoleMenuItem> MenuItems;
    typedef std::list<ConsoleMenuItem>::iterator Iterator;
    typedef std::list<ConsoleMenuItem>::const_iterator ConstIterator;

    //      ConsoleMenu
    //  Creates a menu.
    //  Arguments:
    //      COORD origin:   Where the first item is written.
    //      ConsoleFormat format:   The color of items that are not selected.
    //      ConsoleFormat selectionFormat:  The color of the item that is selected.
    //
    ConsoleMenu(COORD origin, ConsoleFormat format = ConsoleFormat::SYSTEM
        , ConsoleFormat selectionFormat = ~ConsoleFormat::SYSTEM);

    //      ConsoleMenu
    //  Copies a menu.
    //  Arguments:
    //      const ConsoleMenu& rhs: What to copy.
    ConsoleMenu(const ConsoleMenu& rhs);

    //      Origin
    //  Gets the origin of the window
    //  Returns: where the first item is written.
    COORD Origin() const;

    //      Origin
    //  Sets the origin of the window
    //  Arguments:
    //      COORD origin:   Where the first item will be written.
    //  Returns: the old origin.
    COORD Origin(COORD origin);

    //      DisplayFormat
    //  Gets the format for unselected items.
    //  Returns: the color of items that are not selected.
    const ConsoleFormat& DisplayFormat() const;

    //      DisplayFormat
    //  Sets the format for unselected items.
    //  Arguments:
    //      ConsoleFormat format:   The format for unselected items.
    //  Returns: the old color for items that are not selected.
    ConsoleFormat DisplayFormat(ConsoleFormat format);

    //      SelectionFormat
    //  Sets the format for the selected item.
    //  Returns: the color of the selected item.
    const ConsoleFormat& SelectionFormat() const;

    //      SelectionFormat
    //  Gets the format for the selected item.
    //  Arguments:
    //      ConsoleFormat format:   The format for the selected item.
    //  Returns: the old format for the selected item.
    ConsoleFormat SelectionFormat(ConsoleFormat format);

    //      Count
    //  Gets the number of menu items.
    //  Returns: the number of menu items.
    unsigned Count() const;

    //      Append
    //  Appends an item to the menu.
    //  Arguments:
    //      ConsoleMenuItem item:   the item.
    //  Note:
    //      This method invalidates all iterators for this menu.
    void Append(ConsoleMenuItem item);

    //      Append
    //  Appends an item to the menu.
    //  Arguemnts:
    //      string text:    the text of the itme
    //      DWORD value:    the user defined value for the item.
    //  Note:
    //      This method invalidates all iterators for this menu.
    void Append(const std::string& text, DWORD value);

    //      InsertAfter
    //  Inserts an item into the menu after a specific location.
    //  Arguments:
    //      ConsoleMenuItem what:   what to insert.
    //      Iterator where: the item preceeding the location to put the new item.
    //  Notes:
    //      Obtain an iterator using GetIterator.
    //      This method invalidates all iterators for this menu.
    void InsertAfter(ConsoleMenuItem what, Iterator where);

    //      Clear
    //  Removes all items from the menu.
    //  Note:
    //      This method invalidates all iterators for this menu.
    void Clear();

    //      GetIterator
    //  Gets a constant bidirectional iterator into the collection of items.
    //  Arguments:
    //      BOOL back:  whether to get the first item or last item.
    //  Returns: a constant iterator.
    //  Notes:
    //      This function will always return an iterator that points
    //  at an element unless the menu is empty.

    ConstIterator GetIterator(BOOL back = FALSE) const;
    //      GetIterator
    //  Gets a bidirectional iterator into the collection of items.
    //  Arguments:
    //      BOOL back:  whether to get the first item or last item.
    //  Returns: an iterator.
    //  Notes:
    //      This function will always return an iterator that points
    //  at an element unless the menu is empty.
    Iterator GetIterator(BOOL back = FALSE);

    //      GetEnd
    //  Gets a constant iterator into the items collection that is one past the end
    //  of the item collection elements.
    //  Returns: a constant iterator.
    //  Notes:
    //      Use this when you need to manually iterate the items.
    ConstIterator GetEnd() const;

    //      GetEnd
    //  Gets an iterator into the items collection that is one past the end
    //  of the item collection elements.
    //  Returns: a constant iterator.
    //  Notes:
    //      Use this when you need to manually iterate the items.
    Iterator GetEnd();

    //      LongestItem
    //  Gets the length of the longest string.
    //  Returns: the length of the longest string.
    //  Notes:
    //      The return value of this function is updated automatically
    //  when items are appended or inserted.
    //  In the case of WindowedMenu, this length may actually be the
    //  length of the window's title.
    virtual unsigned LongestItem() const;

    //      Show
    //  Runs the menu and waits for the user to select an item or press escape.
    //  Returns:
    //      If the user selects an item: 0.
    //      If the user presses escape: USERESC.
    //      If the menu has no items: BADMENU.
    virtual DWORD Show();

    //      SelectedText
    //  After calling Show, this will return the text of the selected item.
    //  Returns: the text of the selected item.
    //  Notes:
    //      You should always check the return value of Show before
    //  calling this method.  Otherwise the behavior is undefined.
    std::string SelectedText() const;

    //      SelectedValue
    //  After calling Show, this will return the user defined value of the selected item.
    //  Returns: the user defined value of the selected item.
    //  Notes:
    //      You should always check the return value of Show before
    //  calling this method.  Otherwise the behavior is undefined.
    DWORD SelectedValue() const;

    //      SelectedItem
    //  After calling Show, this will return the menu item that was selected.
    //  Returns: the menu item that was selected.
    //  Notes:
    //      You should always check the return value of Show before
    //  calling this method.  Otherwise the behavior is undefined.
    ConsoleMenuItem SelectedItem();

    //      SelectedItem
    //  Moves the current selected item to the one at the given position
    //  Returns: true if successful
    BOOL SelectedItem(int position);

    // Enable the escape key
    void EnableEscape(bool enableEscape);

    // Enable deleting items
    // 1 for just delete/BS, 2 for delete/BS/left/right
    void EnableDelete(int enableDelete);

protected:
    //      Selection
    //  Sets the selected item.
    //  Arguments:
    //      Iterator it:    an iterator into the menu item collection.
    //  Notes:
    //      This method is used internally by derived classes in order
    //  to set the selection.
    void Selection(ConsoleMenu::Iterator it);

    //      Selection
    //  Gets an iterator to the selected item.
    //  Returns: an iterator to the selected item.
    //  Notes:
    //      This method is used internally by derived classes in order
    //  to get the selection.  Do not treat the return value of this
    //  as a reference.  For example, Selection(Selection()++) will
    //  not advance the selected item to the next item in the list!
    Iterator Selection();
private:

    //      ConsoleMenu
    //  Not allowed to make menus like this.
    ConsoleMenu();

    COORD m_origin;             // Where the first item will be written to the screen.
    ConsoleFormat m_format      // The format for unselected items.
        ,m_selectionFormat;     // The format for selected items (highlighting).
    unsigned m_longestItem;     // The length of the longest item.

protected:
    MenuItems m_items;          // The menu items.
    Iterator m_selected;        // The currently/last selected item.
    bool m_enableEscape;        // Enables using escape to quit the menu.
    int m_enableDelete;         // Enables deleting menu items, 1 for just delete/BS, 2 for delete/BS/left/right
};

//      ScrollingMenu
//  The scrollingMenu functions like the ConsoleMenu except
//  it has the ability to hide some of its items.  This makes the
//  ScrollingMenu ideal when screen real estate is limited.
//  As the user presses the up and down arrows the menu items
//  in "view" will shift automatically.
class ScrollingMenu : public ConsoleMenu
{
public:

    //      ScrollingMenu
    //  Creates a scrollable menu.
    //  Arguments:
    //      COORD origin:   Where top visible item will be displayed
    //      unsigned maxToShow: The number of menu items that can be displayed at one time.
    //      ConsoleFormat format:   The color of unselected items.
    //      ConsoleFormat selectionFormat:  The color of the selected item.
    //  Notes:
    //      If the maxToShow is equal to or less than the number of items in the menu
    //  then the behavior of Show is undefined.
    ScrollingMenu(COORD origin, unsigned maxToShow, ConsoleFormat format = ConsoleFormat::SYSTEM
        , ConsoleFormat selectionFormat = ~ConsoleFormat::SYSTEM);

    //      ScrollingMenu
    //  Copies a scrollable menu.
    //  Arguments:
    //      const ScrollingMenu& rhs:   What to copy.
    ScrollingMenu(const ScrollingMenu& rhs);

    //      MaxToShow
    //  Gets the maximum number of items that can be displayed at once.
    //  Returns: the number of items.
    //  Notes:
    //      If this value is less than or equal to the number of menu items the behavior
    //  of Show will be undefined.
    unsigned MaxToShow() const;
    //      MaxToShow
    //  Sets the maximum number of items that can be displayed at once.
    //  Arguments:
    //      unsigned max:   The number of items that can be displayed at once.
    //  Notes:
    //      If this value is less than or equal to the number of menu items the behavior
    //  of Show will be undefined.
    void MaxToShow(unsigned max);

    //      Show
    //  Runs the menu and waits for the user to select an item or press escape.
    //  Returns:
    //      If the user selects an item: 0.
    //      If the user presses escape: USERESC.
    //      If the menu has no items: BADMENU.
    //  Notes:
    //      If this value is less than or equal to the number of menu items the behavior
    //  of Show will be undefined.
    virtual DWORD Show();

    //      SelectedItem
    //  After calling Show, this will return the menu item that was selected.
    //  Returns: the menu item that was selected.
    //  Notes:
    //      You should always check the return value of Show before
    //  calling this method.  Otherwise the behavior is undefined.
    ConsoleMenuItem SelectedItem();

    //      SelectedItem
    //  Moves the current selected item to the one at the given position
    //  Returns: true if successful
    BOOL SelectedItem(int position);

private:
    unsigned m_maxToShow;   // The number of items that can be displayed at one time.
    Iterator m_menuAnchor;  // The current/last menu anchor item.
};

//      WindowedMenu
//  The windowed menu combines the abilities of a CharacterWindow
//  and a ScrollingMenu.  It will automatically draw a CharacterWindow
//  around itself when shown.
//  WindowedMenus do not have to be scrollable.  You can enable or
//  disable this with the Scrollable function.  When Scrolling is disabled
//  the menu will act like a ConsoleMenu.  When SCrolling is enabled
//  the menu will act like a ScrollingMenu and the size of the CharacterWindow
//  will fit the maxToShow number of items.
//  WindowedMenus have two sets of color formatting.  Format and selectionFormat
//  apply to the items displayed.  WindowColor and clientColor apply to the
//  windowed box.  The title of the window is drawn using the clientColor.
class WindowedMenu : public ScrollingMenu
{
public:
    //      WindowedMenu
    //  Creates a windowed menu.
    //  Arguments:
    //      COORD origin:   Where top visible item will be displayed
    //      unsigned maxToShow: The number of menu items that can be displayed at one time.
    //      ConsoleFormat format:   The color of unselected items.
    //      ConsoleFormat selectionFormat:  The color of the selected item.
    //      ConsoleFormat windowColor:  The color used to draw the window.
    //      ConsoleFormat clientColor:  The color used to draw the inside of the window and the title.
    //  Notes:
    //      If maxToShow is zero, the window will be created as unscrollable, otherwise
    //  it will be marked as scrollable.
    //  If the maxToShow is equal to or less than the number of items in the menu
    //  then the behavior of Show is undefined.
    WindowedMenu (COORD origin, unsigned maxToShow, const std::string& title
        , ConsoleFormat format = ConsoleFormat::SYSTEM
        , ConsoleFormat selectionFormat = ~ConsoleFormat::SYSTEM
        , ConsoleFormat windowColor = ConsoleFormat::SYSTEM
        , ConsoleFormat clientColor = ~ConsoleFormat::SYSTEM
        , char fill = '*');

    //      WindowedMenu
    //  Copies a windowed menu.
    //  Arguments:
    //      const WindowedMenu& rhs:    What to copy.
    WindowedMenu (const WindowedMenu & rhs);

    //      Title
    //  Gets the title
    //  Returns: the title.
    const std::string& Title() const;

    //      Title
    //  Sets the title
    //  Arguments:
    //      const string title: The window title.
    void Title(const std::string& title);

    //      Scrollable
    //  Gets whether the menu is displayed with the scrolling style.
    //  Returns: how the menu will be displayed.
    BOOL Scrollable() const;

    //      Scrollable
    //  Sets whether the menu is displayed.
    //  Arguments:
    //      BOOL allow: Whether the menu is scrollable.
    void Scrollable(BOOL allow);

    //      WindowColor
    //  Gets the color used to draw the border of the window.
    //  Returns: the color used to draw the border of the window.
    ConsoleFormat WindowColor() const;

    //      WindowColor
    //  Sets the color used to draw the border of the window.
    //  Arguments:
    //      ConsoleFormat color:    the color used to draw the border of the window.
    ConsoleFormat WindowColor(ConsoleFormat color);

    //      ClientColor
    //  Gets the color used to draw the inside of the window and the title.
    //  Returns: the color used to draw the inside of the window and the title.
    ConsoleFormat ClientColor() const;

    //      ClientColor
    //  Sets the color used to draw the inside of the window and the title.
    //  Arguments:
    //      ConsoleFormat color:    the color used to draw the inside of the window and the title.
    ConsoleFormat ClientColor(ConsoleFormat color);

    //      Fill
    //  Gets the character used to draw the window.
    //  Returns: the character used to draw the window.
    char Fill() const;
    //      Fill
    //  Sets the character used to draw the window.
    //  Arguments:
    //      char fill:  The character.
    void Fill(char fill);

    //      Show
    //  Runs the menu and waits for the user to select an item or press escape.
    //  Returns:
    //      If the user selects an item: 0.
    //      If the user presses escape: USERESC.
    //      If the menu has no items: BADMENU.
    //  Notes:
    //  If Scrolling style is on:
    //  If maxToShow is less than or equal to the number of menu items the behavior
    //  of Show will be undefined.
    virtual DWORD Show();

    //      LongestItem
    //  Gets the length of the longest string.
    //  Returns: the length of the longest string.
    //  Notes:
    //      The return value of this function is updated automatically
    //  when items are appended or inserted.
    //  In the case of WindowedMenu, this length may actually be the
    //  length of the window's title.
    virtual unsigned LongestItem() const;
private:
    DWORD ShowNoScroll();
    std::string m_title;
    BOOL m_scrollable;
    ConsoleFormat m_windowColor
        ,m_clientColor;
    char m_fill;
};
#endif
