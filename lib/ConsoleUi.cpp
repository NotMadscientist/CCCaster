#include "ConsoleUi.hpp"
#include "StringUtils.hpp"
#include "Algorithms.hpp"

#include <windows.h>

#include <climits>

using namespace std;


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


// Initial console window dimensions (definitions needed for JLib)
int MAXSCREENX = 80;
int MAXSCREENY = 25;


// Definitions of text sizes
const string ConsoleUi::Ellipsis = "...";
const string ConsoleUi::MinText = "A...";
const string ConsoleUi::MinMenuItem = "[1] A...";
const string ConsoleUi::Borders = "**";
const string ConsoleUi::PaddedBorders = "*  *";


// TextBox
ConsoleUi::TextBox::TextBox ( const string& text ) : ConsoleUi::Element ( false ), text ( trimmed ( text, "\n" ) ) {}

void ConsoleUi::TextBox::initialize()
{
    stringstream ss ( text );
    string line;

    while ( getline ( ss, line ) )
    {
        if ( line.size() + PaddedBorders.size() > ( size_t ) _size.X )
        {
            vector<string> tokens = split ( line );
            line.clear();

            for ( const string& token : tokens )
            {
                const size_t prefix = ( line.empty() ? 0 : line.size() + 1 );

                if ( prefix + token.size() + PaddedBorders.size() > ( size_t ) _size.X )
                {
                    _lines.push_back ( " " + line + " " );
                    line.clear();
                }

                if ( line.empty() )
                    line = token;
                else
                    line += " " + token;
            }

            if ( ! line.empty() )
                _lines.push_back ( " " + line + " " );
        }
        else
        {
            _lines.push_back ( " " + line + " " );
        }
    }

    if ( _lines.size() + BordersHeight > ( size_t ) _size.Y )
    {
        _lines.resize ( _size.Y - BordersHeight - 1 );
        _lines.push_back ( " ... " );
    }

    size_t longestLine = 0;
    for ( const string& line : _lines )
        if ( line.size() > longestLine )
            longestLine = line.size();

    ASSERT ( ( size_t ) _size.X >= longestLine + Borders.size() );
    ASSERT ( ( size_t ) _size.Y >= _lines.size() + BordersHeight );

    if ( ! _expand.X )
        _size.X = longestLine + Borders.size();

    if ( ! _expand.Y )
        _size.Y = _lines.size() + BordersHeight;
}

void ConsoleUi::TextBox::show()
{
    LOG ( "text='%s'; pos=%s; size=%s", text, _pos, _size );

    CharacterBox::Draw ( _pos, _pos + _size, '*' );
    for ( size_t i = 0; i < _lines.size(); ++i )
        ConsoleCore::GetInstance()->Prints ( _lines[i], false, 0, _pos.X + 1, _pos.Y + 1 + i );
}

// Menu
ConsoleUi::Menu::Menu ( const string& title, const vector<string>& items, const string& lastItem )
    : ConsoleUi::Element ( true ), title ( title ), items ( items ), lastItem ( lastItem )
    , menu ( _pos, items.size() + ( lastItem.empty() ? 0 : 1 ), title, THEME ) {}

ConsoleUi::Menu::Menu ( const vector<string>& items, const string& lastItem )
    : Menu ( "", items, lastItem ) {}

void ConsoleUi::Menu::setPosition ( int position )
{
    if ( position < 0 )
        position = 0;
    else if ( position >= ( int ) menu.Count() )
        position = menu.Count() - 1;

    menu.SelectedItem ( position );
}

void ConsoleUi::Menu::setEscape ( bool enabled )
{
    menu.EnableEscape ( enabled );
}

void ConsoleUi::Menu::setDelete ( int enabled )
{
    menu.EnableDelete ( enabled );
}

