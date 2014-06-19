#include "Event.h"
#include "DoubleSocket.h"
#include "Timer.h"
#include "Log.h"

#include <windows.h>
#include <gtest/gtest.h>

#include <vector>
#include <cstdlib>
#include <cstdio>
#include <cassert>

using namespace std;

int main ( int argc, char *argv[] )
{
    srand ( time ( 0 ) );
    NL::init();
    Log::open();

    testing::InitGoogleTest ( &argc, argv );
    int result = RUN_ALL_TESTS();

    Log::close();
    return result;
}
