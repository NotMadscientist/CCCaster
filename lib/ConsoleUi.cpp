#include "ConsoleUi.h"
#include "Utilities.h"

#include <windows.h>

#include <vector>

using namespace std;


// Initial console window dimensions (definitions needed for JLib)
int MAXSCREENX = 80;
int MAXSCREENY = 25;

// Definitions of text sizes
const string ConsoleUi::ellipsis = "...";
const string ConsoleUi::minText = "A...";
const string ConsoleUi::minMenuItem = "[1] A...";
const string ConsoleUi::borders = "**";
const string ConsoleUi::paddedBorders = "*  *";


ConsoleUi::ConsoleUi ( const string& title )
{
    consoleWindow = GetConsoleWindow();
    SetConsoleTitle ( title.c_str() );
    SetConsoleOutputCP ( 437 );

    if ( detectWine() )
        return;

    // Undocumented console font functions:
    // http://blogs.microsoft.co.il/blogs/pavely/archive/2009/07/23/changing-console-fonts.aspx
    // http://cpptutorials.freeforums.org/please-oh-please-anyway-to-adjust-the-console-font-size-t605.html
    typedef BOOL ( WINAPI * SetConsoleFont_ ) ( HANDLE ConsoleOutput, DWORD FontIndex ); // kernel32!SetConsoleFont
    typedef BOOL ( WINAPI * GetConsoleFontInfo_ ) ( HANDLE ConsoleOutput, BOOL Unknown1, DWORD Unknown2,
            PCONSOLE_FONT_INFO ConsoleFontInfo ); // kernel32!GetConsoleFontInfo
    typedef DWORD ( WINAPI * GetNumberOfConsoleFonts_ ) (); // kernel32!GetNumberOfConsoleFonts

    // Setup undocumented functions
    SetConsoleFont_ SetConsoleFont = reinterpret_cast<SetConsoleFont_> ( GetProcAddress (
                                         GetModuleHandle ( "kernel32.dll" ), "SetConsoleFont" ) );
    GetConsoleFontInfo_ GetConsoleFontInfo = reinterpret_cast<GetConsoleFontInfo_> ( GetProcAddress (
                GetModuleHandle ( "kernel32.dll" ), "GetConsoleFontInfo" ) );
    GetNumberOfConsoleFonts_ GetNumberOfConsoleFonts = reinterpret_cast<GetNumberOfConsoleFonts_> ( GetProcAddress (
                GetModuleHandle ( "kernel32.dll" ), "GetNumberOfConsoleFonts" ) );

#ifdef MISSING_CONSOLE_FONT_SIZE
    typedef COORD ( WINAPI * GetConsoleFontSize_ ) ( HANDLE hConsoleOutput, DWORD nFont );
    GetConsoleFontSize_ GetConsoleFontSize = reinterpret_cast<GetConsoleFontSize_> ( GetProcAddress (
                GetModuleHandle ( "kernel32.dll" ), "GetConsoleFontSize" ) );
#endif

    // Get handle
    HANDLE handle = GetStdHandle ( STD_OUTPUT_HANDLE );

    // Get Number of console fonts
    DWORD numFounts = GetNumberOfConsoleFonts();

    // Setup array
    vector<CONSOLE_FONT_INFO> fonts ( numFounts );

    // Get font info
    GetConsoleFontInfo ( handle, false, numFounts, &fonts[0] );

    for ( size_t i = 0; i < numFounts; ++i )
    {
        // Get console font Size
        fonts[i].dwFontSize = GetConsoleFontSize ( handle, fonts[i].nFont );

        // Find the right font size
        if ( fonts[i].dwFontSize.X == 8 && fonts[i].dwFontSize.Y == 12 )
        {
            // Set that font
            SetConsoleFont ( handle, fonts[i].nFont );
            break;
        }
    }
}

void ConsoleUi::push ( ConsoleUi::Element *element, short width, short height )
{
    if ( stack.empty() )
    {
        element->pos = ORIGIN;
        element->size = { short ( MAXSCREENX  ), short ( MAXSCREENY ) };
    }

    element->size.X = min ( element->size.X, width );
    element->size.Y = min ( element->size.Y, height );
    element->initialize();

    stack.push ( ConsoleUi::ElementPtr ( element ) );
}

void ConsoleUi::pushRight ( ConsoleUi::Element *element, short width, short height )
{
    if ( !stack.empty() )
    {
        element->pos = { short ( stack.top()->pos.X + stack.top()->size.X ), stack.top()->pos.Y };
        element->size = { short ( MAXSCREENX ), short ( MAXSCREENY ) };
        element->size -= element->pos;
    }

    push ( element, width, height );
}

void ConsoleUi::pushBelow ( ConsoleUi::Element *element, short width, short height )
{
    if ( !stack.empty() )
    {
        element->pos = { stack.top()->pos.X, short ( stack.top()->pos.Y + stack.top()->size.Y ) };
        element->pos.Y -= 1; // Merge horizontal borders
        element->size = { short ( MAXSCREENX ), short ( MAXSCREENY ) };
        element->size -= element->pos;
    }

    push ( element, width, height );
}

void ConsoleUi::pushInFront ( ConsoleUi::Element *element, short width, short height )
{
    if ( !stack.empty() )
    {
        element->pos = stack.top()->pos;
        element->size = { short ( MAXSCREENX ), short ( MAXSCREENY ) };
        element->size -= element->pos;
    }

    push ( element, width, height );
}

void ConsoleUi::pop()
{
    ASSERT ( stack.empty() == false );

    stack.pop();
}

ConsoleUi::Element *ConsoleUi::show()
{
    ASSERT ( stack.empty() == false );

    while ( !stack.empty() && stack.top()->show() )
        stack.pop();

    if ( !stack.empty() )
        return stack.top().get();

    return 0;
}
