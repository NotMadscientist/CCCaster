#pragma once

#include "Messages.h"
#include "IpAddrPort.h"
#include "Controller.h"
#include "KeyValueStore.h"

#include <string>
#include <memory>


// The function to run the game with the provided options
typedef void ( * RunFuncPtr ) ( const IpAddrPort& address, const Serializable& config );

class ConsoleUi;


class MainUi : public Controller::Owner
{
    std::shared_ptr<ConsoleUi> ui;

    KeyValueStore config;

    IpAddrPort address;

    NetplayConfig netplayConfig;

    uint32_t mappedKey = 0;

    void netplay ( RunFuncPtr run );
    void spectate ( RunFuncPtr run );
    void broadcast ( RunFuncPtr run );
    void offline ( RunFuncPtr run );
    void controls();
    void settings();

    bool gameMode();

    void doneMapping ( Controller *controller, uint32_t key );

    void saveConfig();
    void loadConfig();

    void saveMappings ( const Controller& controller );
    void loadMappings ( Controller& controller );

    void alertUser();

public:

    InitialConfig initialConfig;

    std::string sessionMessage;

    std::string sessionError;

    void initialize();

    void main ( RunFuncPtr run );

    void display ( const std::string& message, bool replace = true );

    bool accepted ( const InitialConfig& initialConfig, const PingStats& pingStats );

    void connected ( const InitialConfig& initialConfig, const PingStats& pingStats );

    void connected ( const NetplayConfig& netplayConfig );

    void spectate ( const SpectateConfig& spectateConfig );

    bool confirm();

    const KeyValueStore& getConfig() const { return config; }

    const NetplayConfig& getNetplayConfig() const { return netplayConfig; }

    static const void *getConsoleWindow();
};
