#include "MainUi.h"
#include "Logger.h"
#include "Utilities.h"
#include "Version.h"
#include "ConsoleUi.h"
#include "ProcessManager.h"
#include "ControllerManager.h"

using namespace std;


static const string uiTitle = "CCCaster " + LocalVersion.code;

// Indent position of the pinging stats (must be a string)
#define INDENT_STATS "12"

// System sound prefix and default alert
#define SYSTEM_ALERT_PREFEX "System"
#define SYSTEM_DEFAULT_ALERT "SystemDefault"

// Config file
#define CONFIG_FILE FOLDER "config.ini"


static ConsoleUi::Menu *mainMenu = 0;


void MainUi::netplay ( RunFuncPtr run )
{
    ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptString,
                                            "Enter/paste <ip>:<port> to join or <port> to host:",
                                            ( ( address.addr.empty() && !address.empty() )
                                                    ? address.str().substr ( 1 )
                                                    : address.str() ) ),
    { 1, 0 } ); // Expand width

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput();

        if ( menu->resultStr.empty() )
            break;

        ui->clearBelow();

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
            config.putInteger ( "lastUsedPort", address.port );
            saveConfig();

            if ( !gameMode() )
                continue;

            initialConfig.mode.value = ClientMode::Host;
        }
        else
        {
            initialConfig.mode.value = ClientMode::Client;
        }

        run ( address, initialConfig );

        ui->popNonUserInput();

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
                                            "Enter/paste <ip>:<port> to spectate:",
                                            ( !address.addr.empty() ? address.str() : "" ) ),
    { 1, 0 } ); // Expand width

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput();

        if ( menu->resultStr.empty() )
            break;

        ui->clearBelow();

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
            ui->pushBelow ( new ConsoleUi::TextBox ( "Invalid IP address!" ), { 1, 0 } ); // Expand width
            continue;
        }

        initialConfig.mode.value = ClientMode::Spectate;

        run ( address, initialConfig );

        ui->popNonUserInput();

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
                                            ( address.port ? address.port : INT_MIN ), false, 5 ),
    { 1, 0 } ); // Expand width

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput();

        if ( menu->resultInt == INT_MIN )
            break;

        ui->clearBelow();

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
        netplayConfig.broadcastPort = address.port = menu->resultInt;

        config.putInteger ( "lastUsedPort", address.port );
        saveConfig();

        run ( "", netplayConfig );

        ui->popNonUserInput();

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

    ui->popNonUserInput();
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
    // Defaults settings
    config.putInteger ( "alertOnConnect", 3 );
    config.putString ( "alertWavFile", SYSTEM_DEFAULT_ALERT );
    config.putString ( "displayName", ProcessManager::fetchGameUserName() );
    config.putInteger ( "highCpuPriority", 1 );
    config.putInteger ( "lastUsedPort", -1 );
    config.putInteger ( "lastMainMenuPosition", 0 );
    // config.putInteger ( "spectatorCap", -1 );
    config.putInteger ( "showFullCharacterName", 0 );

    // Override with user configuration
    config.load ( CONFIG_FILE );

    // Save config after loading (this creates the config file on the first time)
    saveConfig();

    // Reset the initial config
    initialConfig.clear();
    initialConfig.localName = config.getString ( "displayName" );
}

void MainUi::saveConfig()
{
    if ( config.save ( CONFIG_FILE ) )
        return;

    LOG ( "Failed to save: %s", CONFIG_FILE );

    if ( sessionError.find ( "Failed to save config file:" ) == std::string::npos )
        sessionError += toString ( "\nFailed to save config file: %s", CONFIG_FILE );
}

