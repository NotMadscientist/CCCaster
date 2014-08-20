#include "MainUi.h"
#include "Logger.h"
#include "Utilities.h"
#include "Version.h"
#include "ConsoleUi.h"

#include <iostream>

using namespace std;


#define TITLE "CCCaster " VERSION


void MainUi::initialize()
{
    ui.reset ( new ConsoleUi ( TITLE ) );
    ui->pushRight ( new ConsoleUi::Menu ( TITLE,
    { "Netplay", "Spectate", "Broadcast", "Offline", "Controls", "Settings" }, "Quit" ) );
}

bool MainUi::mainMenu()
{
    ConsoleCore::GetInstance()->ClearScreen();

    // ui->pushBelow ( new ConsoleUi::TextBox ( "foo foofoo\n asdf  asdf asd\n a\n \n\n" ) );
    // ui->pushRight ( new ConsoleUi::TextBox ( "foo foofoo\n asdf  asdf asd\n a\n \n\n" ) );

    ConsoleUi::Element *menu = ui->show();

    switch ( menu->resultInt )
    {
        // Netplay
        case 0:
            break;

        // Spectate
        case 1:
            break;

        // Broadcast
        case 2:
            break;

        // Offline
        case 3:
            ui->pushRight ( new ConsoleUi::Menu ( "Offline", { "Versus", "Training" }, "Cancel" ) );
            menu = ui->show();
            ui->pop();
            break;

        // Controls
        case 4:
            break;

        // Settings
        case 5:
            break;

        // Quit
        case 6:
        case BADMENU:
        case USERESC:
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
