#include "Main.h"
#include "MainUi.h"
#include "Version.h"
#include "ConsoleUi.h"
#include "Exceptions.h"
#include "ErrorStringsExt.h"
#include "CharacterSelect.h"

#include <mmsystem.h>

using namespace std;


// Indent position of the pinging stats (must be a string)
#define INDENT_STATS "12"

// System sound prefix
#define SYSTEM_ALERT_PREFEX "System"

// Main configuration file
#define CONFIG_FILE FOLDER "config.ini"

// Run macro that deinitializes controllers, runs, then reinitializes controllers
#define RUN(ADDRESS, CONFIG)                                                                    \
    do {                                                                                        \
        ControllerManager::get().deinitialize();                                                \
        run ( ADDRESS, CONFIG );                                                                \
        ControllerManager::get().initialize ( 0 );                                              \
    } while ( 0 )


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
        catch ( const Exception& exc )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( exc.user ), { 1, 0 } ); // Expand width
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

        RUN ( address, initialConfig );

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
        catch ( const Exception& exc )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( exc.user ), { 1, 0 } ); // Expand width
            continue;
        }

        if ( address.addr.empty() )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( ERROR_INVALID_ADDR_PORT ), { 1, 0 } ); // Expand width
            continue;
        }

        initialConfig.mode.value = ClientMode::SpectateNetplay;

        RUN ( address, initialConfig );

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
            ui->pushBelow ( new ConsoleUi::TextBox ( ERROR_INVALID_PORT ), { 1, 0 } ); // Expand width
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

        address.port = menu->resultInt;
        address.invalidate();

        config.putInteger ( "lastUsedPort", address.port );
        saveConfig();

        RUN ( "", netplayConfig );

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

        if ( menu->resultInt < 0 )
            break;

        ui->clearBelow ( false );

        if ( menu->resultInt >= 0xFF )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( ERROR_INVALID_DELAY ) );
            continue;
        }

        delay = menu->resultInt;

        if ( gameModeIncludingVersusCpu() )
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
    netplayConfig.hostPlayer = 1; // TODO make this configurable

    RUN ( "", netplayConfig );

    ui->popNonUserInput();
}

