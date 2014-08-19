#pragma once

#include "Utilities.h"
#include "Logger.h"

#include <JLib/ConsoleCore.h>

#include <memory>
#include <vector>
#include <climits>
#include <algorithm>


#define ORIGIN ( ( COORD ) { 0, 0 } )

// unselected colour
// selected colour
// outline colour
// background colour
#define THEME                                               \
    ConsoleFormat::SYSTEM,                                  \
    ConsoleFormat::BLACK | ConsoleFormat::ONBRIGHTWHITE,    \
    ConsoleFormat::SYSTEM,                                  \
    ConsoleFormat::SYSTEM


class ConsoleUi
{
public:

    struct Element
    {
        int resultInt = INT_MIN;
        std::string resultStr;

    protected:

        // Position and size of the UI element
        COORD pos, size;

        // Show the element, returns true if we should keep showing, false if we hit a menu
        virtual bool show() = 0;

        friend ConsoleUi;
    };

private:

    typedef std::shared_ptr<Element> ElementPtr;

    std::vector<ElementPtr> stack;

public:

    struct TextBox : public Element
    {
    };

    struct TitleTextBox : public Element
    {
    };

    class Menu : public Element
    {
        std::string title;
        std::vector<std::string> items;
        std::string lastItem;

        std::shared_ptr<WindowedMenu> menu;

    protected:

        bool show() override
        {
            if ( !menu )
            {
                ASSERT ( size.X > 11 );             // '* [#] A... *'
                ASSERT ( size.Y > 4 );              // three rows + title
                ASSERT ( items.size() <= 9 + 26 );  // 1-9 + A-Z

                if ( title.size() > ( size_t ) size.X - 4 )
                    title = title.substr ( 0, size.X - 7 ) + "...";
                title = " " + title + " ";

                menu.reset ( new WindowedMenu ( pos, items.size(), title, THEME ) );

                for ( size_t i = 0; i < items.size(); ++i )
                {
                    if ( items[i].size() > ( size_t ) size.X - 8 )
                        items[i] = items[i].substr ( 0, size.X - 11 ) + "...";

                    if ( i < 9 )
                        menu->Append ( toString ( " [%d] %s ", i + 1, items[i] ) );
                    else
                        menu->Append ( toString ( " [%c] %s ", 'A' + i - 9, items[i] ) );
                }

                if ( !lastItem.empty() )
                {
                    if ( lastItem.size() > ( size_t ) size.X - 8 )
                        lastItem = lastItem.substr ( 0, size.X - 11 ) + "...";

                    menu->Append ( toString ( " [0] %s ", lastItem ) );
                }

                menu->MaxToShow ( std::min ( ( size_t ) size.Y - 4, menu->Count() ) );

                size.X = std::min ( ( size_t ) size.X, title.size() + 2 );        // title + border
                size.X = std::min ( ( size_t ) size.X, menu->LongestItem() + 2 ); // item + border
                size.Y = std::min ( ( size_t ) size.Y, items.size() + 4 );        // items.size() + three rows + title

                menu->Origin ( pos );
                menu->EscapeKey ( true );
                menu->Scrollable ( true );
            }

            resultInt = menu->Show();
            return false;
        }

    public:

        Menu ( const std::string& title, const std::vector<std::string>& items, const std::string& lastItem = "" )
            : title ( title ), items ( items ), lastItem ( lastItem ) {}
    };

    struct Prompt : public Element
    {
    };

    void pushRight ( Element *element )
    {
        if ( stack.empty() )
        {
            element->pos = ORIGIN;
            element->size = { short ( MAXSCREENX - 1 ), short ( MAXSCREENY - 1 ) };
        }
        else
        {
            element->pos = { short ( stack.back()->pos.X + stack.back()->size.X ), stack.back()->pos.Y };
        }

        stack.push_back ( ElementPtr ( element ) );
    }

    void pushBelow ( Element *element )
    {
        if ( stack.empty() )
        {
            element->pos = ORIGIN;
            element->size = { short ( MAXSCREENX - 1 ), short ( MAXSCREENY - 1 ) };
        }
        else
        {
            element->pos = { stack.back()->pos.X, short ( stack.back()->pos.Y + stack.back()->size.Y ) };
        }

        stack.push_back ( ElementPtr ( element ) );
    }

    void pushCurrent ( Element *element )
    {
        if ( stack.empty() )
        {
            element->pos = ORIGIN;
            element->size = { short ( MAXSCREENX - 1 ), short ( MAXSCREENY - 1 ) };
        }
        else
        {
            element->pos = stack.back()->pos;
        }

        stack.push_back ( ElementPtr ( element ) );
    }

    void pop()
    {
        ASSERT ( stack.empty() == false );

        stack.pop_back();
    }

    Element *show()
    {
        ASSERT ( stack.empty() == false );

        for ( auto it = stack.rbegin(); it != stack.rend(); ++it )
            if ( ! ( **it ).show() )
                return it->get();

        return 0;
    }
};
