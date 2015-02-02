#include "Main.h"
#include "MainUi.h"
#include "Version.h"
#include "ConsoleUi.h"
#include "Exceptions.h"
#include "ErrorStringsExt.h"
#include "CharacterSelect.h"
#include "NetplayManager.h"

#include <mmsystem.h>
#include <wininet.h>

using namespace std;


// Indent position of the pinging stats (must be a string)
#define INDENT_STATS "12"

// System sound prefix
#define SYSTEM_ALERT_PREFEX "System"

// Main configuration file
#define CONFIG_FILE FOLDER "config.ini"

// Path of the latest version file
#define LATEST_VERSION_PATH "LatestVersion"

// Main update archive file location
#define UPDATE_ARCHIVE FOLDER "update.zip"

// Timeout for update version check
#define VERSION_CHECK_TIMEOUT ( 1000 )

// Run macro that deinitializes controllers, runs, then reinitializes controllers
#define RUN(ADDRESS, CONFIG)                                                                    \
    do {                                                                                        \
        ControllerManager::get().deinitialize();                                                \
        run ( ADDRESS, CONFIG );                                                                \
        ControllerManager::get().initialize ( this );                                           \
    } while ( 0 )


extern string appDir;

static const string uiTitle = "CCCaster " + LocalVersion.majorMinor();

static ConsoleUi::Menu *mainMenu = 0;

static const vector<string> updateServers =
{
    "http://192.210.227.23/",
    "http://104.206.199.123/",
};


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
            config.setInteger ( "lastUsedPort", address.port );
            saveConfig();

            if ( !gameMode ( true ) ) // Show below
                continue;

            initialConfig.mode.value = ClientMode::Host;
        }
        else
        {
            initialConfig.mode.value = ClientMode::Client;
        }

        netplayConfig.clear();

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

        if ( !gameMode ( true ) ) // Show below
            continue;

        netplayConfig.clear();
        netplayConfig.mode.value = ClientMode::Broadcast;
        netplayConfig.mode.flags = initialConfig.mode.flags;
        netplayConfig.delay = 0;
        netplayConfig.hostPlayer = 1;
        netplayConfig.broadcastPort = menu->resultInt;

        address.port = menu->resultInt;
        address.invalidate();

        config.setInteger ( "lastUsedPort", address.port );
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

        if ( offlineGameMode() )
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

bool MainUi::gameMode ( bool below )
{
    ConsoleUi::Menu *menu = new ConsoleUi::Menu ( "Mode", { "Versus", "Training" }, "Cancel" );

    if ( below )
        ui->pushBelow ( menu );
    else
        ui->pushInFront ( menu );

    ui->popUntilUserInput ( true ); // Clear other messages since we're starting the game now

    int mode = ui->top()->resultInt;

    ui->pop();

    if ( mode < 0 || mode > 1 )
        return false;

    initialConfig.mode.flags = ( mode == 1 ? ClientMode::Training : 0 );
    return true;
}

bool MainUi::offlineGameMode()
{
    ui->pushInFront ( new ConsoleUi::Menu ( "Mode", { "Training", "Versus", "Versus CPU" }, "Cancel" ) );
    ui->top<ConsoleUi::Menu>()->setPosition ( config.getInteger ( "lastOfflineMenuPosition" ) - 1 );

    int mode = ui->popUntilUserInput ( true )->resultInt; // Clear other messages since we're starting the game now

    ui->pop();

    if ( mode < 0 || mode > 2 )
        return false;

    config.setInteger ( "lastOfflineMenuPosition", mode + 1 );
    saveConfig();

    if ( mode == 0 )
        initialConfig.mode.flags = ClientMode::Training;
    else if ( mode == 1 )
        initialConfig.mode.flags = 0;
    else // if ( mode == 2 )
        initialConfig.mode.flags = ClientMode::VersusCpu;

    return true;
}

void MainUi::detachedJoystick ( Controller *controller )
{
    LOG ( "controller=%08x; currentController=%08x", controller, currentController );

    if ( controller != currentController )
        return;

    currentController = 0;
    EventManager::get().stop();
}

