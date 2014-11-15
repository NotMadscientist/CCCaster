#include "Main.h"
#include "MainUi.h"
#include "Logger.h"
#include "Utilities.h"
#include "Version.h"
#include "ConsoleUi.h"

#include <mmsystem.h>

using namespace std;


// Indent position of the pinging stats (must be a string)
#define INDENT_STATS "12"

// System sound prefix
#define SYSTEM_ALERT_PREFEX "System"

// Config file
#define CONFIG_FILE FOLDER "config.ini"

extern string appDir;

static const string uiTitle = "CCCaster " + LocalVersion.code;

static ConsoleUi::Menu *mainMenu = 0;


void MainUi::netplay ( RunFuncPtr run )
{
    ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptString,
                                            "Enter/paste <ip>:<port> to join or <port> to host:" ),
    { 1, 0 } ); // Expand width

    ui->top<ConsoleUi::Prompt>()
    ->setInitial ( ( address.addr.empty() && !address.empty() ) ? address.str().substr ( 1 ) : address.str() );

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
                                            "Enter/paste <ip>:<port> to spectate:" ), { 1, 0 } ); // Expand width

    ui->top<ConsoleUi::Prompt>()->setInitial ( !address.addr.empty() ? address.str() : "" );

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
                                            "Enter/paste <port> to broadcast:" ), { 1, 0 } ); // Expand width

    ui->top<ConsoleUi::Prompt>()->allowNegative = false;
    ui->top<ConsoleUi::Prompt>()->maxDigits = 5;
    ui->top<ConsoleUi::Prompt>()->setInitial ( address.port ? address.port : INT_MIN );

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput();

        if ( menu->resultInt == INT_MIN )
            break;

        ui->clearBelow();

        ASSERT ( menu->resultInt >= 0 );

        if ( menu->resultInt > 0xFFFF )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( "Port can't be greater than 65535!" ), { 1, 0 } ); // Expand width
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
    ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter delay:" ) );

    ui->top<ConsoleUi::Prompt>()->allowNegative = false;
    ui->top<ConsoleUi::Prompt>()->maxDigits = 3;
    ui->top<ConsoleUi::Prompt>()->setInitial ( 0 );

    uint8_t delay = 0;
    bool good = false;

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput();

        if ( menu->resultInt == INT_MIN )
            break;

        ui->clearBelow ( false );

        ASSERT ( menu->resultInt >= 0 );

        if ( menu->resultInt >= 0xFF )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( "Delay can't be greater than 254!" ) );
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

    run ( "", netplayConfig );

    ui->popNonUserInput();
}

bool MainUi::gameMode()
{
    ui->pushBelow ( new ConsoleUi::Menu ( "Mode", { "Versus", "Training" }, "Cancel" ) );
    ui->popUntilUserInput ( true ); // Clear popped since we should clear any messages

    int mode = ui->top()->resultInt;

    ui->pop();

    if ( mode < 0 || mode > 1 )
        return false;

    initialConfig.mode.flags = ( mode == 1 ? ClientMode::Training : 0 );
    return true;
}

void MainUi::doneMapping ( Controller *controller, uint32_t key )
{
    LOG ( "%s: controller=%08x; key=%08x", controller->getName(), controller, key );

    isMapping = false;
    mappedKey = key;

    EventManager::get().stop();
}