void ConsoleUi::Menu::overlayCurrentPosition ( const string& text, bool selected )
{
    const COORD pos = menu.CursorPosition();
    const size_t i = menu.SelectedValue();
    const string padded = items[i].substr ( 0, 4 ) + text;

    // Pad remaining text if necessary
    string remaining;
    if ( 4 + text.size() < items[i].size() )
        remaining = string ( items[i].substr ( 4 + text.size() ).size(), ' ' );

    ConsoleCore::GetInstance()->Prints ( " " + shortenWithEllipsis ( padded + remaining ) + " ",
                                         false, ( selected ? &menu.SelectionFormat() : 0 ), pos.X, pos.Y );
}

void ConsoleUi::Menu::initialize()
{
    ASSERT ( ( size_t ) _size.X >= MinMenuItem.size() + PaddedBorders.size() );
    ASSERT ( ( size_t ) _size.Y > BordersHeight + ( title.empty() ? 0 : 2 ) );
    ASSERT ( items.size() <= MaxMenuItems );

    if ( ! title.empty() )
        menu.Title ( " " + shortenWithEllipsis ( title ) + " " );

    for ( size_t i = 0; i < items.size(); ++i )
    {
        if ( i < 9 )
            items[i] = format ( "[%d] %s", i + 1, items[i] );
        else
            items[i] = format ( "[%c] %s", 'A' + i - 9, items[i] );

        menu.Append ( " " + shortenWithEllipsis ( items[i] ) + " ", i );
    }

    if ( ! lastItem.empty() )
    {
        lastItem = format ( "[0] %s", lastItem );
        menu.Append ( " " + shortenWithEllipsis ( lastItem ) + " ", items.size() );
    }

    // TODO this is broken because WindowedMenu::SelectedItem doesn't work when the menu is scrolled off-screen
    // // Limit the menu vertical display size
    // menu.MaxToShow ( min ( _size.Y - BordersHeight - ( title.empty() ? 0 : 2 ), menu.Count() ) );

    ASSERT ( menu.Count() <= _size.Y - BordersHeight - ( title.empty() ? 0 : 2 ) );

    // Menus are NEVER expanded
    _size.X = max ( menu.LongestItem() + Borders.size(), title.size() + Borders.size() );
    _size.Y = BordersHeight + ( title.empty() ? 0 : 2 ) + items.size() + ( lastItem.empty() ? 0 : 1 );

    menu.Origin ( _pos );
    menu.Scrollable ( true );
}

