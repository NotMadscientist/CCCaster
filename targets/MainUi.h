#pragma once

#include "Utilities.h"
#include "Messages.h"
#include "Statistics.h"

#include <string>
#include <memory>


class ConsoleUi;

class MainUi
{
    ConfigSettings config;

    NetplaySetup setup;

    std::shared_ptr<ConsoleUi> ui;

public:

    std::string message;

    std::string error;

    void initialize();

    bool mainMenu();

    bool acceptMenu ( const Statistics& stats );

    bool connectMenu ( const Statistics& stats );

    std::string getMainAddress() const;

    NetplaySetup getNetplaySetup() const;
};
