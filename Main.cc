#include "Log.h"
#include "Event.h"
#include "Test.h"

using namespace std;

int main ( int argc, char *argv[] )
{
    Log::get().initialize();

    EventManager::get().initialize();

    int result = RunAllTests ( argc, argv );

    EventManager::get().deinitialize();

    Log::get().deinitialize();

    return result;
}
