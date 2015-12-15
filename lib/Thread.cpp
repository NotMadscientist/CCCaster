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

    pthread_create ( &_thread, 0, func, this );
}

void Thread::join()
{
    LOCK ( _mutex );
    if ( ! _running )
        return;

    pthread_join ( _thread, 0 );
    _running = false;
}

void Thread::release()
{
    LOCK ( _mutex );
    _running = false;
}