void ConsoleUi::Menu::show()
{
    LOG ( "title='%s'; pos=%s; size=%s", title, _pos, _size );

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

string ConsoleUi::Menu::shortenWithEllipsis ( const string& text )
{
    if ( text.size() + PaddedBorders.size() > ( size_t ) _size.X )
        return text.substr ( 0, _size.X - PaddedBorders.size() - Ellipsis.size() ) + Ellipsis;

    return text;
}

// Prompt
ConsoleUi::Prompt::Prompt ( PromptTypeString, const string& title )
    : ConsoleUi::Element ( true ), title ( title ), isIntegerPrompt ( false ) {}

ConsoleUi::Prompt::Prompt ( PromptTypeInteger, const string& title )
    : ConsoleUi::Element ( true ), title ( title ), isIntegerPrompt ( true ) {}

void ConsoleUi::Prompt::setInitial ( int initial )
{
    if ( ! isIntegerPrompt )
        return;

    resultInt = initial;
}

void ConsoleUi::Prompt::setInitial ( const string& initial )
{
    if ( isIntegerPrompt )
        return;

    resultStr = initial;
}

void ConsoleUi::Prompt::initialize()
{
    ASSERT ( ( size_t ) _size.X >= title.size() + PaddedBorders.size() );
    ASSERT ( ( size_t ) _size.Y > BordersHeight + ( title.empty() ? 0 : 2 ) );

    if ( ! _expand.X )
        _size.X = title.size() + PaddedBorders.size();

    // Prompts are NEVER expanded vertically
    _size.Y = 1 + BordersHeight + ( title.empty() ? 0 : 2 );
}

void ConsoleUi::Prompt::show()
{
    LOG ( "title='%s'; pos=%s; size=%s", title, _pos, _size );

    CharacterBox::Draw ( _pos, _pos + _size, '*' );
    ConsoleCore *cc = ConsoleCore::GetInstance();

    if ( ! title.empty() )
    {
        cc->Prints ( " " + title + " ", false, 0, _pos.X + 1, _pos.Y + 1 );
        cc->Prints ( string ( _size.X - Borders.size(), '*' ), false, 0, _pos.X + 1, _pos.Y + 2 );
    }

    COORD scanPos = { short ( _pos.X + 2 ), short ( _pos.Y + ( title.empty() ? 1 : 3 ) ) };
    cc->CursorPosition ( &scanPos );

    if ( isIntegerPrompt )
    {
        if ( cc->ScanNumber ( scanPos, resultInt, min ( maxDigits, _size.X - PaddedBorders.size() ),
                              allowNegative, resultInt != INT_MIN ) )
            LOG ( "resultInt=%d", resultInt );
        else
            resultInt = INT_MIN;
    }
    else
    {
        if ( cc->ScanString ( scanPos, resultStr, _size.X - PaddedBorders.size() ) )
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

// ProgressBar
ConsoleUi::ProgressBar::ProgressBar ( const string& title, size_t length )
    : ConsoleUi::Element ( false ), title ( title ), length ( length ) {}

ConsoleUi::ProgressBar::ProgressBar ( size_t length )
    : ConsoleUi::Element ( false ), title ( "" ), length ( length ) {}

void ConsoleUi::ProgressBar::update ( size_t progress ) const
{
    string bar ( clamped ( progress, 0u, length ), '.' );
    if ( progress < length )
        bar += string ( clamped ( length - progress, 0u, length ), ' ' );

    ConsoleCore::GetInstance()->Prints ( bar, false, 0, _pos.X + 2, _pos.Y + _size.Y - 2 );
}

void ConsoleUi::ProgressBar::initialize()
{
    ASSERT ( ( size_t ) _size.X >= max ( title.size(), length ) + PaddedBorders.size() );
    ASSERT ( ( size_t ) _size.Y > BordersHeight + ( title.empty() ? 0 : 2 ) );

    // Progress bars are NEVER expanded
    _size.X = max ( title.size(), length ) + PaddedBorders.size();
    _size.Y = 1 + BordersHeight + ( title.empty() ? 0 : 2 );
}

void ConsoleUi::ProgressBar::show()
{
    LOG ( "title='%s'; length=%u; pos=%s; size=%s", title, length, _pos, _size );

    CharacterBox::Draw ( _pos, _pos + _size, '*' );
    ConsoleCore *cc = ConsoleCore::GetInstance();

    if ( ! title.empty() )
    {
        cc->Prints ( " " + title + " ", false, 0, _pos.X + 1, _pos.Y + 1 );
        cc->Prints ( string ( _size.X - Borders.size(), '*' ), false, 0, _pos.X + 1, _pos.Y + 2 );
    }
}

// ConsoleUi
ConsoleUi::ConsoleUi ( const string& title, bool isWine )
{
    SetConsoleTitle ( title.c_str() );
    SetConsoleOutputCP ( 437 );

    if ( isWine )
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

void ConsoleUi::pushRight ( ConsoleUi::Element *element, const COORD& expand )
{
    if ( ! _stack.empty() )
    {
        element->_pos = { short ( _stack.back()->_pos.X + _stack.back()->_size.X ), _stack.back()->_pos.Y };
        element->_size = MAX_SCREEN_SIZE;
        element->_size -= element->_pos;
    }

    initalizeAndPush ( element, expand );
}

void ConsoleUi::pushBelow ( ConsoleUi::Element *element, const COORD& expand )
{
    if ( ! _stack.empty() )
    {
        element->_pos = { _stack.back()->_pos.X, short ( _stack.back()->_pos.Y + _stack.back()->_size.Y ) };
        element->_pos.Y -= 1; // Merge horizontal borders
        element->_size = MAX_SCREEN_SIZE;
        element->_size -= element->_pos;
    }

    initalizeAndPush ( element, expand );
}

void ConsoleUi::pushInFront ( ConsoleUi::Element *element, const COORD& expand )
{
    if ( ! _stack.empty() )
    {
        element->_pos = _stack.back()->_pos;
        element->_size = MAX_SCREEN_SIZE;
        element->_size -= element->_pos;
    }

    initalizeAndPush ( element, expand );
}

void ConsoleUi::pushInFront ( ConsoleUi::Element *element, const COORD& expand, bool shouldClearTop )
{
    if ( shouldClearTop )
        clearTop();

    pushInFront ( element, expand );
}

void ConsoleUi::pop()
{
    ASSERT ( _stack.empty() == false );

    clearTop();
    _stack.pop_back();
}

ConsoleUi::Element *ConsoleUi::popUntilUserInput ( bool clearPoppedElements )
{
    ASSERT ( _stack.empty() == false );

    while ( ! _stack.empty() )
    {
        if ( _stack.back()->requiresUser )
        {
            _stack.back()->show();
            break;
        }

        if ( clearPoppedElements )
            pop();
        else
            _stack.pop_back();
    }

    ASSERT ( _stack.empty() == false );
    ASSERT ( _stack.back().get() != 0 );

    return _stack.back().get();
}

void ConsoleUi::popNonUserInput()
{
    while ( !_stack.empty() && !top()->requiresUser )
        pop();
}

bool ConsoleUi::hasBorder() const
{
    for ( size_t i = 2; i < _stack.size(); ++i )
    {
        if ( top()->_pos.X >= _stack[_stack.size() - i]->_pos.X
                && ( top()->_pos.X + top()->_size.X
                     <= _stack[_stack.size() - i]->_pos.X + _stack[_stack.size() - i]->_size.X )
                && top()->_pos.Y > _stack[_stack.size() - i]->_pos.Y )
        {
            return true;
        }
    }

    return false;
}

void ConsoleUi::clearTop() const
{
    if ( _stack.empty() || _stack.size() == 1 )
        ConsoleCore::GetInstance()->ClearScreen();
    else if ( hasBorder() )
        CharacterBox::Draw ( { top()->_pos.X, short ( top()->_pos.Y + 1 ) }, MAX_SCREEN_SIZE, ' ' );
    else
        CharacterBox::Draw ( top()->_pos, MAX_SCREEN_SIZE, ' ' );
}

void ConsoleUi::clearBelow ( bool preserveBorder ) const
{
    if ( _stack.empty() )
    {
        ConsoleCore::GetInstance()->ClearScreen();
    }
    else
    {
        const COORD pos = { top()->_pos.X, short ( top()->_pos.Y + top()->_size.Y - ( preserveBorder ? 0 : 1 ) ) };
        CharacterBox::Draw ( pos, MAX_SCREEN_SIZE, ' ' );
    }
}

void ConsoleUi::clearRight() const
{
    if ( _stack.empty() )
    {
        ConsoleCore::GetInstance()->ClearScreen();
    }
    else if ( hasBorder() )
    {
        const COORD pos = { short ( top()->_pos.X + top()->_size.X ), short ( top()->_pos.Y + 1 ) };
        CharacterBox::Draw ( pos, MAX_SCREEN_SIZE, ' ' );
    }
    else
    {
        CharacterBox::Draw ( { short ( top()->_pos.X + top()->_size.X ), top()->_pos.Y }, MAX_SCREEN_SIZE, ' ' );
    }
}

void ConsoleUi::clearAll()
{
    clearScreen();
    _stack.clear();
}

void ConsoleUi::clearScreen()
{
    ConsoleCore::GetInstance()->ClearScreen();
}

void *ConsoleUi::getConsoleWindow()
{
    static void *consoleWindow = 0;

    if ( ! consoleWindow )
        consoleWindow = GetConsoleWindow();

    return consoleWindow;
}

void ConsoleUi::initalizeAndPush ( ConsoleUi::Element *element, const COORD& expand )
{
    if ( _stack.empty() )
    {
        element->_pos = ORIGIN;
        element->_size = MAX_SCREEN_SIZE;
    }

    element->_expand = expand;
    element->initialize();

    _stack.push_back ( ElementPtr ( element ) );

    if ( ! element->requiresUser )
        element->show();
}
