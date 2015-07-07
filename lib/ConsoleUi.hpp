#pragma once

#include "StringUtils.hpp"
#include "Logger.hpp"
#include "Algorithms.hpp"

// Most of the implementation that depend on JLib is in this header so only the targets that need JLib can to link it
#include <JLib/ConsoleCore.h>

#include <memory>
#include <vector>
#include <climits>
#include <sstream>


#define ORIGIN ( ( COORD ) { 0, 0 } )

#define MAX_SCREEN_SIZE ( ( COORD ) { short ( MAXSCREENX ), short ( MAXSCREENY ) } )

// unselected color
// selected color
// outline color
// background color
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
    class Element
    {
    public:

        // Indicates this is an element that requires user interaction
        const bool requiresUser;

        // Output integer, INT_MIN is the invalid sentinel value
        int resultInt = INT_MIN;

        // Output string
        std::string resultStr;

        // True if the element fills the width of the screen.
        bool expandWidth() const { return expand.X; }

        // True if the element fills the height of the screen.
        bool expandHeight() const { return expand.X; }

    protected:

        // Basic constructor
        Element ( bool requiresUser ) : requiresUser ( requiresUser ) {}

        // Position that the element should be displayed
        COORD pos;

        // Size of the element. Used as both input and output parameters.
        // It is first set to the max size bounding box available to draw.
        // Then it is set to the actual size of the of element drawn.
        COORD size;

        // Indicates if this element should expand to take up the remaining screen space in either dimension.
        // Note the X and Y components are treated as boolean values.
        COORD expand = { 0, 0 };

        // Initialize the element based on the current size, this also updates the size
        virtual void initialize() = 0;

        // Show the element, may hang waiting for user interaction
        virtual void show() = 0;

        friend ConsoleUi;
    };

    // Auto-wrapped text box
    class TextBox : public Element
    {
    public:

        const std::string text;

        TextBox ( const std::string& text ) : Element ( false ), text ( trimmed ( text, "\n" ) ) {}

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

            if ( !expand.X )
                size.X = longestLine + borders.size();

            if ( !expand.Y )
                size.Y = lines.size() + bordersHeight;
        }

        void show() override
        {
            LOG ( "text='%s'; pos=%s; size=%s", text, pos, size );

            CharacterBox::Draw ( pos, pos + size, '*' );
            for ( size_t i = 0; i < lines.size(); ++i )
                ConsoleCore::GetInstance()->Prints ( lines[i], false, 0, pos.X + 1, pos.Y + 1 + i );
        }

    private:

        std::vector<std::string> lines;
    };

    // Scrollable menu
    class Menu : public Element
    {
    public:

        const std::string title;

        void setPosition ( int position )
        {
            if ( position < 0 )
                position = 0;
            else if ( position >= ( int ) menu.Count() )
                position = menu.Count() - 1;

            menu.SelectedItem ( position );
        }

        void setEscape ( bool enabled )
        {
            menu.EnableEscape ( enabled );
        }

        void setDelete ( int enabled )
        {
            menu.EnableDelete ( enabled );
        }

        void overlayCurrentPosition ( const std::string& text, bool selected = false )
        {
            const COORD pos = menu.CursorPosition();
            const size_t i = menu.SelectedValue();
            const std::string padded = items[i].substr ( 0, 4 ) + text;

            // Pad remaining text if necessary
            std::string remaining;
            if ( 4 + text.size() < items[i].size() )
                remaining = std::string ( items[i].substr ( 4 + text.size() ).size(), ' ' );

            ConsoleCore::GetInstance()->Prints ( " " + shortenWithEllipsis ( padded + remaining ) + " ",
                                                 false, ( selected ? &menu.SelectionFormat() : 0 ), pos.X, pos.Y );
        }

        Menu ( const std::string& title, const std::vector<std::string>& items, const std::string& lastItem = "" )
            : Element ( true ), title ( title ), items ( items ), lastItem ( lastItem )
            , menu ( pos, items.size() + ( lastItem.empty() ? 0 : 1 ), title, THEME ) {}

        Menu ( const std::vector<std::string>& items, const std::string& lastItem = "" )
            : Menu ( "", items, lastItem ) {}

    protected:

        void initialize() override
        {
            ASSERT ( ( size_t ) size.X >= minMenuItem.size() + paddedBorders.size() );
            ASSERT ( ( size_t ) size.Y > bordersHeight + ( title.empty() ? 0 : 2 ) );
            ASSERT ( items.size() <= maxMenuItems );

            if ( !title.empty() )
                menu.Title ( " " + shortenWithEllipsis ( title ) + " " );

            for ( size_t i = 0; i < items.size(); ++i )
            {
                if ( i < 9 )
                    items[i] = format ( "[%d] %s", i + 1, items[i] );
                else
                    items[i] = format ( "[%c] %s", 'A' + i - 9, items[i] );

                menu.Append ( " " + shortenWithEllipsis ( items[i] ) + " ", i );
            }

            if ( !lastItem.empty() )
            {
                lastItem = format ( "[0] %s", lastItem );
                menu.Append ( " " + shortenWithEllipsis ( lastItem ) + " ", items.size() );
            }

            // TODO this is broken because WindowedMenu::SelectedItem doesn't work when the menu is scrolled off-screen
            // // Limit the menu vertical display size
            // menu.MaxToShow ( std::min ( size.Y - bordersHeight - ( title.empty() ? 0 : 2 ), menu.Count() ) );

            ASSERT ( menu.Count() <= size.Y - bordersHeight - ( title.empty() ? 0 : 2 ) );

            // Menus are NEVER expanded
            size.X = std::max ( menu.LongestItem() + borders.size(), title.size() + borders.size() );
            size.Y = bordersHeight + ( title.empty() ? 0 : 2 ) + items.size() + ( lastItem.empty() ? 0 : 1 );

            menu.Origin ( pos );
            menu.Scrollable ( true );
        }

        void show() override
        {
            LOG ( "title='%s'; pos=%s; size=%s", title, pos, size );

            ASSERT ( menu.Count() > 0 );

            resultInt = menu.Show();

            if ( resultInt == 0 )
            {
                resultInt = menu.SelectedValue();
                resultStr = menu.SelectedText();
                LOG ( "resultInt=%d; resultStr='%s'", resultInt, resultStr );
            }
            else if ( resultInt == USERDELETE )
            {
                resultInt = menu.SelectedValue();
                resultStr.clear();
                LOG ( "resultInt=%d; deleted", resultInt );
            }
        }

    private:

        std::vector<std::string> items;

        std::string lastItem;

        WindowedMenu menu;

        std::string shortenWithEllipsis ( const std::string& text )
        {
            if ( text.size() + paddedBorders.size() > ( size_t ) size.X )
                return text.substr ( 0, size.X - paddedBorders.size() - ellipsis.size() ) + ellipsis;

            return text;
        }
    };

    // Prompt types
    enum PromptTypeInteger { PromptInteger };
    enum PromptTypeString { PromptString };

    // Integer or string prompt
    class Prompt : public Element
    {
    public:

        const std::string title;

        const bool isIntegerPrompt = false;

        bool allowNegative = true;

        size_t maxDigits = 9;

        void setInitial ( int initial )
        {
            if ( !isIntegerPrompt )
                return;

            resultInt = initial;
        }

        void setInitial ( const std::string& initial )
        {
            if ( isIntegerPrompt )
                return;

            resultStr = initial;
        }

        Prompt ( PromptTypeString, const std::string& title = "" )
            : Element ( true ), title ( title ), isIntegerPrompt ( false ) {}

        Prompt ( PromptTypeInteger, const std::string& title = "" )
            : Element ( true ), title ( title ), isIntegerPrompt ( true ) {}

    protected:

        void initialize() override
        {
            ASSERT ( ( size_t ) size.X >= title.size() + paddedBorders.size() );
            ASSERT ( ( size_t ) size.Y > bordersHeight + ( title.empty() ? 0 : 2 ) );

            if ( !expand.X )
                size.X = title.size() + paddedBorders.size();

            // Prompts are NEVER expanded vertically
            size.Y = 1 + bordersHeight + ( title.empty() ? 0 : 2 );
        }

        void show() override
        {
            LOG ( "title='%s'; pos=%s; size=%s", title, pos, size );

            CharacterBox::Draw ( pos, pos + size, '*' );
            ConsoleCore *cc = ConsoleCore::GetInstance();

            if ( !title.empty() )
            {
                cc->Prints ( " " + title + " ", false, 0, pos.X + 1, pos.Y + 1 );
                cc->Prints ( std::string ( size.X - borders.size(), '*' ), false, 0, pos.X + 1, pos.Y + 2 );
            }

            COORD scanPos = { short ( pos.X + 2 ), short ( pos.Y + ( title.empty() ? 1 : 3 ) ) };
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
                {
                    LOG ( "resultStr='%s'", resultStr );
                    resultInt = 0;
                }
                else
                {
                    resultStr.clear();
                    resultInt = INT_MIN;
                }
            }
        }
    };

    // Progress bar
    class ProgressBar : public Element
    {
    public:

        const std::string title;

        const size_t length;

        ProgressBar ( const std::string& title, size_t length )
            : Element ( false ), title ( title ), length ( length ) {}

        ProgressBar ( size_t length )
            : Element ( false ), title ( "" ), length ( length ) {}

        void update ( size_t progress ) const
        {
            std::string bar ( clamped ( progress, 0u, length ), '.' );
            if ( progress < length )
                bar += std::string ( clamped ( length - progress, 0u, length ), ' ' );

            ConsoleCore::GetInstance()->Prints ( bar, false, 0, pos.X + 2, pos.Y + size.Y - 2 );
        }

    protected:

        void initialize() override
        {
            ASSERT ( ( size_t ) size.X >= std::max ( title.size(), length ) + paddedBorders.size() );
            ASSERT ( ( size_t ) size.Y > bordersHeight + ( title.empty() ? 0 : 2 ) );

            // Progress bars are NEVER expanded
            size.X = std::max ( title.size(), length ) + paddedBorders.size();
            size.Y = 1 + bordersHeight + ( title.empty() ? 0 : 2 );
        }

        void show() override
        {
            LOG ( "title='%s'; length=%u; pos=%s; size=%s", title, length, pos, size );

            CharacterBox::Draw ( pos, pos + size, '*' );
            ConsoleCore *cc = ConsoleCore::GetInstance();

            if ( !title.empty() )
            {
                cc->Prints ( " " + title + " ", false, 0, pos.X + 1, pos.Y + 1 );
                cc->Prints ( std::string ( size.X - borders.size(), '*' ), false, 0, pos.X + 1, pos.Y + 2 );
            }
        }

    };

    // Basic constructor
    ConsoleUi ( const std::string& title, bool isWine = false );

    // Push an element to the right or below the current one
    void pushRight ( Element *element, const COORD& expand = { 0, 0 } );
    void pushBelow ( Element *element, const COORD& expand = { 0, 0 } );

    // Push an element in front of the current one
    void pushInFront ( Element *element, const COORD& expand = { 0, 0 } );
    void pushInFront ( Element *element, const COORD& expand, bool shouldClearTop )
    {
        if ( shouldClearTop )
            clearTop();

        pushInFront ( element, expand );
    }


    // Pop an element off the stack
    void pop()
    {
        ASSERT ( stack.empty() == false );

        clearTop();
        stack.pop_back();
    }

    // Pop and show until we reach an element that requires user interaction, then return element.
    // This should NOT be called without any such elements in the stack.
    // This does NOT pop the element that it returns.
    Element *popUntilUserInput ( bool clearPoppedElements = false )
    {
        ASSERT ( stack.empty() == false );

        while ( !stack.empty() )
        {
            if ( stack.back()->requiresUser )
            {
                stack.back()->show();
                break;
            }

            if ( clearPoppedElements )
                pop();
            else
                stack.pop_back();
        }

        ASSERT ( stack.empty() == false );
        ASSERT ( stack.back().get() != 0 );

        return stack.back().get();
    }

    // Pop the non user input elements from the top of the stack
    void popNonUserInput()
    {
        while ( !stack.empty() && !top()->requiresUser )
            pop();
    }

    // Get the top element
    template<typename T = Element>
    T *top() const
    {
        if ( stack.empty() )
            return 0;

        ASSERT ( stack.back().get() != 0 );
        ASSERT ( typeid ( T ) == typeid ( Element ) || typeid ( *stack.back().get() ) == typeid ( T ) );

        return ( T * ) stack.back().get();
    }

    // True if there are no elements
    bool empty() const
    {
        return stack.empty();
    }

    // Clear the screen
    void clear()
    {
        ConsoleCore::GetInstance()->ClearScreen();
    }

    // Clear all elements
    void clearAll()
    {
        ConsoleCore::GetInstance()->ClearScreen();
        stack.clear();
    }

    // If the top element has a border with the element below
    bool hasBorder() const
    {
        for ( size_t i = 2; i < stack.size(); ++i )
        {
            if ( top()->pos.X >= stack[stack.size() - i]->pos.X
                    && top()->pos.X + top()->size.X <= stack[stack.size() - i]->pos.X + stack[stack.size() - i]->size.X
                    && top()->pos.Y > stack[stack.size() - i]->pos.Y )
            {
                return true;
            }
        }

        return false;
    }

    // Clear the top element (visually)
    void clearTop() const
    {
        if ( stack.empty() || stack.size() == 1 )
            ConsoleCore::GetInstance()->ClearScreen();
        else if ( hasBorder() )
            CharacterBox::Draw ( { top()->pos.X, short ( top()->pos.Y + 1 ) }, MAX_SCREEN_SIZE, ' ' );
        else
            CharacterBox::Draw ( top()->pos, MAX_SCREEN_SIZE, ' ' );
    }

    // Clear below the top element (visually)
    void clearBelow ( bool preserveBorder = true ) const
    {
        if ( stack.empty() )
        {
            ConsoleCore::GetInstance()->ClearScreen();
        }
        else
        {
            const COORD pos = { top()->pos.X, short ( top()->pos.Y + top()->size.Y - ( preserveBorder ? 0 : 1 ) ) };
            CharacterBox::Draw ( pos, MAX_SCREEN_SIZE, ' ' );
        }
    }

    // Clear to the right of the top element (visually)
    void clearRight() const
    {
        if ( stack.empty() )
        {
            ConsoleCore::GetInstance()->ClearScreen();
        }
        else if ( hasBorder() )
        {
            const COORD pos = { short ( top()->pos.X + top()->size.X ), short ( top()->pos.Y + 1 ) };
            CharacterBox::Draw ( pos, MAX_SCREEN_SIZE, ' ' );
        }
        else
        {
            CharacterBox::Draw ( { short ( top()->pos.X + top()->size.X ), top()->pos.Y }, MAX_SCREEN_SIZE, ' ' );
        }
    }

    // Get console window handle
    static void *getConsoleWindow();

private:

    static const std::string ellipsis; // "..."

    static const std::string minText; // "A..."

    static const std::string minMenuItem; // "[1] A..."

    static const std::string borders; // "**"

    static const std::string paddedBorders; // "*  *"

    static const size_t bordersHeight = 2; // 2 borders

    static const size_t maxMenuItems = 9 + 26; // 1-9 and A-Z

    typedef std::shared_ptr<Element> ElementPtr;

    // UI elements stack
    std::vector<ElementPtr> stack;

    // Initialize the element and push it onto the stack
    void initalizeAndPush ( Element *element, const COORD& expand );
};