bool MainUi::areYouSure()
{
    ui->pushRight ( new ConsoleUi::Menu ( "Are you sure?", { "Yes" }, "Cancel" ) );

    int ret = ui->popUntilUserInput()->resultInt;

    ui->pop();

    return ( ret == 0 );
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

bool MainUi::gameModeIncludingVersusCpu()
{
    ui->pushInFront ( new ConsoleUi::Menu ( "Mode", { "Versus", "Versus CPU", "Training" }, "Cancel" ) );
    ui->popUntilUserInput ( true ); // Clear popped since we should clear any messages

    int mode = ui->top()->resultInt;

    ui->pop();

    if ( mode < 0 || mode > 2 )
        return false;

    if ( mode == 0 )
        initialConfig.mode.flags = 0;
    else if ( mode == 1 )
        initialConfig.mode.flags = ClientMode::VersusCpu;
    else // if ( mode == 2 )
        initialConfig.mode.flags = ClientMode::Training;

    return true;
}

void MainUi::doneMapping ( Controller *controller, uint32_t key )
{
    LOG ( "%s: controller=%08x; key=%08x", controller->getName(), controller, key );

    mappedKey = key;

    EventManager::get().stop();
}

void MainUi::controls()
{
    for ( ;; )
    {
        ControllerManager::get().refreshJoysticks();

        vector<Controller *> controllers = ControllerManager::get().getControllers();

        vector<string> names;
        names.reserve ( controllers.size() );
        for ( Controller *c : controllers )
            names.push_back ( c->getName() );

        ui->pushRight ( new ConsoleUi::Menu ( "Controllers", names, "Back" ) );
        int ret = ui->popUntilUserInput ( true )->resultInt; // Clear popped since we don't care about messages
        ui->pop();

        if ( ret < 0 || ret >= ( int ) controllers.size() )
            break;

        Controller& controller = *controllers[ret];

        int pos = ( controller.isKeyboard() ? 0 : 4 );

        for ( ;; )
        {
            loadMappings ( controller );

            vector<string> options ( gameInputBits.size() );

            for ( size_t i = 0; i < gameInputBits.size(); ++i )
            {
                const string mapping = controller.getMapping ( gameInputBits[i].second );
                options[i] = format ( "%-11s: %s", gameInputBits[i].first, mapping );
            }

            options.push_back ( "Clear all" );
            options.push_back ( "Reset to defaults" );

            if ( controller.isJoystick() )
                options.push_back ( format ( "Set joystick deadzone (%.2f)", controller.getDeadzone() ) );

            // Add instructions above menu
            ui->pushRight ( new ConsoleUi::TextBox (
                                controller.getName() + " mappings\n"
                                "Press Left/Right/Delete to delete a key\n"
                                "Press Escape to cancel mapping\n" ) );

            // Add menu without title
            ui->pushBelow ( new ConsoleUi::Menu ( options, "Back" ) );
            ui->top<ConsoleUi::Menu>()->setPosition ( pos );
            ui->top<ConsoleUi::Menu>()->setDelete ( 2 );

            for ( ;; )
            {
                pos = ui->popUntilUserInput()->resultInt;

                // Cancel or exit
                if ( pos < 0 || pos > ( int ) options.size() )
                    break;

                // Modify a key
                if ( pos >= 0 && pos < ( int ) gameInputBits.size() )
                    break;

                // Last 3 options (ignore delete)
                if ( !ui->top()->resultStr.empty() )
                    break;
            }

            // Cancel or exit
            if ( pos < 0
                    || pos > ( int ) options.size()
                    || ( pos == ( int ) options.size() && !ui->top()->resultStr.empty() ) )
            {
                ui->pop();
                ui->pop();
                break;
            }

            // Clear all
            if ( pos == ( int ) gameInputBits.size() )
            {
                ui->pop();
                ui->pop();

                if ( areYouSure() )
                {
                    controller.clearAllMappings();
                    saveMappings ( controller );
                }
                continue;
            }

            // Reset to defaults
            if ( pos == ( int ) gameInputBits.size() + 1 )
            {
                ui->pop();
                ui->pop();

                if ( areYouSure() )
                {
                    if ( controller.isKeyboard() )
                        controller.setMappings ( ProcessManager::fetchKeyboardConfig() );
                    else
                        controller.resetToDefaults();
                    saveMappings ( controller );
                }
                continue;
            }

            // Set joystick deadzone
            if ( pos == ( int ) gameInputBits.size() + 2 )
            {
                ui->pop();
                ui->pop();

                if ( !controller.isJoystick() )
                    continue;

                ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptString,
                                                        "Enter a value between 0.0 and 1.0:" ) );

                ui->top<ConsoleUi::Prompt>()->setInitial ( format ( "%.2f", controller.getDeadzone() ) );

                for ( ;; )
                {
                    ui->popUntilUserInput();

                    if ( ui->top()->resultStr.empty() )
                        break;

                    stringstream ss ( ui->top()->resultStr );
                    float ret = -1.0f;

                    if ( ! ( ss >> ret ) || ret <= 0.0f || ret >= 1.0f )
                    {
                        ui->pushBelow ( new ConsoleUi::TextBox ( "Value must be a number between 0.0 and 1.0!" ) );
                        continue;
                    }

                    controller.setDeadzone ( ret );
                    break;
                }

                saveMappings ( controller );
                ui->pop();
                continue;
            }

            // Delete specific mapping
            if ( ui->top()->resultStr.empty() )
            {
                if ( pos < ( int ) gameInputBits.size() )
                    controller.clearMapping ( gameInputBits[pos].second );

                saveMappings ( controller );
                ui->pop();
                ui->pop();
                continue;
            }

            // Map selected key
            ui->top<ConsoleUi::Menu>()->overlayCurrentPosition ( format ( "%-11s: ...", gameInputBits[pos].first ) );

            AutoManager _;

            if ( controller.isKeyboard() )
            {
                controller.startMapping ( this, gameInputBits[pos].second, getConsoleWindow() );

                EventManager::get().start();
            }
            else
            {
                ControllerManager::get().check(); // Flush joystick events before mapping

                controller.startMapping ( this, gameInputBits[pos].second, getConsoleWindow() );

                EventManager::get().startPolling();

                while ( EventManager::get().poll ( 1 ) )
                    ControllerManager::get().check();
            }

            saveMappings ( controller );
            ui->pop();
            ui->pop();

            // Continue mapping
            if ( mappedKey && pos + 1 < ( int ) gameInputBits.size() )
            {
                ++pos;
                _ungetch ( RETURN_KEY );
            }
        }
    }
}

