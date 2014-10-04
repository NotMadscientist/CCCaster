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
    ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptString,
                                            "Enter/paste <ip>:<port> to join or <port> to host:",
                                            ( address.addr.empty() && !address.empty()
                                                    ? address.str().substr ( 1 )
                                                    : address.str() ) ),
    { 1, 0 } ); // Expand width

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUser();

        if ( menu->resultStr.empty() )
            break;

        try
        {
            address = menu->resultStr;
        }
        catch ( const Exception& err )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( err.str() ), { 1, 0 } ); // Expand width
            continue;
        }

        if ( address.addr.empty() )
        {
            ui->pushBelow ( new ConsoleUi::Menu ( "Netplay", { "Versus", "Training" }, "Cancel" ) );

            menu = ui->popUntilUser();
            int result = menu->resultInt;

            ui->pop();

            if ( result >= 0 && result <= 1 )
            {
                netplaySetup.training = result;

                ui->pushBelow ( new ConsoleUi::TextBox ( "Hosting..." ), { 1, 0 } ); // Expand width

                // TODO get external IP
                // TODO copy it to clipboard

                // run ( address, netplaySetup );
                Sleep ( 1000 );

                ui->pop();
            }
        }
        else
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( "Connecting..." ), { 1, 0 } ); // Expand width

            // run ( address, NetplaySetup() );
            Sleep ( 1000 );

            ui->pop();
        }
    }

    ui->pop();
}

void MainUi::spectate ( RunFuncPtr run )
{
    ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptString,
                                            "Enter/paste <ip>:<port> to spectate:" ) );

    // TODO implement me

    ui->pop();
}

void MainUi::broadcast ( RunFuncPtr run )
{
    ui->pushRight ( new ConsoleUi::Menu ( "Broadcast", { "Versus", "Training" }, "Cancel" ) );

    // TODO implement me

    ui->pop();
}

void MainUi::offline ( RunFuncPtr run )
{
    ui->pushRight ( new ConsoleUi::Menu ( "Offline", { "Versus", "Training" }, "Cancel" ) );

    for ( bool finished = false; !finished; )
    {
        ConsoleUi::Element *menu = ui->popUntilUser();

        if ( menu->resultInt < 0 || menu->resultInt > 1 )
            break;

        netplaySetup.training = menu->resultInt;

        ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter delay:", 0, false, 3 ),
        { 1, 0 } ); // Expand width

        while ( !finished )
        {
            menu = ui->popUntilUser();

            if ( menu->resultInt < 0 )
                break;

            if ( menu->resultInt >= 0xFF )
            {
                ui->pushBelow ( new ConsoleUi::TextBox ( "Delay must be less than 255!" ), { 1, 0 } ); // Expand width
                continue;
            }

            netplaySetup.delay = menu->resultInt;

            // TODO remove me testing
            // netplaySetup.rollback = 30;

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

        ConsoleUi::Element *menu = ui->popUntilUser();

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
    LOG ( "latency=%.2f ms; jitter=%.2f ms", stats.getMean(), stats.getJitter() );

    // int value;

    // PRINT ( "Enter delay:" );

    // cin >> value;
    // netplaySetup.delay = value;

    // PRINT ( "Enter training mode:" );

    // cin >> value;
    // netplaySetup.training = value;

    // PRINT ( "Connect?" );

    // netplaySetup.hostPlayer = 1 + ( rand() % 2 );

    // cin >> value;
    // return value;

    netplaySetup.delay = 4;
    netplaySetup.rollback = 30;
    netplaySetup.training = 0;
    netplaySetup.hostPlayer = 1;

    // TODO implement me

    return true;
}

bool MainUi::connected ( const Statistics& stats )
{
    LOG ( "latency=%.2f ms; jitter=%.2f ms", stats.getMean(), stats.getJitter() );

    // int value;

    // PRINT ( "Connect?" );

    // cin >> value;
    // return value;

    // TODO implement me

    return true;
}
