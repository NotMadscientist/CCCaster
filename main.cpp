#include "Log.h"
#include "Test.h"

#include <netlink/socket.h>

#include <cstdlib>
#include <ctime>

using namespace std;

int main ( int argc, char *argv[] )
{
    srand ( time ( 0 ) );
    NL::init();
    Log::open();

    int result = RunAllTests ( argc, argv );
    LOG ( "Finished all tests" );

    Log::close();
    return result;
}
