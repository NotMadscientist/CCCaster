#include "MainUi.h"
#include "Logger.h"
#include "Utilities.h"
#include "Version.h"
#include "ConsoleUi.h"
#include "ProcessManager.h"
#include "ControllerManager.h"

using namespace std;


static const string uiTitle = "CCCaster " + LocalVersion.code;

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
            if ( !gameMode() )
                continue;

            initialConfig.mode.value = ClientMode::Host;
        }
        else
        {
            initialConfig.mode.value = ClientMode::Client;
        }

        run ( address, initialConfig );

        // TODO better way to do this?
        while ( !ui->top()->requiresUser )
            ui->pop();

        if ( !sessionError.empty() )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( sessionError ), { 1, 0 } ); // Expand width
            sessionError.clear();
        }
    }

    ui->pop();
}

void MainUi::spectate ( RunFuncPtr run )
{
    ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptString,
                                            "Enter/paste <ip>:<port> to spectate:" ),
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

        initialConfig.mode.value = ClientMode::Spectate;

        run ( address, initialConfig );

        // TODO better way to do this?
        while ( !ui->top()->requiresUser )
            ui->pop();

        if ( !sessionError.empty() )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( sessionError ), { 1, 0 } ); // Expand width
            sessionError.clear();
        }
    }

    ui->pop();
}

void MainUi::broadcast ( RunFuncPtr run )
{
    ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger,
                                            "Enter/paste <port> to broadcast:",
                                            INT_MIN, false, 5 ) );

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput();

        if ( menu->resultInt == INT_MIN )
            break;

        ASSERT ( menu->resultInt >= 0 );

        if ( menu->resultInt > 0xFFFF )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( "Port must be less than 65536!" ) );
            continue;
        }

        if ( !gameMode() )
            continue;

        netplayConfig.clear();
        netplayConfig.mode.value = ClientMode::Broadcast;
        netplayConfig.mode.flags = initialConfig.mode.flags;
        netplayConfig.delay = 0;
        netplayConfig.hostPlayer = 1;
        netplayConfig.broadcastPort = menu->resultInt;

        run ( "", netplayConfig );

        // TODO better way to do this?
        while ( !ui->top()->requiresUser )
            ui->pop();

        if ( !sessionError.empty() )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( sessionError ), { 1, 0 } ); // Expand width
            sessionError.clear();
        }
    }

    ui->pop();
}

void MainUi::offline ( RunFuncPtr run )
{
    uint8_t delay = 0;
    bool good = false;

    ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter delay:", 0, false, 3 ) );

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput();

        if ( menu->resultInt == INT_MIN )
            break;

        ASSERT ( menu->resultInt >= 0 );

        if ( menu->resultInt >= 0xFF )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( "Delay must be less than 255!" ) );
            continue;
        }

        delay = menu->resultInt;

        if ( gameMode() )
        {
            good = true;
            break;
        }
    }

    ui->pop();

    if ( !good )
        return;

    netplayConfig.clear();
    netplayConfig.mode.value = ClientMode::Offline;
    netplayConfig.mode.flags = initialConfig.mode.flags;
    netplayConfig.delay = delay;
    netplayConfig.hostPlayer = 1;

    // TODO remove me testing
    // netplayConfig.rollback = 30;

    run ( "", netplayConfig );
}

bool MainUi::gameMode()
{
    ui->pushBelow ( new ConsoleUi::Menu ( "Mode", { "Versus", "Training" }, "Cancel" ) );
    int mode = ui->popUntilUserInput()->resultInt;
    ui->pop();

    if ( mode < 0 || mode > 1 )
        return false;

    initialConfig.mode.flags = ( mode == 1 ? ClientMode::Training : 0 );
    return true;
}

void MainUi::doneMapping ( Controller *controller, uint32_t key )
{
    LOG ( "%s: controller=%08x; key=%08x", controller->name, controller, key );
}

void MainUi::controls()
{
    ControllerManager::get().check();

    vector<Controller *> controllers = ControllerManager::get().getControllers();

    vector<string> names;
    names.reserve ( controllers.size() );
    for ( Controller *c : controllers )
        names.push_back ( c->name );

    ui->pushRight ( new ConsoleUi::Menu ( "Controllers", names, "Cancel" ) );

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput();

        if ( menu->resultInt < 0 || menu->resultInt >= ( int ) controllers.size() )
            break;
    }

    ui->pop();
}

void MainUi::settings()
{
}