void MainUi::controls()
{
    ControllerManager::get().initialize ( 0 );
    ControllerManager::get().check();

    vector<Controller *> controllers = ControllerManager::get().getControllers();

    vector<string> names;
    names.reserve ( controllers.size() );
    for ( Controller *c : controllers )
        names.push_back ( c->getName() );

    ui->pushRight ( new ConsoleUi::Menu ( "Controllers", names, "Back" ) );

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput ( true ); // Clear popped since we don't care about messages

        if ( menu->resultInt < 0 || menu->resultInt >= ( int ) controllers.size() )
            break;

        Controller& controller = *controllers[menu->resultInt];

        static const vector<pair<string, uint32_t>> bits =
        {
            { "Up         : ",    BIT_UP },
            { "Down       : ",  BIT_DOWN },
            { "Left       : ",  BIT_LEFT },
            { "Right      : ", BIT_RIGHT },
            { "A (confirm): ", ( CC_BUTTON_A | CC_BUTTON_SELECT ) << 8 },
            { "B (cancel) : ", ( CC_BUTTON_B | CC_BUTTON_CANCEL ) << 8 },
            { "C          : ", CC_BUTTON_C << 8 },
            { "D          : ", CC_BUTTON_D << 8 },
            { "E          : ", CC_BUTTON_E << 8 },
            { "A+B        : ", CC_BUTTON_AB << 8 },
            { "Start      : ", CC_BUTTON_START << 8 },
            { "FN1        : ", CC_BUTTON_FN1 << 8 },
            { "FN2        : ", CC_BUTTON_FN2 << 8 },
        };

        ui->clearTop();

        int position = ( controller.isKeyboard() ? 0 : 4 );

        for ( ;; )
        {
            vector<string> mappings ( bits.size() );
            for ( size_t i = 0; i < bits.size(); ++i )
            {
                string mapping = controller.getMapping ( bits[i].second );
                mappings[i] = bits[i].first + ( mapping.empty() ? "   " : mapping );
            }

            // TODO show more info at the top
            ui->pushInFront ( new ConsoleUi::Menu ( controller.getName(), mappings, "Back" ) );
            ui->top<ConsoleUi::Menu>()->setPosition ( position );
            ui->top<ConsoleUi::Menu>()->setDelete ( 2 );

            position = ui->popUntilUserInput()->resultInt;

            if ( position < 0
                    || position > ( int ) mappings.size()
                    || ( position == ( int ) mappings.size() && !ui->top()->resultStr.empty() ) )
            {
                ui->pop();
                break;
            }

            if ( ui->top()->resultStr.empty() )
            {
                if ( position < ( int ) mappings.size() )
                    controller.clearMapping ( bits[position].second );

                ui->pop();
                continue;
            }

            ui->top<ConsoleUi::Menu>()->overlayCurrentPosition ( bits[position].first + "..." );

            if ( controller.isKeyboard() )
            {
                AutoManager _;

                isMapping = true;
                controller.startMapping ( this, bits[position].second, getConsoleWindow() );

                EventManager::get().start();
            }
            else
            {
                AutoManager _;

                isMapping = true;
                controller.startMapping ( this, bits[position].second );

                while ( isMapping )
                {
                    ControllerManager::get().check();
                    Sleep ( 1 );
                }
            }

            ui->pop();

            // Continue mapping
            if ( mappedKey )
            {
                ++position;

                if ( position < ( int ) mappings.size() )
                    _ungetch ( RETURN_KEY );
            }
        }
    }

    ui->pop();
}

void MainUi::settings()
{
    static const vector<string> options =
    { "Alert on connect", "Display name", "Show full character name", "Game CPU priority" };

    ui->pushRight ( new ConsoleUi::Menu ( "Settings", options, "Back" ) );

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput ( true ); // Clear popped since we don't care about messages

        if ( menu->resultInt < 0 || menu->resultInt >= ( int ) options.size() )
            break;

        switch ( menu->resultInt )
        {
            case 0:
            {
                ui->pushInFront ( new ConsoleUi::Menu ( "Alert when connected?",
                { "Focus window", "Play a sound", "Do both", "Don't alert" }, "Cancel" ),
                { 0, 0 }, true ); // Don't expand but clear

                ui->top<ConsoleUi::Menu>()->setPosition ( ( config.getInteger ( "alertOnConnect" ) + 3 ) % 4 );
                ui->popUntilUserInput();

                int alertType = ui->top()->resultInt;

                ui->pop();

                if ( alertType < 0 || alertType > 3 )
                    break;

                config.putInteger ( "alertOnConnect", ( alertType + 1 ) % 4 );
                saveConfig();

                if ( alertType == 0 || alertType == 3 )
                    break;

                ui->pushInFront ( new ConsoleUi::TextBox (
                                      "Enter/paste/drag a .wav file here:\n"
                                      "(Leave blank to use SystemDefault)" ),
                { 1, 0 }, true ); // Expand width and clear

                ui->pushBelow ( new ConsoleUi::Prompt ( ConsoleUi::PromptString ), { 1, 0 } ); // Expand width

                ui->top<ConsoleUi::Prompt>()->setInitial ( config.getString ( "alertWavFile" ) );
                ui->popUntilUserInput();

                if ( ui->top()->resultInt == 0 )
                {
                    if ( ui->top()->resultStr.empty() )
                        config.putString ( "alertWavFile", "SystemDefault" );
                    else
                        config.putString ( "alertWavFile", ui->top()->resultStr );
                    saveConfig();
                }

                ui->pop();
                ui->pop();
                break;
            }

            case 1:
                ui->pushInFront ( new ConsoleUi::Prompt ( ConsoleUi::PromptString,
                                  "Enter name to show when connecting:" ), { 1, 0 }, true ); // Expand width and clear

                ui->top<ConsoleUi::Prompt>()->setInitial ( config.getString ( "displayName" ) );
                ui->popUntilUserInput();

                if ( ui->top()->resultInt == 0 )
                {
                    config.putString ( "displayName", ui->top()->resultStr );
                    saveConfig();
                }

                ui->pop();
                break;

            case 2:
                ui->pushInFront ( new ConsoleUi::Menu ( "Show full character names when spectating?",
                { "Yes", "No" }, "Cancel" ),
                { 0, 0 }, true ); // Don't expand but clear

                ui->top<ConsoleUi::Menu>()->setPosition ( ( config.getInteger ( "fullCharacterName" ) + 1 ) % 2 );
                ui->popUntilUserInput();

                if ( ui->top()->resultInt >= 0 && ui->top()->resultInt <= 1 )
                {
                    config.putInteger ( "fullCharacterName", ( ui->top()->resultInt + 1 ) % 2 );
                    saveConfig();
                }

                ui->pop();
                break;

            case 3:
                ui->pushInFront ( new ConsoleUi::Menu ( "Start game with high CPU priority?",
                { "Yes", "No" }, "Cancel" ),
                { 0, 0 }, true ); // Don't expand but clear

                ui->top<ConsoleUi::Menu>()->setPosition ( ( config.getInteger ( "highCpuPriority" ) + 1 ) % 2 );
                ui->popUntilUserInput();

                if ( ui->top()->resultInt >= 0 && ui->top()->resultInt <= 1 )
                {
                    config.putInteger ( "highCpuPriority", ( ui->top()->resultInt + 1 ) % 2 );
                    saveConfig();
                }

                ui->pop();
                break;

            default:
                break;
        }
    }

    ui->pop();
}

