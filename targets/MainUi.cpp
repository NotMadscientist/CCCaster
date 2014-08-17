#include "MainUi.h"
#include "Logger.h"

#include <JLib/ConsoleCore.h>

#include <iostream>

using namespace std;


#define ORIGIN ( ( COORD ) { 0, 0 } )

// Initial console window dimensions (definitions needed for JLib)
int MAXSCREENX = 80;
int MAXSCREENY = 25;


// void MainUi::mainMenu()
// {
//     ConsoleCore::GetInstance()->ClearScreen();

//     WindowedMenu menu ( ORIGIN,                                                 // location
//                         5,                                                      // max items to show
//                         "asdf",                                                 // title
//                         ConsoleFormat::SYSTEM,                                  // unselected colour
//                         ConsoleFormat::BLACK | ConsoleFormat::ONBRIGHTWHITE,    // selected colour
//                         ConsoleFormat::SYSTEM,                                  // outline colour
//                         ConsoleFormat::SYSTEM );                                // background colour

//     for ( int i = 0; i < 20; ++ i )
//         menu.Append ( toString ( i ), i );

//     menu.EscapeKey ( true );
//     menu.Scrollable ( true );

//     switch ( menu.Show() )
//     {
//         case BADMENU:
//         case USERESC:
//             break;

//         case USERDELETE:
//         default:
//             break;
//     }
// }


static string address;

static NetplaySetup netplaySetup;

bool MainUi::mainMenu()
{
    PRINT ( "Enter address:" );

    getline ( cin, address );

    return true;
}

bool MainUi::acceptMenu ( const Statistics& stats )
{
    PRINT ( "latency=%.2f ms; jitter=%.2f ms", stats.getMean(), stats.getJitter() );

    int value;

    PRINT ( "Enter delay:" );

    cin >> value;
    netplaySetup.delay = value;

    PRINT ( "Enter training mode:" );

    cin >> value;
    netplaySetup.training = value;

    PRINT ( "Connect?" );

    cin >> value;
    return value;
}

bool MainUi::connectMenu ( const Statistics& stats )
{
    PRINT ( "latency=%.2f ms; jitter=%.2f ms", stats.getMean(), stats.getJitter() );

    int value;

    PRINT ( "Connect?" );

    cin >> value;
    return value;
}

string MainUi::getMainAddress() const
{
    return address;
}

NetplaySetup MainUi::getNetplaySetup() const
{
    return netplaySetup;
}