void MainUi::settings()
{
    static const vector<string> options =
    {
        "Alert on connect",
        "Display name",
        "Show full character name",
        "Game CPU priority",
        "Versus mode win count",
        "About",
    };

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
                { 0, 0 }, true ); // Don't expand but DO clear

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
                { 0, 0 }, true ); // Don't expand but DO clear

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
                { 0, 0 }, true ); // Don't expand but DO clear

                ui->top<ConsoleUi::Menu>()->setPosition ( ( config.getInteger ( "highCpuPriority" ) + 1 ) % 2 );
                ui->popUntilUserInput();

                if ( ui->top()->resultInt >= 0 && ui->top()->resultInt <= 1 )
                {
                    config.putInteger ( "highCpuPriority", ( ui->top()->resultInt + 1 ) % 2 );
                    saveConfig();
                }

                ui->pop();
                break;

            case 4:
                ui->pushInFront ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter versus mode win count:" ),
                { 0, 0 }, true ); // Don't expand but DO clear

                ui->top<ConsoleUi::Prompt>()->allowNegative = false;
                ui->top<ConsoleUi::Prompt>()->maxDigits = 1;
                ui->top<ConsoleUi::Prompt>()->setInitial ( config.getInteger ( "versusWinCount" ) );

                for ( ;; )
                {
                    ui->popUntilUserInput();

                    if ( ui->top()->resultInt < 0 )
                        break;

                    if ( ui->top()->resultInt == 0 )
                    {
                        ui->pushBelow ( new ConsoleUi::TextBox ( "Win count can't be zero!" ) );
                        continue;
                    }

                    if ( ui->top()->resultInt > 5 )
                    {
                        ui->pushBelow ( new ConsoleUi::TextBox ( "Win count can't be greater than 5!" ) );
                        continue;
                    }

                    config.putInteger ( "versusWinCount", ui->top()->resultInt );
                    saveConfig();
                    break;
                }

                ui->pop();
                break;

            case 5:
                ui->pushInFront ( new ConsoleUi::TextBox ( format ( "%s%s\n\nRevision %s\n\nBuilt on %s\n\n"
                                  "Created by Madscientist\n\nPress any key to go back",
                                  uiTitle,
#if defined(DEBUG)
                                  " (debug)",
#elif defined(LOGGING)
                                  " (logging)",
#else
                                  "",
#endif
                                  LocalVersion.revision, LocalVersion.buildTime ) ),
                { 0, 0 }, true ); // Don't expand but DO clear
                system ( "@pause > nul" );
                break;

            default:
                break;
        }
    }

    ui->pop();
}

