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

        // Position that the UI element should be displayed
        COORD pos;

        // Size of the UI element. Used as both input and output parameters.
        // It is first set to the max size bounding box available to draw.
        // Then it is set to the actual size of the of element drawn.
        COORD size;

        // Show the element, returns true if we should show the next one, false if we hit a menu and should stop
        virtual bool show() = 0;

        friend ConsoleUi;
    };

private:

    static const std::string ellipsis; // "..."

    static const std::string minText; // "A..."

    static const std::string minMenuItem; // "[1] A..."

    static const std::string borders; // "**"

    static const std::string paddedBorders; // "*  *"

    static const size_t bordersHeight = 2; // 2 borders

    static const size_t bordersTitleHeight = 4; // 3 borders + title

    static const size_t maxMenuItems = 9 + 26; // 1-9 and A-Z

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

        std::string shortenWithEllipsis ( const std::string& text )
        {
            if ( text.size() + paddedBorders.size() > ( size_t ) size.X )
                return text.substr ( 0, size.X - paddedBorders.size() - ellipsis.size() ) + ellipsis;

            return text;
        }

    protected:

        bool show() override
        {
            if ( !menu )
            {
                ASSERT ( ( size_t ) size.X >= minMenuItem.size() + paddedBorders.size() );
                ASSERT ( ( size_t ) size.Y > bordersTitleHeight );
                ASSERT ( items.size() <= maxMenuItems );

                title = " " + shortenWithEllipsis ( title ) + " ";

                menu.reset ( new WindowedMenu ( pos, items.size(), title, THEME ) );

                for ( size_t i = 0; i < items.size(); ++i )
                {
                    if ( i < 9 )
                        items[i] = toString ( "[%d] %s", i + 1, items[i] );
                    else
                        items[i] = toString ( "[%c] %s", 'A' + i - 9, items[i] );

                    menu->Append ( " " + shortenWithEllipsis ( items[i] ) + " " );
                }

                if ( !lastItem.empty() )
                {
                    lastItem = toString ( "[0] %s", lastItem );
                    menu->Append ( " " + shortenWithEllipsis ( lastItem ) + " " );
                }

                // Limit the menu vertial display size
                menu->MaxToShow ( std::min ( ( size_t ) size.Y - bordersTitleHeight, menu->Count() ) );

                // Update the element size after formatting the text
                size.X = std::min ( ( size_t ) size.X, title.size() + borders.size() );
                size.X = std::min ( ( size_t ) size.X, menu->LongestItem() + borders.size() );
                size.Y = std::min ( ( size_t ) size.Y, items.size() + bordersTitleHeight );

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

    void *consoleWindow = 0;

    void initialize ( const std::string& title );

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
