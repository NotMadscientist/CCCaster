#include "Thread.hpp"


void *Thread::func ( void *ptr )
{
    static_cast<Thread *> ( ptr )->run();
    pthread_exit ( 0 );
    return 0;
}

void Thread::start()
{
    LOCK ( _mutex );
    if ( _running )
        return;

    _running = true;

    pthread_attr_t attr;
    pthread_attr_init ( &attr );
    pthread_create ( &thread, &attr, func, this );
    pthread_attr_destroy ( &attr );
}

void Thread::join()
{
    LOCK ( _mutex );
    if ( ! _running )
        return;

    pthread_join ( thread, 0 );
    _running = false;
}

void Thread::release()
{
    LOCK ( _mutex );
    _running = false;
}
