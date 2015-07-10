#pragma once

#include "Messages.hpp"
#include "IpAddrPort.hpp"
#include "Controller.hpp"
#include "ControllerManager.hpp"
#include "KeyValueStore.hpp"
#include "MainUpdater.hpp"

#include <string>
#include <memory>


// The function to run the game with the provided options
typedef void ( * RunFuncPtr ) ( const IpAddrPort& address, const Serializable& config );


// Function that computes the delay from the latency
inline int computeDelay ( double latency )
{
    return ( int ) ceil ( latency / ( 1000.0 / 60 ) );
}


class ConsoleUi;

class MainUi
    : private Controller::Owner
    , private ControllerManager::Owner
    , private MainUpdater::Owner
{
public:

    InitialConfig initialConfig;

    std::string sessionMessage;

    std::string sessionError;


    MainUi();

    void initialize();

    void main ( RunFuncPtr run );

    void display ( const std::string& message, bool replace = true );

    bool connected ( const InitialConfig& initialConfig, const PingStats& pingStats );

    void spectate ( const SpectateConfig& spectateConfig );

    bool confirm ( const std::string& question );


    void setMaxRealDelay ( uint8_t delay );

    void setDefaultRollback ( uint8_t rollback );

    const KeyValueStore& getConfig() const { return config; }

    const NetplayConfig& getNetplayConfig() const { return netplayConfig; }


    static void *getConsoleWindow();

    static std::string formatStats ( const PingStats& pingStats );

private:

    std::shared_ptr<ConsoleUi> ui;

    MainUpdater updater;

    KeyValueStore config;

    IpAddrPort address;

    NetplayConfig netplayConfig;

    Controller *currentController = 0;

    uint32_t mappedKey = 0;

    bool upToDate = false;

    void netplay ( RunFuncPtr run );
    void spectate ( RunFuncPtr run );
    void broadcast ( RunFuncPtr run );
    void offline ( RunFuncPtr run );
    void controls();
    void settings();

    bool areYouSure();

    bool gameMode ( bool below );
    bool offlineGameMode();

    void controllerKeyMapped ( Controller *controller, uint32_t key ) override;

    void joystickAttached ( Controller *controller ) override {};
    void joystickToBeDetached ( Controller *controller ) override;

    void saveConfig();
    void loadConfig();

    void saveMappings ( const Controller& controller );
    void loadMappings ( Controller& controller );

    void alertUser();

    std::string formatPlayer ( const SpectateConfig& spectateConfig, uint8_t player ) const;

    bool configure ( const PingStats& pingStats );

    void update ( bool isStartup = false );

    void openChangeLog();

    void fetch ( const MainUpdater::Type& type );

    void fetchCompleted ( MainUpdater *updater, const MainUpdater::Type& type ) override;

    void fetchFailed ( MainUpdater *updater, const MainUpdater::Type& type ) override;

    void fetchProgress ( MainUpdater *updater, const MainUpdater::Type& type, double progress ) override;
};