void MainUi::initialize()
{
    // Configurable settings
    config.putInteger ( "alertOnConnect", 3 );
    config.putString ( "alertWavFile", "SystemDefault" );
    config.putString ( "displayName", ProcessManager::fetchGameUserName() );
    config.putInteger ( "fullCharacterName", 0 );
    config.putInteger ( "highCpuPriority", 1 );

    // Cached UI state
    config.putInteger ( "lastUsedPort", -1 );
    config.putInteger ( "lastMainMenuPosition", 0 );

    // Override with user configuration
    config.load ( appDir + CONFIG_FILE );

    // Save config after loading (this creates the config file on the first time)
    saveConfig();

    // Reset the initial config
    initialConfig.clear();
    initialConfig.localName = config.getString ( "displayName" );
}

void MainUi::saveConfig()
{
    LOG ( "Saving: %s", appDir + CONFIG_FILE );

    if ( config.save ( appDir + CONFIG_FILE ) )
        return;

    LOG ( "Failed to save: %s", appDir + CONFIG_FILE );

    if ( sessionError.find ( "Failed to save config file:" ) == std::string::npos )
        sessionError += toString ( "\nFailed to save config file: %s", appDir + CONFIG_FILE );
}

void MainUi::main ( RunFuncPtr run )
{
    ControllerManager::get().initialize ( 0 );
    ControllerManager::get().check();

    static const vector<string> options = { "Netplay", "Spectate", "Broadcast", "Offline", "Controls", "Settings" };

    ui.reset ( new ConsoleUi ( uiTitle ) );
    ui->pushRight ( new ConsoleUi::Menu ( uiTitle, options, "Quit" ) );

    mainMenu = ui->top<ConsoleUi::Menu>();
    mainMenu->setEscape ( false );
    mainMenu->setPosition ( config.getInteger ( "lastMainMenuPosition" ) - 1 );

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
        if ( address.empty() )
            address.port = config.getInteger ( "lastUsedPort" );

        int mainSelection = ui->popUntilUserInput()->resultInt;

        if ( mainSelection < 0 || mainSelection >= ( int ) options.size() )
            break;

        ui->clearRight();

        if ( mainSelection >= 0 && mainSelection <= 3 )
        {
            config.putInteger ( "lastMainMenuPosition", mainSelection + 1 );
            saveConfig();
        }

        switch ( mainSelection )
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

void MainUi::display ( const string& message, bool replace )
{
    if ( !ui )
        ui.reset ( new ConsoleUi ( uiTitle ) );

    if ( replace && ( ui->empty() || !ui->top()->requiresUser || ui->top() != mainMenu ) )
        ui->pushInFront ( new ConsoleUi::TextBox ( message ), { 1, 0 }, true ); // Expand width and clear
    else if ( ui->top()->expandWidth() )
        ui->pushBelow ( new ConsoleUi::TextBox ( message ), { 1, 0 } ); // Expand width
    else
        ui->pushRight ( new ConsoleUi::TextBox ( message ), { 1, 0 } ); // Expand width
}

void MainUi::alertUser()
{
    int alert = config.getInteger ( "alertOnConnect" );

    if ( alert & 1 )
        SetForegroundWindow ( ( HWND ) getConsoleWindow() );

    if ( alert & 2 )
    {
        string buffer = config.getString ( "alertWavFile" );

        if ( buffer.find ( SYSTEM_ALERT_PREFEX ) == 0 )
            PlaySound ( TEXT ( buffer.c_str() ), 0, SND_ALIAS | SND_ASYNC );
        else
            PlaySound ( TEXT ( buffer.c_str() ), 0, SND_FILENAME | SND_ASYNC | SND_NODEFAULT );
    }
}

bool MainUi::accepted ( const InitialConfig& initialConfig, const PingStats& pingStats )
{
    alertUser();

    bool ret = false;

    ASSERT ( ui.get() != 0 );

    ui->pushInFront ( new ConsoleUi::TextBox (
                          initialConfig.getAcceptMessage ( "connected" ) + "\n\n"
                          + formatStats ( pingStats ) ), { 1, 0 }, true ); // Expand width and clear

    ui->pushBelow ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter delay:" ) );

    ui->top<ConsoleUi::Prompt>()->allowNegative = false;
    ui->top<ConsoleUi::Prompt>()->maxDigits = 3;
    ui->top<ConsoleUi::Prompt>()->setInitial ( computeDelay ( pingStats.latency ) );

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput ( true ); // Clear popped since we don't care about messages

        if ( menu->resultInt < 0 )
            break;

        if ( menu->resultInt >= 0xFF )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( "Delay can't be greater than 254!" ) );
            continue;
        }

        // TODO select rollback

        netplayConfig.delay = menu->resultInt;
        netplayConfig.hostPlayer = 1; // TODO randomize
        ret = true;
        break;
    }

    ui->pop();

    if ( ret )
    {
        ui->pushBelow ( new ConsoleUi::TextBox ( toString ( "Using %u delay%s",
                        netplayConfig.delay,
                        ( netplayConfig.rollback ? toString ( ", %u rollback", netplayConfig.rollback ) : "" ) ) ),
        { 1, 0 } ); // Expand width

        ui->pushBelow ( new ConsoleUi::TextBox ( "Waiting for client..." ), { 1, 0 } ); // Expand width
    }

    return ret;
}