void MainUi::initialize()
{
    ui.reset ( new ConsoleUi ( uiTitle, ProcessManager::isWine() ) );

    // Configurable settings
    config.putInteger ( "alertOnConnect", 3 );
    config.putString ( "alertWavFile", "SystemDefault" );
    config.putString ( "displayName", ProcessManager::fetchGameUserName() );
    config.putInteger ( "fullCharacterName", 0 );
    config.putInteger ( "highCpuPriority", 1 );
    config.putInteger ( "versusWinCount", 2 );

    // Cached UI state
    config.putInteger ( "lastUsedPort", -1 );
    config.putInteger ( "lastMainMenuPosition", 0 );

    // Load and save main config (this creates the config file on the first time)
    loadConfig();
    saveConfig();

    // Reset the initial config
    initialConfig.clear();
    initialConfig.localName = config.getString ( "displayName" );
    initialConfig.winCount = config.getInteger ( "versusWinCount" );

    // Initialize controllers
    ControllerManager::get().initialize ( 0 );
    ControllerManager::get().windowHandle = getConsoleWindow();
    ControllerManager::get().check();

    // Setup default mappings
    ControllerManager::get().getKeyboard()->setMappings ( ProcessManager::fetchKeyboardConfig() );
    for ( Controller *controller : ControllerManager::get().getJoysticks() )
        controller->resetToDefaults();

    // Load then save all controller mappings
    ControllerManager::get().loadMappings ( appDir + FOLDER, MAPPINGS_EXT );
    ControllerManager::get().saveMappings ( appDir + FOLDER, MAPPINGS_EXT );
}

void MainUi::saveConfig()
{
    const string file = appDir + CONFIG_FILE;

    LOG ( "Saving: %s", file );

    if ( config.save ( file ) )
        return;

    const string msg = format ( "Failed to save: %s", file );

    LOG ( "%s", msg );

    if ( sessionError.find ( msg ) == string::npos )
        sessionError += format ( "\n%s", msg );
}

void MainUi::loadConfig()
{
    const string file = appDir + CONFIG_FILE;

    LOG ( "Loading: %s", file );

    if ( config.load ( file ) )
        return;

    LOG ( "Failed to load: %s", file );
}

void MainUi::saveMappings ( const Controller& controller )
{
    const string file = appDir + FOLDER + controller.getName() + MAPPINGS_EXT;

    LOG ( "Saving: %s", file );

    if ( controller.saveMappings ( file ) )
        return;

    const string msg = format ( "Failed to save: %s", file );

    LOG ( "%s", msg );

    if ( sessionError.find ( msg ) == string::npos )
        sessionError += format ( "\n%s", msg );
}

void MainUi::loadMappings ( Controller& controller )
{
    const string file = appDir + FOLDER + controller.getName() + MAPPINGS_EXT;

    LOG ( "Loading: %s", file );

    if ( controller.loadMappings ( file ) )
        return;

    LOG ( "Failed to load: %s", file );
}

void MainUi::main ( RunFuncPtr run )
{
    static const vector<string> options = { "Netplay", "Spectate", "Broadcast", "Offline", "Controls", "Settings" };

    ASSERT ( ui.get() != 0 );

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
        initialConfig.winCount = config.getInteger ( "versusWinCount" );

        if ( address.empty() && config.getInteger ( "lastUsedPort" ) > 0 )
        {
            address.port = config.getInteger ( "lastUsedPort" );
            address.invalidate();
        }

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
    return format (
               "%-" INDENT_STATS "sPing: %.2f ms"
#ifndef NDEBUG
               "\n%-" INDENT_STATS "sWorst: %.2f ms"
               "\n%-" INDENT_STATS "sStdErr: %.2f ms"
               "\n%-" INDENT_STATS "sStdDev: %.2f ms"
               "\n%-" INDENT_STATS "sPacket Loss: %d%%"
#endif
               , format ( "Delay: %d", computeDelay ( pingStats.latency ) ), pingStats.latency.getMean()
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
                          initialConfig.remoteName + " connected"
                          "\n\n" + formatStats ( pingStats ) ), { 1, 0 }, true ); // Expand width and clear

    ui->pushBelow ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter delay:" ) );

    ui->top<ConsoleUi::Prompt>()->allowNegative = false;
    ui->top<ConsoleUi::Prompt>()->maxDigits = 3;
    ui->top<ConsoleUi::Prompt>()->setInitial ( computeDelay ( pingStats.latency ) );

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput ( true ); // Clear popped since we don't care about messages

        if ( menu->resultInt < 0 )
            break;

        ui->clearBelow ( false );

        if ( menu->resultInt >= 0xFF )
        {
            ui->pushBelow ( new ConsoleUi::TextBox ( ERROR_INVALID_DELAY ) );
            continue;
        }

        netplayConfig.delay = menu->resultInt;
        netplayConfig.rollback = 0; // TODO select rollback
        netplayConfig.winCount = config.getInteger ( "versusWinCount" );
