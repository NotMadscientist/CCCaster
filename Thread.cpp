#include "Thread.h"

void *Thread::func ( void *ptr )
{
    ( ( Thread * ) ptr )->run();
    pthread_exit ( 0 );
    return 0;
}
