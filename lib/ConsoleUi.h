#pragma once

#include "Utilities.h"
#include "Logger.h"

#include <JLib/ConsoleCore.h>

#include <memory>
#include <vector>
#include <stack>
#include <climits>
#include <algorithm>
#include <sstream>


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


// Operators
inline COORD operator+ ( const COORD& a, const COORD& b ) { return { short ( a.X + b.X ), short ( a.Y + b.Y ) }; }
inline COORD operator- ( const COORD& a, const COORD& b ) { return { short ( a.X - b.X ), short ( a.Y - b.Y ) }; }

inline COORD& operator+= ( COORD& a, const COORD& b ) { a.X += b.X; a.Y += b.Y; return a; }
inline COORD& operator-= ( COORD& a, const COORD& b ) { a.X -= b.X; a.Y -= b.Y; return a; }

inline std::ostream& operator<< ( std::ostream& os, const COORD& a )
{ return ( os << '{' << a.X << ", " << a.Y << '}' ); }


class ConsoleUi
{
public:

    // Base UI element
    struct Element
    {
        // Output integer, INT_MIN is the invalid sentinel value
        int resultInt = INT_MIN;

        // Output string
        std::string resultStr;

    protected:

        // Position that the UI element should be displayed
        COORD pos;

        // Size of the UI element. Used as both input and output parameters.
        // It is first set to the max size bounding box available to draw.
        // Then it is set to the actual size of the of element drawn.
        COORD size;

        // Initialize the UI element based on the current size, this also updates the size
        virtual void initialize() = 0;

        // Show the element, returns true if we should pop and continue, false if we hit a menu and should stop
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

    // UI elements stack
    std::stack<ElementPtr> stack;

    // Console window handle
    void *consoleWindow = 0;

    // Initialize the element and push it onto the stack
    void initalizeAndPush ( Element *element, short width, short height );

    // Clear the top element (visually)
    void clearTop() const
    {
        if ( stack.empty() )
            ConsoleCore::GetInstance()->ClearScreen();
        else
            CharacterBox::Draw ( top()->pos, top()->pos + top()->size, ' ' );
    }

public:

    // Auto-wrapped text box
    class TextBox : public Element
    {
        std::string text;
        std::vector<std::string> lines;

    protected:

        void initialize() override
        {
            std::stringstream ss ( text );
            std::string line;

            while ( getline ( ss, line ) )
            {
                if ( line.size() + paddedBorders.size() > ( size_t ) size.X )
                {
                    std::vector<std::string> tokens = split ( line );
                    line.clear();

                    for ( const std::string& token : tokens )
                    {
                        const size_t prefix = ( line.empty() ? 0 : line.size() + 1 );

                        if ( prefix + token.size() + paddedBorders.size() > ( size_t ) size.X )
                        {
                            lines.push_back ( " " + line + " " );
                            line.clear();
                        }

                        if ( line.empty() )
                            line = token;
                        else
                            line += " " + token;
                    }

                    if ( !line.empty() )
                        lines.push_back ( " " + line + " " );
                }
                else
                {
                    lines.push_back ( " " + line + " " );
                }
            }

            if ( lines.size() + bordersHeight > ( size_t ) size.Y )
            {
                lines.resize ( size.Y - bordersHeight - 1 );
                lines.push_back ( " ... " );
            }

            size_t longestLine = 0;
            for ( const std::string& line : lines )
                if ( line.size() > longestLine )
                    longestLine = line.size();

            ASSERT ( ( size_t ) size.X >= longestLine + borders.size() );
            ASSERT ( ( size_t ) size.Y >= lines.size() + bordersHeight );

            size.X = longestLine + borders.size();
            size.Y = lines.size() + bordersHeight;
        }

        bool show() override
        {
            LOG ( "text='%s'; pos=%s; size=%s", text, pos, size );

            CharacterBox::Draw ( pos, pos + size, '*' );
            for ( size_t i = 0; i < lines.size(); ++i )
                ConsoleCore::GetInstance()->Prints ( lines[i], false, 0, pos.X + 1, pos.Y + 1 + i );

            return true;
        }

    public:

        TextBox ( const std::string& text ) : text ( text ) {}
    };

    // Scrollable menu
    class Menu : public Element
    {
        std::string title;
        std::vector<std::string> items;
        std::string lastItem;

        WindowedMenu menu;

        std::string shortenWithEllipsis ( const std::string& text )
        {
            if ( text.size() + paddedBorders.size() > ( size_t ) size.X )
                return text.substr ( 0, size.X - paddedBorders.size() - ellipsis.size() ) + ellipsis;

            return text;
        }

    protected:

