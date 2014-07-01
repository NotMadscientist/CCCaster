#include "Log.h"
#include "Event.h"
#include "Test.h"

#include <cstdlib>
#include <ctime>

using namespace std;

int main ( int argc, char *argv[] )
{
    Log::open();

    EventManager::initialize();

    int result = RunAllTests ( argc, argv );

    Log::close();

    return result;
}
