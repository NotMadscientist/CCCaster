#pragma once

#include "Utilities.h"
#include "Messages.h"
#include "IpAddrPort.h"

#include <string>
#include <memory>


// The function to run the game with the provided connection address or netplay setup
typedef void ( * RunFuncPtr ) ( const IpAddrPort& address, const NetplayConfig& netplayConfig );

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

    std::string sessionMessage;

    std::string sessionError;

    void main ( RunFuncPtr run );

    bool accepted ( const InitialConfig& initialConfig );

    bool connected ( const InitialConfig& initialConfig );

    const NetplayConfig& getNetplayConfig() const { return netplayConfig; }
};
