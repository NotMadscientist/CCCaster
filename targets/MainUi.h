#pragma once

#include "Utilities.h"


class MainUi
{
    ConfigSettings config;

public:

    void start();

    const ConfigSettings& getConfig() const { return config; }
};
