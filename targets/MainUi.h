#pragma once

#include "Utilities.h"
#include "Messages.h"
#include "Statistics.h"
#include "ConsoleUi.h"

#include <string>


class MainUi
{
    ConfigSettings config;

    NetplaySetup setup;

    ConsoleUi ui;

public:

    std::string message;

    std::string error;

    MainUi();

    void initialize();

    bool mainMenu();

    bool acceptMenu ( const Statistics& stats );

    bool connectMenu ( const Statistics& stats );

    std::string getMainAddress() const;

    NetplaySetup getNetplaySetup() const;
};
