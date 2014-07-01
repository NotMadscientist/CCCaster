#include "Log.h"
#include "Test.h"

#include <cstdlib>
#include <ctime>

using namespace std;

int main ( int argc, char *argv[] )
{
    Log::open();

    int result = RunAllTests ( argc, argv );

    Log::close();

    return result;
}
