#pragma once

#include "Utilities.h"
#include "Messages.h"
#include "Statistics.h"

#include <string>


// The function to run the game with the provided connection address or netplay setup
typedef void ( * RunFuncPtr ) ( const std::string& address, const NetplaySetup& netplaySetup );


class MainUi
{
    ConfigSettings config;

    NetplaySetup netplaySetup;

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

    bool accepted ( const Statistics& stats );

    bool connected ( const Statistics& stats );

    const NetplaySetup& getNetplaySetup() const { return netplaySetup; }
};