#ifdef RELEASE
        netplayConfig.hostPlayer = 1 + ( rand() % 2 );
#else
        netplayConfig.hostPlayer = 1; // Host is always player 1 for easier debugging
#endif
        ret = true;
        break;
    }

    ui->pop();

    if ( ret )
    {
        ui->pushBelow ( new ConsoleUi::TextBox ( format ( "Using %u delay%s",
                        netplayConfig.delay,
                        ( netplayConfig.rollback ? format ( ", %u rollback", netplayConfig.rollback ) : "" ) ) ),
        { 1, 0 } ); // Expand width

        ui->pushBelow ( new ConsoleUi::TextBox ( "Waiting for client..." ), { 1, 0 } ); // Expand width
    }

    return ret;
}

void MainUi::connected ( const InitialConfig& initialConfig, const PingStats& pingStats )
{
    alertUser();

    ASSERT ( ui.get() != 0 );

    const string modeString = ( initialConfig.mode.isTraining()
                                ? "Training mode"
                                : format ( "Versus mode, each game is %u rounds", initialConfig.winCount ) );

    ui->pushInFront ( new ConsoleUi::TextBox (
                          "Connected to " + initialConfig.remoteName
                          + "\n\n" + modeString
                          + "\n\n" + formatStats ( pingStats ) ), { 1, 0 }, true ); // Expand width and clear

    ui->pushBelow ( new ConsoleUi::TextBox ( "Waiting for host to choose delay..." ), { 1, 0 } ); // Expand width
}

void MainUi::connected ( const NetplayConfig& netplayConfig )
{
    ASSERT ( ui.get() != 0 );

    ui->pushInFront ( new ConsoleUi::TextBox ( format ( "Host chose %u delay%s",
                      netplayConfig.delay,
                      ( netplayConfig.rollback ? format ( ", %u rollback", netplayConfig.rollback ) : "" ) ) ),
    { 1, 0 }, true ); // Expand width and clear
}

void MainUi::spectate ( const SpectateConfig& spectateConfig )
{
    alertUser();

    ASSERT ( ui.get() != 0 );

    string text;

    if ( spectateConfig.mode.isBroadcast() )
    {
        text = format ( "Spectating a %s mode broadcast (0 delay)\n\n",
                        ( spectateConfig.mode.isTraining() ? "training" : "versus" ) );
    }
    else
    {
        text = format ( "Spectating %s mode (%u delay%s)\n\n",
                        ( spectateConfig.mode.isTraining() ? "training" : "versus" ), spectateConfig.delay,
                        ( spectateConfig.rollback ? format ( ", %u rollback", spectateConfig.rollback ) : "" ) );
    }

    CharaNameFunc charaNameFunc = ( config.getInteger ( "fullCharacterName" ) ? getFullCharaName : getShortCharaName );

    if ( spectateConfig.initial.netplayState <= NetplayState::CharaSelect )
    {
        if ( spectateConfig.mode.isBroadcast() )
            text += "Currently selecting characters...";
        else
            text += format ( "%s vs %s", spectateConfig.names[0], spectateConfig.names[1] );
    }
    else
    {
        text += spectateConfig.formatPlayer ( 1, charaNameFunc )
                + " vs " + spectateConfig.formatPlayer ( 2, charaNameFunc );
    }

    text += "\n\n(Press and hold space-bar to prevent fast-forward)";

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

void *MainUi::getConsoleWindow()
{
    return ConsoleUi::getConsoleWindow();
}
