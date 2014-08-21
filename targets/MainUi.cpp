#include "MainUi.h"
#include "Logger.h"
#include "Utilities.h"
#include "Version.h"
#include "ConsoleUi.h"

#include <iostream>

using namespace std;


#define TITLE "CCCaster " VERSION

// System sound prefix and default alert
#define SYSTEM_ALERT_PREFEX "System"
#define SYSTEM_DEFAULT_ALERT "SystemDefault"


void MainUi::netplay ( RunFuncPtr run )
{
}

void MainUi::spectate ( RunFuncPtr run )
{
    ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptString,
                                            "Enter/paste <ip>:<port> to spectate:" ) );
    ui->pop();
}

void MainUi::broadcast ( RunFuncPtr run )
{
    ui->pushRight ( new ConsoleUi::Menu ( "Broadcast", { "Versus", "Training" }, "Cancel" ) );
    ui->pop();
}

void MainUi::offline ( RunFuncPtr run )
{
    ui->pushRight ( new ConsoleUi::Menu ( "Offline", { "Versus", "Training" }, "Cancel" ) );

    for ( bool finished = false; !finished; )
    {
        ConsoleUi::Element *menu = ui->popUntilMenu();

        ASSERT ( menu != 0 );

        if ( menu->resultInt < 0 || menu->resultInt > 1 )
            break;

        netplaySetup.training = menu->resultInt;

        ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter delay:", 0, false ) );

        while ( !finished )
        {
            menu = ui->popUntilMenu();

            ASSERT ( menu != 0 );

            if ( menu->resultInt < 0 )
                break;

            if ( menu->resultInt >= 0xFF )
            {
                ui->pushBelow ( new ConsoleUi::TextBox ( "Delay must be less than 255!" ) );
                continue;
            }

            netplaySetup.delay = menu->resultInt;

            run ( "", netplaySetup );
            finished = true;
        }

        ui->pop();
    }

    ui->pop();
}

void MainUi::controls()
{
}

void MainUi::settings()
{
}

void MainUi::main ( RunFuncPtr run )
{
    config.putInteger ( "alertOnConnect", 3 );
    config.putString ( "alertWavFile", SYSTEM_DEFAULT_ALERT );
    config.putInteger ( "autoRehostOnError", 5 );
    // config.putString ( "displayName", fetchGameUserName() );
    // config.putInteger ( "highCpuPriority", 1 );
    // config.putInteger ( "joystickDeadzone", 25000 );
    config.putInteger ( "lastUsedPort", -1 );
    config.putInteger ( "spectatorCap", -1 );

    ui.reset ( new ConsoleUi ( TITLE ) );

    ui->pushRight ( new ConsoleUi::Menu ( TITLE,
    { "Netplay", "Spectate", "Broadcast", "Offline", "Controls", "Settings" }, "Quit" ) );

    for ( ;; )
    {
        ui->clear();

        if ( !sessionError.empty() && !sessionMessage.empty() )
            ui->pushRight ( new ConsoleUi::TextBox ( sessionError + "\n" + sessionMessage ) );
        else if ( !sessionError.empty() )
            ui->pushRight ( new ConsoleUi::TextBox ( sessionError ) );
        else if ( !sessionMessage.empty() )
            ui->pushRight ( new ConsoleUi::TextBox ( sessionMessage ) );

        sessionError.clear();
        sessionMessage.clear();

        ConsoleUi::Element *menu = ui->popUntilMenu();

        ASSERT ( menu != 0 );

        switch ( menu->resultInt )
        {
            case 0:
                netplay ( run );
                break;

            case 1:
                spectate ( run );
                break;

            case 2:
                broadcast ( run );
                break;

            case 3:
                offline ( run );
                break;

            case 4:
                controls();
                break;

            case 5:
                settings();
                break;

            default:
                return;
        }
    }
}

bool MainUi::accepted ( const Statistics& stats )
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

    netplaySetup.hostPlayer = 1 + ( rand() % 2 );

    cin >> value;
    return value;
}

bool MainUi::connected ( const Statistics& stats )
{
    PRINT ( "latency=%.2f ms; jitter=%.2f ms", stats.getMean(), stats.getJitter() );

    int value;

    PRINT ( "Connect?" );

    cin >> value;
    return value;
}
