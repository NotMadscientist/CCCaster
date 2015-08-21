#pragma once

#include "Logger.hpp"

#include <JLib/ConsoleCore.h>

#include <memory>
#include <vector>
#include <sstream>


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
        bool expandWidth() const { return _expand.X; }

        // True if the element fills the height of the screen.
        bool expandHeight() const { return _expand.X; }

    protected:

        // Basic constructor
        Element ( bool requiresUser ) : requiresUser ( requiresUser ) {}

        // Position that the element should be displayed
        COORD _pos;

        // Size of the element. Used as both input and output parameters.
        // It is first set to the max size bounding box available to draw.
        // Then it is set to the actual size of the of element drawn.
        COORD _size;

        // Indicates if this element should expand to take up the remaining screen space in either dimension.
        // Note the X and Y components are treated as boolean values.
        COORD _expand = { 0, 0 };

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

        TextBox ( const std::string& text );

    protected:

        void initialize() override;
        void show() override;

    private:

        std::vector<std::string> _lines;
    };

    // Scrollable menu
    class Menu : public Element
    {
    public:

        const std::string title;

        Menu ( const std::string& title, const std::vector<std::string>& items, const std::string& lastItem = "" );
        Menu ( const std::vector<std::string>& items, const std::string& lastItem = "" );

        void setPosition ( int position );
        void setEscape ( bool enabled );
        void setDelete ( int enabled );

        void overlayCurrentPosition ( const std::string& text, bool selected = false );

    protected:

        void initialize();
        void show();

    private:

        std::vector<std::string> items;

        std::string lastItem;

        WindowedMenu menu;

        std::string shortenWithEllipsis ( const std::string& text );
    };

    // Integer or string prompt
    class Prompt : public Element
    {
    public:

        const std::string title;

        const bool isIntegerPrompt = false;

        bool allowNegative = true;

        size_t maxDigits = 9;

        enum PromptTypeInteger { Integer };
        enum PromptTypeString { String };

        Prompt ( PromptTypeString, const std::string& title = "" );
        Prompt ( PromptTypeInteger, const std::string& title = "" );

        void setInitial ( int initial );
        void setInitial ( const std::string& initial );

    protected:

        void initialize();
        void show();
    };

    // Progress bar
    class ProgressBar : public Element
    {
    public:

        const std::string title;

        const size_t length;

        ProgressBar ( const std::string& title, size_t length );
        ProgressBar ( size_t length );

        void update ( size_t progress ) const;

    protected:

        void initialize() override;
        void show() override;
    };

    // Basic constructor
    ConsoleUi ( const std::string& title, bool isWine = false );

    // Push an element to the right or below the current one
    void pushRight ( Element *element, const COORD& expand = { 0, 0 } );
    void pushBelow ( Element *element, const COORD& expand = { 0, 0 } );

    // Push an element in front of the current one
    void pushInFront ( Element *element, const COORD& expand = { 0, 0 } );
    void pushInFront ( Element *element, const COORD& expand, bool shouldClearTop );

    // Pop an element off the stack
    void pop();

    // Pop and show until we reach an element that requires user interaction, then return element.
    // This should NOT be called without any such elements in the stack.
    // This does NOT pop the element that it returns.
    Element *popUntilUserInput ( bool clearPoppedElements = false );

    // Pop the non user input elements from the top of the stack
    void popNonUserInput();

    // Get the top element
    template<typename T = Element>
    T *top() const
    {
        if ( _stack.empty() )
            return 0;

        ASSERT ( _stack.back().get() != 0 );
        ASSERT ( typeid ( T ) == typeid ( Element ) || typeid ( *_stack.back().get() ) == typeid ( T ) );

        return ( T * ) _stack.back().get();
    }

    // True if there are no elements
    bool empty() const
    {
        return _stack.empty();
    }

    // If the top element has a border with the element below
    bool hasBorder() const;

    // Clear the top element (visually)
    void clearTop() const;

    // Clear below the top element (visually)
    void clearBelow ( bool preserveBorder = true ) const;

    // Clear to the right of the top element (visually)
    void clearRight() const;

    // Clear all elements
    void clearAll();

    // Clear the screen
    static void clearScreen();

    // Get console window handle
    static void *getConsoleWindow();

private:

    static const std::string Ellipsis; // "..."

    static const std::string MinText; // "A..."

    static const std::string MinMenuItem; // "[1] A..."

    static const std::string Borders; // "**"

    static const std::string PaddedBorders; // "*  *"

    static const size_t BordersHeight = 2; // 2 borders

    static const size_t MaxMenuItems = 9 + 26; // 1-9 and A-Z

    typedef std::shared_ptr<Element> ElementPtr;

    // UI elements stack
    std::vector<ElementPtr> _stack;

    // Initialize the element and push it onto the stack
    void initalizeAndPush ( Element *element, const COORD& expand );
};
