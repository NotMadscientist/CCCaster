#include "ConsoleUi.h"

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


void ConsoleUi::initialize ( const string& title )
{
    consoleWindow = GetConsoleWindow();
    SetConsoleTitle ( title.c_str() );
    SetConsoleOutputCP ( 437 );

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
    HANDLE hnd = GetStdHandle ( STD_OUTPUT_HANDLE );

    // Get Number of console fonts
    DWORD num = GetNumberOfConsoleFonts();

    // Setup array
    vector<CONSOLE_FONT_INFO> fonts ( num );

    // Get font info
    GetConsoleFontInfo ( hnd, false, num, &fonts[0] );

    for ( size_t i = 0; i < num; ++i )
    {
        // Get console font Size
        fonts[i].dwFontSize = GetConsoleFontSize ( hnd, fonts[i].nFont );

        // Find the right font size
        if ( fonts[i].dwFontSize.X == 8 && fonts[i].dwFontSize.Y == 12 )
        {
            // Set that font
            SetConsoleFont ( hnd, fonts[i].nFont );
            break;
        }
    }
}
