#pragma once

#include "Utilities.h"
#include "Messages.h"
#include "Statistics.h"


class MainUi
{
    ConfigSettings config;

public:

    bool mainMenu();

    bool acceptMenu ( const Statistics& stats );

    bool connectMenu ( const Statistics& stats );

    std::string getMainAddress() const;

    NetplaySetup getNetplaySetup() const;
};