void MainUi::doneMapping ( Controller *controller, uint32_t key )
{
    ASSERT ( controller == currentController );

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

            try
            {
                currentController = &controller;

                LOG ( "currentController=%08x", currentController );

                AutoManager _ ( currentController, getConsoleWindow() );

                if ( controller.isKeyboard() )
                {
                    controller.startMapping ( this, gameInputBits[pos].second );

                    EventManager::get().start();
                }
                else
                {
                    ControllerManager::get().check(); // Flush joystick events before mapping

                    if ( currentController )
                    {
                        controller.startMapping ( this, gameInputBits[pos].second );

                        EventManager::get().startPolling();

                        while ( EventManager::get().poll ( 1 ) )
                            ControllerManager::get().check();
                    }
                }
            }
            catch ( ... )
            {
            }

            ui->pop();
            ui->pop();

            if ( !currentController )
                break;

            saveMappings ( controller );
            currentController = 0;

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
        "Show full character names",
        "Game CPU priority",
        "Versus mode win count",
        "Check for updates on startup",
        "Max allowed real delay",
        "Default rollback",
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

                config.setInteger ( "alertOnConnect", ( alertType + 1 ) % 4 );
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
                    config.setInteger ( "fullCharacterName", ( ui->top()->resultInt + 1 ) % 2 );
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
                    config.setInteger ( "highCpuPriority", ( ui->top()->resultInt + 1 ) % 2 );
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

                    config.setInteger ( "versusWinCount", ui->top()->resultInt );
                    saveConfig();
                    break;
                }

                ui->pop();
                break;

            case 5:
                ui->pushInFront ( new ConsoleUi::Menu ( "Check for updates on startup?",
                { "Yes", "No" }, "Cancel" ),
                { 0, 0 }, true ); // Don't expand but DO clear

                ui->top<ConsoleUi::Menu>()->setPosition ( ( config.getInteger ( "autoCheckUpdates" ) + 1 ) % 2 );
                ui->popUntilUserInput();

                if ( ui->top()->resultInt >= 0 && ui->top()->resultInt <= 1 )
                {
                    config.setInteger ( "autoCheckUpdates", ( ui->top()->resultInt + 1 ) % 2 );
                    saveConfig();
                }

                ui->pop();
                break;

            case 6:
                ui->pushInFront ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter max allowed real delay:" ),
                { 0, 0 }, true ); // Don't expand but DO clear

                ui->top<ConsoleUi::Prompt>()->allowNegative = false;
                ui->top<ConsoleUi::Prompt>()->maxDigits = 3;
                ui->top<ConsoleUi::Prompt>()->setInitial ( config.getInteger ( "maxRealDelay" ) );

                for ( ;; )
                {
                    ui->popUntilUserInput();

                    if ( ui->top()->resultInt < 0 )
                        break;

                    if ( ui->top()->resultInt == 0 )
                    {
                        ui->pushBelow ( new ConsoleUi::TextBox ( "Max real delay can't be zero!" ) );
                        continue;
                    }

                    if ( ui->top()->resultInt >= 0xFF )
                    {
                        ui->pushBelow ( new ConsoleUi::TextBox ( ERROR_INVALID_DELAY ) );
                        continue;
                    }

                    config.setInteger ( "maxRealDelay", ui->top()->resultInt );
                    saveConfig();
                    break;
                }

                ui->pop();
                break;

            case 7:
                ui->pushInFront ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter default rollback:" ),
                { 0, 0 }, true ); // Don't expand but DO clear

                ui->top<ConsoleUi::Prompt>()->allowNegative = false;
                ui->top<ConsoleUi::Prompt>()->maxDigits = 2;
                ui->top<ConsoleUi::Prompt>()->setInitial ( config.getInteger ( "defaultRollback" ) );

                for ( ;; )
                {
                    ui->popUntilUserInput();

                    if ( ui->top()->resultInt < 0 )
                        break;

                    if ( ui->top()->resultInt > MAX_ROLLBACK )
                    {
                        ui->pushBelow ( new ConsoleUi::TextBox (
                                            format ( ERROR_INVALID_ROLLBACK, 1 + MAX_ROLLBACK ) ) );
                        continue;
                    }

                    config.setInteger ( "defaultRollback", ui->top()->resultInt );
                    saveConfig();
                    break;
                }

                ui->pop();
                break;

            case 8:
                ui->pushInFront ( new ConsoleUi::TextBox ( format ( "CCCaster %s%s\n\nRevision %s\n\nBuilt on %s\n\n"
                                  "Created by Madscientist\n\nPress any key to go back",
                                  LocalVersion.code,
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

void MainUi::setMaxRealDelay ( uint8_t delay )
{
    config.setInteger ( "maxRealDelay", delay );
    saveConfig();
}

void MainUi::setDefaultRollback ( uint8_t rollback )
{
    config.setInteger ( "defaultRollback", rollback );
    saveConfig();
}

void MainUi::initialize()
{
    ui.reset ( new ConsoleUi ( uiTitle, ProcessManager::isWine() ) );

    // Configurable settings (defaults)
    config.setInteger ( "alertOnConnect", 3 );
    config.putString ( "alertWavFile", "SystemDefault" );
    config.putString ( "displayName", ProcessManager::fetchGameUserName() );
    config.setInteger ( "fullCharacterName", 0 );
    config.setInteger ( "highCpuPriority", 1 );
    config.setInteger ( "versusWinCount", 2 );
    config.setInteger ( "maxRealDelay", 254 );
    config.setInteger ( "defaultRollback", 4 );
    config.setInteger ( "autoCheckUpdates", 1 );

    // Cached UI state (defaults)
    config.setInteger ( "lastUsedPort", -1 );
    config.setInteger ( "lastMainMenuPosition", -1 );
    config.setInteger ( "lastOfflineMenuPosition", -1 );

    // Load and save main config (this creates the config file on the first time)
    loadConfig();
    saveConfig();

    // Reset the initial config
    initialConfig.clear();
    initialConfig.localName = config.getString ( "displayName" );
    initialConfig.winCount = config.getInteger ( "versusWinCount" );

    // Initialize controllers
    ControllerManager::get().initialize ( this );
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
    static const vector<string> options =
    {
        "Netplay",
        "Spectate",
        "Broadcast",
        "Offline",
        "Controls",
        "Settings",
        "Update",
    };

    ASSERT ( ui.get() != 0 );

    ui->clearAll();

    if ( config.getInteger ( "autoCheckUpdates" ) )
        update ( true );

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

        // Reset flags
        initialConfig.mode.flags = 0;

        if ( address.empty() && config.getInteger ( "lastUsedPort" ) > 0 )
        {
            address.port = config.getInteger ( "lastUsedPort" );
            address.invalidate();
        }

        const int mainSelection = ui->popUntilUserInput()->resultInt;

        if ( mainSelection < 0 || mainSelection >= ( int ) options.size() )
            break;

        ui->clearRight();

        if ( mainSelection >= 0 && mainSelection <= 3 )
        {
            config.setInteger ( "lastMainMenuPosition", mainSelection + 1 );
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

            case 6:
                update();
                break;

            default:
                break;
        }
    }

    ControllerManager::get().deinitialize();
}

string MainUi::formatStats ( const PingStats& pingStats )
{
    return format (
               "%-" INDENT_STATS "sPing: %.2f ms"
#ifndef NDEBUG
               "\n%-" INDENT_STATS "sWorst: %.2f ms"
               "\n%-" INDENT_STATS "sStdErr: %.2f ms"
               "\n%-" INDENT_STATS "sStdDev: %.2f ms"
               "\n%-" INDENT_STATS "sPacket Loss: %d%%"
#endif
               , format ( "Delay: %d", computeDelay ( pingStats.latency.getMean() ) )
               , pingStats.latency.getMean()
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

    const int delay = computeDelay ( pingStats.latency.getMean() );
    const int worst = computeDelay ( pingStats.latency.getWorst() );
    const int variance = computeDelay ( pingStats.latency.getVariance() );

    int rollback = clamped ( delay + worst + variance, 0, config.getInteger ( "defaultRollback" ) );

    netplayConfig.delay = worst + 1;

    ui->pushInFront ( new ConsoleUi::TextBox (
                          initialConfig.remoteName + " connected"
                          "\n\n" + formatStats ( pingStats ) ), { 1, 0 }, true ); // Expand width and clear

    // TODO maybe implement this as a slider or something

    ui->pushBelow ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter rollback:" ) );

    ui->top<ConsoleUi::Prompt>()->allowNegative = false;
    ui->top<ConsoleUi::Prompt>()->maxDigits = 2;
    ui->top<ConsoleUi::Prompt>()->setInitial ( rollback );

    for ( ;; )
    {
        ConsoleUi::Element *menu = ui->popUntilUserInput();

        if ( menu->resultInt < 0 )
        {
            ui->pop();
            break;
        }

        if ( menu->resultInt > MAX_ROLLBACK )
        {
            ui->pushRight ( new ConsoleUi::TextBox ( format ( ERROR_INVALID_ROLLBACK, 1 + MAX_ROLLBACK ) ) );
            continue;
        }

        ui->clearRight();

        rollback = netplayConfig.rollback = menu->resultInt;

        ui->pushRight ( new ConsoleUi::Prompt ( ConsoleUi::PromptInteger, "Enter delay:" ) );

        ui->top<ConsoleUi::Prompt>()->allowNegative = false;
        ui->top<ConsoleUi::Prompt>()->maxDigits = 3;
        ui->top<ConsoleUi::Prompt>()->setInitial ( clamped ( delay - rollback, 0, delay ) );

        for ( ;; )
        {
            menu = ui->popUntilUserInput();

            if ( menu->resultInt < 0 )
                break;

            if ( menu->resultInt >= 0xFF )
            {
                ui->pushRight ( new ConsoleUi::TextBox ( ERROR_INVALID_DELAY ) );
                continue;
            }

            if ( rollback )
                netplayConfig.rollbackDelay = menu->resultInt;
            else
                netplayConfig.delay = menu->resultInt;

            netplayConfig.winCount = config.getInteger ( "versusWinCount" );
#ifdef RELEASE
            netplayConfig.hostPlayer = 1 + ( rand() % 2 );
#else
            netplayConfig.hostPlayer = 1; // Host is always player 1 for easier debugging
#endif

            ret = true;
            ui->pop();
            break;
        }

        ui->pop();
        if ( ret )
            break;
    }

    if ( ret )
    {
        ui->pushBelow ( new ConsoleUi::TextBox ( format ( "Using %u delay%s",
                        ( netplayConfig.rollback ? netplayConfig.rollbackDelay : netplayConfig.delay ),
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
                      ( netplayConfig.rollback ? netplayConfig.rollbackDelay : netplayConfig.delay ),
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

    text += "\n\n(Tap the spacebar to toggle fast-forward)";

    ui->pushInFront ( new ConsoleUi::TextBox ( text ), { 1, 0 }, true ); // Expand width and clear
}

bool MainUi::confirm ( const string& question )
{
    bool ret = false;

    ASSERT ( ui.get() != 0 );

    ui->pushBelow ( new ConsoleUi::Menu ( question, { "Yes" }, "No" ) );

    ret = ( ui->popUntilUserInput()->resultInt == 0 );

    ui->pop();

    return ret;
}

void *MainUi::getConsoleWindow()
{
    return ConsoleUi::getConsoleWindow();
}

void MainUi::httpResponse ( HttpGet *httpGet, int code, const std::string& data, uint32_t remainingBytes )
{
    ASSERT ( this->httpGet.get() == httpGet );

    Version version ( trimmed ( data ) );

    if ( code != 200 || version.major().empty() || version.minor().empty() )
    {
        httpFailed ( httpGet );
        return;
    }

    this->httpGet.reset();

    latestVersion = version;

    EventManager::get().stop();
}

void MainUi::httpFailed ( HttpGet *httpGet )
{
    ASSERT ( this->httpGet.get() == httpGet );

    this->httpGet.reset();

    ++serverIndex;

    if ( serverIndex >= updateServers.size() )
    {
        EventManager::get().stop();
        return;
    }

    updateTo ( "" );
}

void MainUi::downloadComplete ( HttpDownload *httpDl )
{
    ASSERT ( this->httpDl.get() == httpDl );

    this->httpDl.reset();

    downloadCompleted = true;

    EventManager::get().stop();
}

void MainUi::downloadFailed ( HttpDownload *httpDl )
{
    ASSERT ( this->httpDl.get() == httpDl );

    this->httpDl.reset();

    ++serverIndex;

    if ( serverIndex >= updateServers.size() )
    {
        EventManager::get().stop();
        return;
    }

    const ConsoleUi::ProgressBar *bar = ui->top<ConsoleUi::ProgressBar>();
    bar->update ( 0 );

    updateTo ( latestVersion.code );
}

void MainUi::downloadProgress ( HttpDownload *httpDl, uint32_t downloadedBytes, uint32_t totalBytes )
{
    const ConsoleUi::ProgressBar *bar = ui->top<ConsoleUi::ProgressBar>();
    bar->update ( ( bar->length * downloadedBytes ) / totalBytes );
    LOG ( "%u / %u", downloadedBytes, totalBytes );
}

void MainUi::updateTo ( const string& version )
{
    if ( version.empty() )
    {
        httpGet.reset ( new HttpGet ( this, updateServers[serverIndex] + LATEST_VERSION_PATH, VERSION_CHECK_TIMEOUT ) );
        httpGet->start();
    }
    else
    {
        const string file = format ( "cccaster.v%s.zip", version );
        httpDl.reset ( new HttpDownload ( this, updateServers[serverIndex] + file, appDir + UPDATE_ARCHIVE ) );
        httpDl->start();
    }
}

static bool isOnline()
{
    DWORD state;
    InternetGetConnectedState ( &state, 0 );
    return ( state & ( INTERNET_CONNECTION_LAN | INTERNET_CONNECTION_MODEM | INTERNET_CONNECTION_PROXY ) );
}

void MainUi::update ( bool isStartup )
{
    if ( !isOnline() )
    {
        sessionMessage = "No Internet connection!";
        return;
    }

    AutoManager _;

    latestVersion.clear();
    serverIndex = 0;
    updateTo ( "" );

    EventManager::get().start();

    LOG ( "latestVersion=%s", latestVersion );

    if ( latestVersion.code.empty() )
    {
        sessionMessage = "Cannot fetch latest version!";
        return;
    }

    if ( LocalVersion.similar ( latestVersion, 2 ) )
    {
        if ( !isStartup )
            sessionMessage = "Already up to date!";
        return;
    }

    ui->pushRight ( new ConsoleUi::TextBox ( format ( "Latest version is %s", latestVersion ) ) );

    if ( !confirm ( "Update?" ) )
    {
        ui->pop();
        return;
    }

    ui->pop();
    ui->pushRight ( new ConsoleUi::ProgressBar ( "Downloading...", 20 ) );

    downloadCompleted = false;
    serverIndex = 0;
    updateTo ( latestVersion.code );

    EventManager::get().start();

    if ( !downloadCompleted )
    {
        ui->pop();
        sessionMessage = "Cannot download latest version!";
        return;
    }

    ASSERT ( latestVersion.major().empty() == false );
    ASSERT ( latestVersion.minor().empty() == false );

    const string binary = format ( "cccaster.v%s.%s.exe", latestVersion.major(), latestVersion.minor() );

    // TODO actually test this
    if ( ProcessManager::isWine() )
    {
        const string command = format ( "\"%scccaster\\unzip.exe\" -o %s%s -d %s",
                                        appDir, appDir, UPDATE_ARCHIVE, appDir );

        LOG ( "Update command: %s", command );

        system ( ( "\"" + command + "\"" ).c_str() );

        system ( ( "\"" + appDir + binary + "\" &" ).c_str() );

        exit ( 0 );
        return;
    }

    const string command = format ( "\"%scccaster\\updater.exe\" %s %s %s", appDir, binary, UPDATE_ARCHIVE, appDir );

    LOG ( "Update command: %s", command );

    system ( ( "\"start \"Updating...\" " + command + "\"" ).c_str() );

    exit ( 0 );
}