void MainUi::initialize()
{
    config.putInteger ( "alertOnConnect", 3 );
    config.putString ( "alertWavFile", SYSTEM_DEFAULT_ALERT );
    config.putInteger ( "autoRehostOnError", 5 );
    config.putString ( "displayName", ProcessManager::fetchGameUserName() );
    // config.putInteger ( "highCpuPriority", 1 );
    // config.putInteger ( "joystickDeadzone", 25000 );
    config.putInteger ( "lastUsedPort", -1 );
    config.putInteger ( "spectatorCap", -1 );

    initialConfig.clear();
    initialConfig.localName = config.getString ( "displayName" );
}

void MainUi::main ( RunFuncPtr run )
{
    ControllerManager::get().initialize ( 0 );
    ControllerManager::get().check();

    ui.reset ( new ConsoleUi ( uiTitle ) );
    ui->pushRight ( new ConsoleUi::Menu ( uiTitle,
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

        int main = ui->popUntilUserInput()->resultInt;

        if ( main < 0 || main > 5 )
            break;

        switch ( main )
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
                break;
        }
    }

    ControllerManager::get().deinitialize();
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
        ui.reset ( new ConsoleUi ( uiTitle ) );

    if ( ui->empty() || !ui->top()->requiresUser )
        ui->pushInFront ( new ConsoleUi::TextBox ( message ), { 1, 0 }, true ); // Expand width and clear
    else if ( !ui->top()->expandWidth() )
        ui->pushRight ( new ConsoleUi::TextBox ( message ), { 1, 0 } ); // Expand width
    else if ( !ui->top()->expandHeight() )
        ui->pushBelow ( new ConsoleUi::TextBox ( message ), { 1, 0 } ); // Expand width
    else
        ASSERT_IMPOSSIBLE;
}

bool MainUi::accepted ( const InitialConfig& initialConfig, const PingStats& pingStats )
{
    bool ret = false;

    ASSERT ( ui.get() != 0 );

    ui->pushInFront ( new ConsoleUi::TextBox (
                          initialConfig.getAcceptMessage ( "connected" ) + "\n"
                          + formatStats ( pingStats ) ), { 1, 0 }, true ); // Expand width and clear

    ui->pushBelow ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger,
                                            "Enter delay:", computeDelay ( pingStats.latency ),
                                            false, 3 ), { 1, 0 } ); // Expand width

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput();

        if ( menu->resultInt < 0 )
            break;

        if ( menu->resultInt >= 0xFF )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( "Delay must be less than 255!" ), { 1, 0 } ); // Expand width
            continue;
        }

        netplayConfig.delay = menu->resultInt;
        netplayConfig.hostPlayer = 1; // TODO randomize
        ret = true;
        break;
    }

    ui->pop();
    ui->pop();

    return ret;

    // netplayConfig.delay = 4;
    // // netplayConfig.rollback = 30;
    // netplayConfig.hostPlayer = 1;
    // return true;
}

bool MainUi::connected ( const InitialConfig& initialConfig, const PingStats& pingStats )
{
    bool ret = false;

    ASSERT ( ui.get() != 0 );

    ui->pushInFront ( new ConsoleUi::TextBox (
                          initialConfig.getConnectMessage ( "Connected" ) + "\n"
                          + formatStats ( pingStats ) ), { 1, 0 }, true ); // Expand width and clear

    ui->pushBelow ( new ConsoleUi::Menu ( "Continue?", { "Yes" }, "No" ) );

    ret = ( ui->popUntilUserInput()->resultInt == 0 );

    ui->pop();
    ui->pop();

    return ret;
}

bool MainUi::spectate ( const SpectateConfig& spectateConfig )
{
    bool ret = false;

    ASSERT ( ui.get() != 0 );

    string text;
    if ( spectateConfig.mode.isBroadcast() )
        text = "Spectating a broadcast (0 delay)";
    else
        text = toString ( "Spectating %s vs %s (%u delay%s)",
                          spectateConfig.names[0], spectateConfig.names[1], spectateConfig.delay,
                          spectateConfig.rollback ? toString ( ", %u rollback)", spectateConfig.rollback ) : "" );

    ui->pushInFront ( new ConsoleUi::TextBox ( text ), { 1, 0 }, true ); // Expand width and clear

    ui->pushBelow ( new ConsoleUi::Menu ( "Continue?", { "Yes" }, "No" ) );

    ret = ( ui->popUntilUserInput()->resultInt == 0 );

    ui->pop();
    ui->pop();

    return ret;
}

const void *MainUi::getConsoleWindow() const
{
    if ( !ui )
        return 0;

    return ui->getConsoleWindow();
}