void MainUi::main ( RunFuncPtr run )
{
    ControllerManager::get().initialize ( 0 );
    ControllerManager::get().check();

    ui.reset ( new ConsoleUi ( uiTitle ) );
    ui->pushRight ( new ConsoleUi::Menu ( uiTitle,
    { "Netplay", "Spectate", "Broadcast", "Offline", "Controls", "Settings" }, "Quit" ) );

    mainMenu = ( ConsoleUi::Menu * ) ui->top();

    for ( ;; )
    {
        ui->clear();

        if ( !sessionError.empty() && !sessionMessage.empty() )
            ui->pushRight ( new ConsoleUi::TextBox ( sessionError + "\n" + sessionMessage ), { 1, 0 } ); // Expand width
        else if ( !sessionError.empty() )
            ui->pushRight ( new ConsoleUi::TextBox ( sessionError ), { 1, 0 } ); // Expand width
        else if ( !sessionMessage.empty() )
            ui->pushRight ( new ConsoleUi::TextBox ( sessionMessage ), { 1, 0 } ); // Expand width

        sessionError.clear();
        sessionMessage.clear();

        // Update cached UI state
        initialConfig.localName = config.getString ( "displayName" );
        mainMenu->position = ( config.getInteger ( "lastMainMenuPosition" ) - 1 );
        if ( address.empty() )
            address.port = config.getInteger ( "lastUsedPort" );

        int main = ui->popUntilUserInput()->resultInt;

        if ( main < 0 || main > 5 )
            break;

        ui->clearRight();

        if ( main >= 0 && main <= 3 )
        {
            config.putInteger ( "lastMainMenuPosition", main + 1 );
            saveConfig();
        }

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
               "%-" INDENT_STATS "sPing: %.2f ms"
#ifndef NDEBUG
               "\n%-" INDENT_STATS "sWorst: %.2f ms"
               "\n%-" INDENT_STATS "sStdErr: %.2f ms"
               "\n%-" INDENT_STATS "sStdDev: %.2f ms"
               "\n%-" INDENT_STATS "sPacket Loss: %d%%"
#endif
               , toString ( "Delay: %d", computeDelay ( pingStats.latency ) ), pingStats.latency.getMean()
#ifndef NDEBUG
               , "", pingStats.latency.getWorst()
               , "", pingStats.latency.getStdErr()
               , "", pingStats.latency.getStdDev()
               , "", pingStats.packetLoss
#endif
           );
}

void MainUi::display ( const string& message )
{
    if ( !ui )
        ui.reset ( new ConsoleUi ( uiTitle ) );

    if ( ui->empty() || !ui->top()->requiresUser || ui->top() != mainMenu )
        ui->pushInFront ( new ConsoleUi::TextBox ( message ), { 1, 0 }, true ); // Expand width and clear
    else if ( ui->top()->expandWidth() )
        ui->pushBelow ( new ConsoleUi::TextBox ( message ), { 1, 0 } ); // Expand width
    else
        ui->pushRight ( new ConsoleUi::TextBox ( message ), { 1, 0 } ); // Expand width
}

bool MainUi::accepted ( const InitialConfig& initialConfig, const PingStats& pingStats )
{
    bool ret = false;

    ASSERT ( ui.get() != 0 );

    ui->pushInFront ( new ConsoleUi::TextBox (
                          initialConfig.getAcceptMessage ( "connected" ) + "\n\n"
                          + formatStats ( pingStats ) ), { 1, 0 }, true ); // Expand width and clear

    ui->pushBelow ( new ConsoleUi::Prompt (
                        ConsoleUi::PromptInteger, "Enter delay:",
                        computeDelay ( pingStats.latency ), false, 3 ) );

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
                          initialConfig.getConnectMessage ( "Connected" ) + "\n\n"
                          + formatStats ( pingStats ) ), { 1, 0 }, true ); // Expand width and clear

    ui->pushBelow ( new ConsoleUi::Menu ( "Continue?", { "Yes" }, "No" ) );

    ret = ( ui->popUntilUserInput()->resultInt == 0 );

    ui->pop();
    ui->pop();

    return ret;
}

#include "CharacterNames.h"

typedef const char * ( *CharaNameFunc ) ( uint32_t chara );

bool MainUi::spectate ( const SpectateConfig& spectateConfig )
{
    bool ret = false;

    ASSERT ( ui.get() != 0 );

    string text;

    if ( spectateConfig.mode.isBroadcast() )
        text = "Spectating a broadcast (0 delay)\n\n";
    else
        text = toString ( "Spectating %s mode (%u delay%s)\n\n",
                          ( spectateConfig.mode.isTraining() ? "training" : "versus" ), spectateConfig.delay,
                          ( spectateConfig.rollback ? toString ( ", %u rollback", spectateConfig.rollback ) : "" ) );

    if ( spectateConfig.chara[0] )
    {
        CharaNameFunc charaName = ( config.getInteger ( "useFullCharacterName" ) ? fullCharaName : shortCharaName );

        text += toString ( ( spectateConfig.names[0].empty() ? "%s%c-%s" : "%s (%c-%s)" ),
                           spectateConfig.names[0], spectateConfig.moon[0], charaName ( spectateConfig.chara[0] ) );

        text += " vs ";

        text += toString ( ( spectateConfig.names[1].empty() ? "%s%c-%s" : "%s (%c-%s)" ),
                           spectateConfig.names[1], spectateConfig.moon[1], charaName ( spectateConfig.chara[1] ) );
    }
    else
    {
        text += "Selecting characters...";
    }

    ui->pushInFront ( new ConsoleUi::TextBox ( text ), { 1, 0 }, true ); // Expand width and clear

    ui->pushBelow ( new ConsoleUi::Menu ( "Continue?", { "Yes" }, "No" ) );

    ret = ( ui->popUntilUserInput()->resultInt == 0 );

    ui->pop();
    ui->pop();

    return ret;
}

const void *MainUi::getConsoleWindow()
{
    return ConsoleUi::getConsoleWindow();
}
