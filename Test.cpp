#include "Test.h"
#include "Log.h"
#include "Event.h"

#include <gtest/gtest.h>

using namespace std;

int RunAllTests ( int& argc, char *argv[] )
{
    testing::InitGoogleTest ( &argc, argv );

    int result = RUN_ALL_TESTS();

    LOG ( "Finished all tests" );

    return result;
}
