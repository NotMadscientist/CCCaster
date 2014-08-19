#include "MainUi.h"
#include "Logger.h"
#include "Utilities.h"

#include <JLib/ConsoleCore.h>

#include <iostream>

using namespace std;


// Initial console window dimensions (definitions needed for JLib)
int MAXSCREENX = 80;
int MAXSCREENY = 25;


MainUi::MainUi()
{
    ui.pushRight ( new ConsoleUi::Menu ( "Main",
    { "Netplay", "Spectate", "Broadcast", "Training", "Controls", "Settings" }, "Quit" ) );
}

bool MainUi::mainMenu()
{
    ConsoleCore::GetInstance()->ClearScreen();

    ConsoleUi::Element *element = ui.show();

    switch ( element->resultInt )
    {
        case 0:
            break;

        case 1:
            break;

        default:
            return false;
    }

    return true;
}

bool MainUi::acceptMenu ( const Statistics& stats )
{
    PRINT ( "latency=%.2f ms; jitter=%.2f ms", stats.getMean(), stats.getJitter() );

    int value;

    PRINT ( "Enter delay:" );

    cin >> value;
    setup.delay = value;

    PRINT ( "Enter training mode:" );

    cin >> value;
    setup.training = value;

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
    return "";
}

NetplaySetup MainUi::getNetplaySetup() const
{
    return setup;
}
