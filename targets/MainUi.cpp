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
        ConsoleUi::Element *menu = ui->popUntilUserInput();

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

            menu = ui->popUntilUserInput();
            int result = menu->resultInt;

            ui->pop();

            if ( result >= 0 && result <= 1 )
            {
                netplayConfig.training = result;

                ui->pushBelow ( new ConsoleUi::TextBox ( "Hosting..." ), { 1, 0 } ); // Expand width

                // TODO get external IP
                // TODO copy it to clipboard

                run ( address, netplayConfig );

                ui->pop();
            }
        }
        else
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( "Connecting..." ), { 1, 0 } ); // Expand width

            run ( address, NetplayConfig() );

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
        ConsoleUi::Element *menu = ui->popUntilUserInput();

        if ( menu->resultInt < 0 || menu->resultInt > 1 )
            break;

        netplayConfig.training = menu->resultInt;

        ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter delay:", 0, false, 3 ),
        { 1, 0 } ); // Expand width

        while ( !finished )
        {
            menu = ui->popUntilUserInput();

            if ( menu->resultInt < 0 )
                break;

            if ( menu->resultInt >= 0xFF )
            {
                ui->pushBelow ( new ConsoleUi::TextBox ( "Delay must be less than 255!" ), { 1, 0 } ); // Expand width
                continue;
            }

            netplayConfig.delay = menu->resultInt;

            // TODO remove me testing
            // netplayConfig.rollback = 30;

            run ( "", netplayConfig );
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

        ConsoleUi::Element *menu = ui->popUntilUserInput();

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

static int computeDelay ( const Statistics& stats )
{
    return ( int ) ceil ( stats.getMean() / ( 1000.0 / 60 ) );
}

static ConsoleUi::TextBox *initialConfigTextBox ( const InitialConfig& initialConfig )
{
    return new ConsoleUi::TextBox ( toString (
                                        "Connected\n"
                                        "Ping: %.2f ms \n"
#ifndef NDEBUG
                                        "Worst: %.2f ms\n"
                                        "StdErr: %.2f ms\n"
                                        "StdDev: %.2f ms\n"
#endif
                                        "Delay: %d\n",
                                        initialConfig.stats.getMean(),
#ifndef NDEBUG
                                        initialConfig.stats.getWorst(),
                                        initialConfig.stats.getStdErr(),
                                        initialConfig.stats.getStdDev(),
#endif
                                        computeDelay ( initialConfig.stats ) ) );
}

bool MainUi::accepted ( const InitialConfig& initialConfig )
{
    // netplayConfig.delay = 4;
    // netplayConfig.rollback = 30;
    // netplayConfig.training = 0;
    // netplayConfig.hostPlayer = 1;

    // Reset and clear the screen if launched directly from command line args
    if ( !ui )
    {
        ui.reset ( new ConsoleUi ( TITLE ) );
        ui->clear();
    }

    ui->pushInFront ( initialConfigTextBox ( initialConfig ), { 1, 0 } ); // Expand width

    // TODO implement me
    Sleep ( 10000 );

    ui->pop();

    return false;
}

bool MainUi::connected ( const InitialConfig& initialConfig )
{
    // Reset and clear the screen if launched directly from command line args
    if ( !ui )
    {
        ui.reset ( new ConsoleUi ( TITLE ) );
        ui->clear();
    }

    ui->pushInFront ( initialConfigTextBox ( initialConfig ), { 1, 0 } ); // Expand width

    // TODO implement me
    Sleep ( 10000 );

    ui->pop();

    return false;
}
