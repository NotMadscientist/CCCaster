#include "MainUi.h"
#include "Logger.h"
#include "Utilities.h"
#include "Version.h"
#include "ConsoleUi.h"
#include "Constants.h"

#include <iostream>
#include <fstream>

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
                initialConfig.isTraining = result;

                run ( address, initialConfig );
            }
        }
        else
        {
            run ( address, initialConfig );
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

        netplayConfig.flags = NetplayConfig::Offline;
        if ( menu->resultInt )
            netplayConfig.flags |= NetplayConfig::Training;

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

static string fetchGameUserName()
{
    string line, buffer;
    ifstream fin ( CC_NETWORK_CONFIG_FILE );

    while ( getline ( fin, line ) )
    {
        buffer.clear();

        if ( line.substr ( 0, sizeof ( CC_NETWORK_USERNAME_KEY ) - 1 ) == CC_NETWORK_USERNAME_KEY )
        {
            // Find opening quote
            size_t pos = line.find ( '"' );
            if ( pos == string::npos )
                break;

            buffer = line.substr ( pos + 1 );

            // Find closing quote
            pos = buffer.rfind ( '"' );
            if ( pos == string::npos )
                break;

            buffer.erase ( pos );
            break;
        }
    }

    fin.close();
    return buffer;
}

MainUi::MainUi()
{
    config.putInteger ( "alertOnConnect", 3 );
    config.putString ( "alertWavFile", SYSTEM_DEFAULT_ALERT );
    config.putInteger ( "autoRehostOnError", 5 );
    config.putString ( "displayName", fetchGameUserName() );
    // config.putInteger ( "highCpuPriority", 1 );
    // config.putInteger ( "joystickDeadzone", 25000 );
    config.putInteger ( "lastUsedPort", -1 );
    config.putInteger ( "spectatorCap", -1 );

    initialConfig.localName = config.getString ( "displayName" );
}

void MainUi::main ( RunFuncPtr run )
{
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

        // Update UI internal state here
        initialConfig.localName = config.getString ( "displayName" );

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

static int computeDelay ( const Statistics& latency )
{
    return ( int ) ceil ( latency.getMean() / ( 1000.0 / 60 ) );
}

static string formatStats ( const PingStats& pingStats )
{
    return toString (
               "Ping: %.2f ms \n"
#ifndef NDEBUG
               "Worst: %.2f ms\n"
               "StdErr: %.2f ms\n"
               "StdDev: %.2f ms\n"
               "Packet Loss: %d%%\n"
#endif
               "Delay: %d\n",
               pingStats.latency.getMean(),
#ifndef NDEBUG
               pingStats.latency.getWorst(),
               pingStats.latency.getStdErr(),
               pingStats.latency.getStdDev(),
               pingStats.packetLoss,
#endif
               computeDelay ( pingStats.latency ) );
}

void MainUi::display ( const string& message )
{
    if ( !ui )
        ui.reset ( new ConsoleUi ( TITLE ) );

    ui->pushInFront ( new ConsoleUi::TextBox ( message ), { 1, 0 }, true ); // Expand width and clear
}

bool MainUi::accepted ( const InitialConfig& initialConfig, const PingStats& pingStats )
{
    // netplayConfig.delay = 4;
    // netplayConfig.rollback = 30;
    // netplayConfig.training = 0;
    // netplayConfig.hostPlayer = 1;

    ASSERT ( ui.get() != 0 );

    ui->pushInFront ( new ConsoleUi::TextBox (
                          initialConfig.getAcceptMessage ( "connected" ) + "\n"
                          + formatStats ( pingStats ) ), { 1, 0 }, true ); // Expand width and clear

    // TODO implement me
    Sleep ( 10000 );

    ui->pop();

    return false;
}

bool MainUi::connected ( const InitialConfig& initialConfig, const PingStats& pingStats )
{
    ASSERT ( ui.get() != 0 );

    ui->pushInFront ( new ConsoleUi::TextBox (
                          initialConfig.getConnectMessage ( "Connected" ) + "\n"
                          + formatStats ( pingStats ) ), { 1, 0 }, true ); // Expand width and clear

    // TODO implement me
    Sleep ( 10000 );

    ui->pop();

    return false;
}

const void *MainUi::getConsoleWindow() const
{
    if ( !ui )
        return 0;

    return ui->getConsoleWindow();
}
