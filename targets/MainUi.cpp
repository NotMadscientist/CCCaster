#include "MainUi.h"

#include <JLib/ConsoleCore.h>

using namespace std;


#define ORIGIN ((COORD){0, 0})

// Initial console window dimensions (definition needed for JLIB)
int MAXSCREENX = 80;
int MAXSCREENY = 25;


void MainUi::start()
{
    ConsoleCore::GetInstance()->ClearScreen();

    WindowedMenu menu ( ORIGIN, 5, "asdf",
                        ConsoleFormat::SYSTEM, ConsoleFormat::BLACK | ConsoleFormat::ONBRIGHTWHITE,
                        ConsoleFormat::SYSTEM, ConsoleFormat::SYSTEM );

    for ( int i = 0; i < 20; ++ i )
        menu.Append ( toString ( i ), i );

    menu.EscapeKey ( true );
    menu.Scrollable ( true );

    switch ( menu.Show() )
    {
        case BADMENU:
        case USERESC:
            break;

        case USERDELETE:
        default:
            break;
    }
}
