#include "Thread.h"

void *Thread::func ( void *ptr )
{
    static_cast<Thread *> ( ptr )->run();
    pthread_exit ( 0 );
    return 0;
}

void Thread::start()
{
    LOCK ( mutex );
    if ( running )
        return;

    running = true;

    pthread_attr_t attr;
    pthread_attr_init ( &attr );
    pthread_create ( &thread, &attr, func, this );
    pthread_attr_destroy ( &attr );
}

void Thread::join()
{
    LOCK ( mutex );
    if ( !running )
        return;

    pthread_join ( thread, 0 );
    running = false;
}