void MainUi::connected ( const InitialConfig& initialConfig, const PingStats& pingStats )
{
    alertUser();

    ASSERT ( ui.get() != 0 );

    ui->pushInFront ( new ConsoleUi::TextBox (
                          initialConfig.getConnectMessage ( "Connected" ) + "\n\n"
                          + formatStats ( pingStats ) ), { 1, 0 }, true ); // Expand width and clear

    ui->pushBelow ( new ConsoleUi::TextBox ( "Waiting for host to choose delay..." ), { 1, 0 } ); // Expand width
}

void MainUi::connected ( const NetplayConfig& netplayConfig )
{
    ASSERT ( ui.get() != 0 );

    ui->pushInFront ( new ConsoleUi::TextBox ( toString ( "Host chose %u delay%s",
                      netplayConfig.delay,
                      ( netplayConfig.rollback ? toString ( ", %u rollback", netplayConfig.rollback ) : "" ) ) ),
    { 1, 0 }, true ); // Expand width and clear
}

#include "CharacterNames.h"

typedef const char * ( *CharaNameFunc ) ( uint32_t chara );

void MainUi::spectate ( const SpectateConfig& spectateConfig )
{
    alertUser();

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
}

bool MainUi::confirm()
{
    bool ret = false;

    ASSERT ( ui.get() != 0 );

    ui->pushBelow ( new ConsoleUi::Menu ( "Continue?", { "Yes" }, "No" ) );

    ret = ( ui->popUntilUserInput()->resultInt == 0 );

    ui->pop();

    return ret;
}

const void *MainUi::getConsoleWindow()
{
    return ConsoleUi::getConsoleWindow();
}
