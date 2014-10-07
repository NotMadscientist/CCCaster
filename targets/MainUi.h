#pragma once

#include "Utilities.h"
#include "Messages.h"
#include "IpAddrPort.h"

#include <string>
#include <memory>


// The function to run the game with the provided connection address or config
typedef void ( * RunFuncPtr ) ( const IpAddrPort& address, const Serializable& config );

class ConsoleUi;


class MainUi
{
    std::shared_ptr<ConsoleUi> ui;

    ConfigSettings config;

    IpAddrPort address;

    NetplayConfig netplayConfig;

    void netplay ( RunFuncPtr run );
    void spectate ( RunFuncPtr run );
    void broadcast ( RunFuncPtr run );
    void offline ( RunFuncPtr run );
    void controls();
    void settings();

public:

    InitialConfig initialConfig;

    std::string sessionMessage;

    std::string sessionError;

    MainUi();

    void main ( RunFuncPtr run );

    void display ( const std::string& message );

    bool accepted ( const InitialConfig& initialConfig, const PingStats& pingStats );

    bool connected ( const InitialConfig& initialConfig, const PingStats& pingStats );

    const ConfigSettings& getConfig() const { return config; }

    const NetplayConfig& getNetplayConfig() const { return netplayConfig; }

    const void *getConsoleWindow() const;
};