        void initialize() override
        {
            ASSERT ( ( size_t ) size.X >= minMenuItem.size() + paddedBorders.size() );
            ASSERT ( ( size_t ) size.Y > bordersTitleHeight );
            ASSERT ( items.size() <= maxMenuItems );

            menu.Title ( " " + shortenWithEllipsis ( title ) + " " );

            for ( size_t i = 0; i < items.size(); ++i )
            {
                if ( i < 9 )
                    items[i] = toString ( "[%d] %s", i + 1, items[i] );
                else
                    items[i] = toString ( "[%c] %s", 'A' + i - 9, items[i] );

                menu.Append ( " " + shortenWithEllipsis ( items[i] ) + " ", i );
            }

            if ( !lastItem.empty() )
            {
                lastItem = toString ( "[0] %s", lastItem );
                menu.Append ( " " + shortenWithEllipsis ( lastItem ) + " ", items.size() );
            }

            // Limit the menu vertial display size
            menu.MaxToShow ( std::min ( ( size_t ) size.Y - bordersTitleHeight, menu.Count() ) );

            // Update the element size after formatting the text
            size.X = std::max ( menu.LongestItem() + borders.size(), title.size() + borders.size() );
            size.Y = items.size() + ( lastItem.empty() ? 0 : 1 ) + bordersTitleHeight;

            menu.Origin ( pos );
            menu.EscapeKey ( true );
            menu.Scrollable ( true );
        }

        bool show() override
        {
            LOG ( "title='%s'; pos=%s; size=%s", title, pos, size );

            if ( ( resultInt = menu.Show() ) == 0 )
            {
                resultInt = menu.SelectedValue();
                resultStr = menu.SelectedText();
                LOG ( "resultInt=%d; resultStr='%s'", resultInt, resultStr );
            }

            return false;
        }

    public:

        Menu ( const std::string& title, const std::vector<std::string>& items, const std::string& lastItem = "" )
            : title ( title ), items ( items ), lastItem ( lastItem )
            , menu ( pos, items.size(), title, THEME ) {}
    };

    // Prompt types
    enum PromptTypeInteger { PromptInteger };
    enum PromptTypeString { PromptString };

    // Integer or string prompt
    class Prompt : public Element
    {
        std::string title;
        bool isIntegerPrompt = false;
        bool allowNegative = true;
        size_t maxDigits = 9;

    protected:

        void initialize() override
        {
            ASSERT ( ( size_t ) size.X >= title.size() + paddedBorders.size() );
            ASSERT ( ( size_t ) size.Y > bordersTitleHeight );
            size.Y = 1 + bordersTitleHeight;
        }

        bool show() override
        {
            LOG ( "title='%s'; pos=%s; size=%s", title, pos, size );

            CharacterBox::Draw ( pos, pos + size, '*' );
            ConsoleCore *cc = ConsoleCore::GetInstance();
            cc->Prints ( " " + title + " ", false, 0, pos.X + 1, pos.Y + 1 );
            cc->Prints ( std::string ( size.X - borders.size(), '*' ), false, 0, pos.X + 1, pos.Y + 2 );

            COORD scanPos = { short ( pos.X + 2 ), short ( pos.Y + 3 ) };
            cc->CursorPosition ( &scanPos );

            if ( isIntegerPrompt )
            {
                if ( cc->ScanNumber ( scanPos, resultInt, std::min ( maxDigits, size.X - paddedBorders.size() ),
                                      allowNegative, resultInt != INT_MIN ) )
                    LOG ( "resultInt=%d", resultInt );
                else
                    resultInt = INT_MIN;
            }
            else
            {
                if ( cc->ScanString ( scanPos, resultStr, size.X - paddedBorders.size() ) )
                    LOG ( "resultStr='%s'", resultStr );
                else
                    resultStr.clear();
            }

            return false;
        }

    public:

        Prompt ( PromptTypeString, const std::string& title, const std::string& initial = "" )
            : title ( title ), isIntegerPrompt ( false )
        {
            resultStr = initial;
        }

        Prompt ( PromptTypeInteger, const std::string& title, int initial = INT_MIN,
                 bool allowNegative = true, size_t maxDigits = 9 )
            : title ( title ), isIntegerPrompt ( true ), allowNegative ( allowNegative ), maxDigits ( maxDigits )
        {
            resultInt = initial;
        }
    };

    // Basic constructor
    ConsoleUi ( const std::string& title );

    // Push an element to the right or below the current one
    void pushRight ( Element *element, short width = SHRT_MAX, short height = SHRT_MAX );
    void pushBelow ( Element *element, short width = SHRT_MAX, short height = SHRT_MAX );

    // Push an element in front of the current one
    void pushInFront ( Element *element, short width = SHRT_MAX, short height = SHRT_MAX );

    // Pop an element off the stack
    void pop()
    {
        ASSERT ( stack.empty() == false );

        clearTop();
        stack.pop();
    }

    // Pop and show elements until we reach a menu, then return the pointer to the menu.
    // Note this DOESN'T clear any non-menu elements from the screen.
    Element *popUntilMenu();

    // Get the top element
    Element *top() const
    {
        if ( stack.empty() )
            return 0;

        return stack.top().get();
    }

    // Clear the screen
    void clear()
    {
        ConsoleCore::GetInstance()->ClearScreen();
    }
};
